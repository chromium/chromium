// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/frame_generator/headless_frame_generator.h"

#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "remoting/test/frame_generator/file_frame_generator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kChromeBinaryName[] =
    FILE_PATH_LITERAL("chrome.exe");
#elif BUILDFLAG(IS_MAC)
constexpr base::FilePath::CharType kChromeBinaryName[] =
    FILE_PATH_LITERAL("Google Chrome.app/Contents/MacOS/Google Chrome");
#else
constexpr base::FilePath::CharType kChromeBinaryName[] =
    FILE_PATH_LITERAL("chrome");
#endif

}  // namespace

HeadlessFrameGenerator::HeadlessFrameGenerator(const std::string& scenario,
                                               const webrtc::DesktopSize& size,
                                               int frame_count,
                                               double fps)
    : scenario_(scenario), size_(size), frame_count_(frame_count), fps_(fps) {}

HeadlessFrameGenerator::~HeadlessFrameGenerator() = default;

std::unique_ptr<webrtc::DesktopFrame> HeadlessFrameGenerator::GenerateFrame() {
  CHECK(file_generator_)
      << "Initialize() must be called before GenerateFrame()";
  return file_generator_->GenerateFrame();
}

void HeadlessFrameGenerator::SetChromePath(const base::FilePath& chrome_path) {
  chrome_path_ = chrome_path;
}

bool HeadlessFrameGenerator::Initialize() {
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create temporary directory.";
    return false;
  }

  base::FilePath script_path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &script_path)) {
    LOG(ERROR) << "Failed to get source root directory.";
    return false;
  }
  script_path = script_path.AppendASCII("remoting")
                    .AppendASCII("test")
                    .AppendASCII("frame_generator")
                    .AppendASCII("frame_generator.py");

  base::FilePath chrome_path = chrome_path_;
  if (chrome_path.empty()) {
    base::FilePath exe_dir;
    if (base::PathService::Get(base::DIR_EXE, &exe_dir)) {
      base::FilePath candidate = exe_dir.Append(kChromeBinaryName);
      if (base::PathExists(candidate)) {
        chrome_path = candidate;
      }
    }
  }

  // base::CommandLine is used here to ensure platform-specific quoting and
  // escaping is handled correctly, which is especially important on Windows.
  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("vpython3")));
  cmd.AppendArgPath(script_path);

  // We use AppendArg instead of AppendSwitch because base::CommandLine places
  // switches before arguments. Since the script path is an argument, using
  // AppendSwitch would place the script options before the script path, causing
  // the python interpreter to consume them instead of passing them to the
  // script.
  cmd.AppendArg("--scenario=" + scenario_);
  cmd.AppendArg("--width=" + base::NumberToString(size_.width()));
  cmd.AppendArg("--height=" + base::NumberToString(size_.height()));
  cmd.AppendArg("--frames=" + base::NumberToString(frame_count_));
  cmd.AppendArg("--fps=" + base::NumberToString(fps_));
  cmd.AppendArg("--out-dir=" + temp_dir_.GetPath().AsUTF8Unsafe());
  if (!chrome_path.empty()) {
    cmd.AppendArg("--chrome-path=" + chrome_path.AsUTF8Unsafe());
  }

  VLOG(1) << "Launching frame generator: " << cmd.GetCommandLineString();

  base::LaunchOptions options;
  base::Process process = base::LaunchProcess(cmd, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch frame generator process.";
    return false;
  }

  // This call blocks until all frames have been generated, which can take
  // a significant amount of time depending on the frame count and scenario.
  int exit_code;
  if (!process.WaitForExit(&exit_code) || exit_code != 0) {
    LOG(ERROR) << "Frame generator process failed with exit code " << exit_code;
    return false;
  }

  file_generator_ =
      std::make_unique<FileFrameGenerator>(temp_dir_.GetPath(), size_);
  return true;
}

}  // namespace remoting
