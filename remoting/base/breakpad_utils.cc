// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad_utils.h"

#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/version.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

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
    base::BasePathKey::DIR_ASSETS;
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

}  // namespace

const char kBreakpadProductVersionKey[] = "product_version";
const char kBreakpadProcessStartTimeKey[] = "process_start_time";
const char kBreakpadProcessIdKey[] = "process_id";
const char kBreakpadProcessNameKey[] = "process_name";
const char kBreakpadProcessUptimeKey[] = "process_uptime";

#if BUILDFLAG(IS_WIN)

const wchar_t kCrashServerPipeName[] =
    L"\\\\.\\pipe\\RemotingCrashService\\S-1-5-18";

base::win::ScopedHandle GetClientHandleForCrashServerPipe() {
  const ACCESS_MASK kPipeAccessMask = FILE_READ_ATTRIBUTES | FILE_READ_DATA |
                                      FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
                                      SYNCHRONIZE;
  const DWORD kPipeFlagsAndAttributes =
      SECURITY_IDENTIFICATION | SECURITY_SQOS_PRESENT;

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.bInheritHandle = true;

  base::win::ScopedHandle handle(
      CreateFile(kCrashServerPipeName, kPipeAccessMask,
                 /*dwShareMode=*/0, &security_attributes, OPEN_EXISTING,
                 kPipeFlagsAndAttributes,
                 /*hTemplateFile=*/nullptr));
  if (!handle.get()) {
    PLOG(ERROR) << "Failed to open named pipe to crash server.";
  }

  return handle;
}

#endif  // BUILDFLAG(IS_WIN)

base::FilePath GetMinidumpDirectoryPath() {
  base::FilePath base_path;
  if (base::PathService::Get(kBasePathKey, &base_path)) {
    return base_path.Append(kMinidumpsPath);
  }

  LOG(ERROR) << "Failed to retrieve a directory for crash reporting.";
  return base::FilePath();
}

bool CreateMinidumpDirectoryIfNeeded(const base::FilePath& minidump_directory) {
  if (!base::DirectoryExists(minidump_directory) &&
      !base::CreateDirectory(minidump_directory)) {
    LOG(ERROR) << "Failed to create minidump directory: " << minidump_directory;
    return false;
  }
  return true;
}

bool WriteMetadataForMinidump(const base::FilePath& minidump_file_path,
                              base::Value::Dict metadata) {
  auto metadata_file_contents = base::WriteJson(metadata);
  if (!metadata_file_contents.has_value()) {
    LOG(ERROR) << "Failed to convert metadata to JSON.";
    return false;
  }

  ScopedAllowBlockingForCrashReporting scoped_allow_blocking;
  auto temp_metadata_file_path =
      base::FilePath(minidump_file_path).ReplaceExtension(kTempExtension);
  if (!base::WriteFile(temp_metadata_file_path, *metadata_file_contents)) {
    LOG(ERROR) << "Failed to write crash dump metadata to temp file.";
    return false;
  }

  auto metadata_file_path =
      temp_metadata_file_path.ReplaceExtension(kJsonExtension);
  if (!base::Move(temp_metadata_file_path, metadata_file_path)) {
    LOG(ERROR) << "Failed to rename temp metadata file.";
    return false;
  }

  return true;
}

BreakpadHelper::BreakpadHelper() = default;
BreakpadHelper::~BreakpadHelper() = default;

bool BreakpadHelper::Initialize(const base::FilePath& minidump_directory) {
  CHECK(!initialized_);

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  // This includes both executable name and the path such as:
  // /opt/google/chrome-remote-desktop/chrome-remote-desktop-host
  //   or
  // C:\\Program Files (x86)\\Chromoting\\127.0.6498.0\\remoting_host.exe
  process_name_ = cmd_line->GetProgram();

  if (!CreateMinidumpDirectoryIfNeeded(minidump_directory)) {
    return false;
  }

  initialized_ = true;
  return initialized_;
}

void BreakpadHelper::OnException() {
  // Shared by in-proc and out-of-proc exception handlers.
  if (handling_exception_.exchange(true)) {
    base::PlatformThread::Sleep(base::TimeDelta::Max());
  }
}

bool BreakpadHelper::OnMinidumpGenerated(
    const base::FilePath& minidump_file_path) {
  CHECK(initialized_);

  if (!handling_exception_.exchange(true)) {
    // Log a warning that the caller should be using OnException() to indicate
    // when exception processing has started. Don't block so the current
    // exception is handled.
    LOG(WARNING) << "OnException() not called in filter callback.";
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
          .Set(kBreakpadProductVersionKey, REMOTING_VERSION_STRING);

  bool metadata_written =
      WriteMetadataForMinidump(minidump_file_path, std::move(metadata));
  handling_exception_.exchange(false);
  return metadata_written;
}

}  // namespace remoting
