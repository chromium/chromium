// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad_utils.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/version.h"

namespace remoting {

// This class is allowlisted in thread_restrictions.h.
class ScopedAllowBlockingForCrashReporting : public base::ScopedAllowBlocking {
};

namespace {

const base::BasePathKey kBasePathKey =
#if BUILDFLAG(IS_WIN)
    // We can't use %TEMP% for Windows because our processes run as SYSTEM,
    // Local Service, and the user. SYSTEM processes will write to %WINDIR%\Temp
    // which we don't want to do and if a crash occurs before login, there isn't
    // a user temp env var to query. Because of these issues, we store the crash
    // dumps, which are usually < 100KB, in the install folder so they will get
    // cleaned up when the user uninstalls or we push an update.
    base::BasePathKey::FILE_MODULE;
#else
    base::BasePathKey::DIR_TEMP;
#endif

const base::FilePath::CharType kMinidumpsPath[] =
#if BUILDFLAG(IS_WIN)
    FILE_PATH_LITERAL("minidumps");
#else
    FILE_PATH_LITERAL("chromoting/minidumps");
#endif

const base::FilePath::CharType kTempExtension[] = FILE_PATH_LITERAL("temp");
const base::FilePath::CharType kJsonExtension[] = FILE_PATH_LITERAL("json");
const char kBreakpadProcessIdKey[] = "process_id";
const char kBreakpadProcessNameKey[] = "process_name";
const char kBreakpadProcessStartTimeKey[] = "process_start_time";
}  // namespace

const char kBreakpadProcessUptimeKey[] = "process_uptime";
const char kBreakpadHostVersionKey[] = "host_version";

base::FilePath GetMinidumpDirectoryPath() {
  base::FilePath base_path;
  if (base::PathService::Get(kBasePathKey, &base_path)) {
    // |base_path| will contain a file for the Windows case so remove it before
    // appending the fragments.
    return base_path.DirName().Append(kMinidumpsPath);
  }

  LOG(ERROR) << "Failed to retrieve a directory for crash reporting.";
  return base::FilePath();
}

BreakpadHelper::BreakpadHelper() = default;
BreakpadHelper::~BreakpadHelper() = default;

bool BreakpadHelper::Initialize(const base::FilePath& minidump_directory) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  // This includes both executable name and the path such as:
  // /opt/google/chrome-remote-desktop/chrome-remote-desktop-host
  //   or
  // C:\\Program Files (x86)\\Chromoting\\127.0.6498.0\\remoting_host.exe
  process_name_ = cmd_line->GetProgram();

  if (!base::DirectoryExists(minidump_directory) &&
      !base::CreateDirectory(minidump_directory)) {
    LOG(ERROR) << "Failed to create minidump directory: " << minidump_directory;
    return false;
  }

  initialized_ = true;
  return initialized_;
}

bool BreakpadHelper::OnMinidumpGenerated(
    const base::FilePath& minidump_file_path) {
  CHECK(initialized_);

  if (handling_exception_.exchange(true)) {
    LOG(WARNING) << "Already processing another crash";
    return false;
  }

  auto process_uptime = base::Time::NowFromSystemTime() - process_start_time_;
  auto metadata =
      base::Value::Dict()
          .Set(kBreakpadProcessIdKey, static_cast<int>(process_id_))
          .Set(kBreakpadProcessNameKey, process_name_.MaybeAsASCII())
          .Set(kBreakpadProcessStartTimeKey,
               base::NumberToString(process_start_time_.ToTimeT()))
          .Set(kBreakpadProcessUptimeKey,
               base::NumberToString(process_uptime.InMilliseconds()))
          .Set(kBreakpadHostVersionKey, REMOTING_VERSION_STRING);

  auto metadata_file_contents = base::WriteJson(metadata);
  if (!metadata_file_contents.has_value()) {
    LOG(ERROR) << "Failed to convert metadata to JSON.";
    handling_exception_.exchange(false);
    return false;
  }

  ScopedAllowBlockingForCrashReporting scoped_allow_blocking;
  auto temp_metadata_file_path =
      base::FilePath(minidump_file_path).ReplaceExtension(kTempExtension);
  if (!base::WriteFile(temp_metadata_file_path, *metadata_file_contents)) {
    LOG(ERROR) << "Failed to write crash dump metadata to temp file.";
    handling_exception_.exchange(false);
    return false;
  }

  auto metadata_file_path =
      temp_metadata_file_path.ReplaceExtension(kJsonExtension);
  if (!base::Move(temp_metadata_file_path, metadata_file_path)) {
    LOG(ERROR) << "Failed to rename temp metadata file.";
    handling_exception_.exchange(false);
    return false;
  }

  handling_exception_.exchange(false);
  return true;
}

}  // namespace remoting
