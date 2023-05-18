// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

#include <unistd.h>

#include <atomic>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/base/breakpad_utils_linux.h"
#include "remoting/base/version.h"
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"

namespace remoting {

namespace {

class BreakpadLinux {
 public:
  BreakpadLinux();

  BreakpadLinux(const BreakpadLinux&) = delete;
  BreakpadLinux& operator=(const BreakpadLinux&) = delete;

  ~BreakpadLinux() = delete;

  static BreakpadLinux& GetInstance();

  std::atomic<bool>& handling_exception() { return handling_exception_; }
  base::Time process_start_time() const { return process_start_time_; }
  int process_id() const { return pid_; }
  const std::string& program_name() const { return program_name_; }

 private:
  // Breakpad's exception handler.
  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_;

  // Indicates whether an exception is already being handled.
  std::atomic<bool> handling_exception_{false};

  base::Time process_start_time_ = base::Time::NowFromSystemTime();
  pid_t pid_ = getpid();
  std::string program_name_;
};

bool MinidumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                      void* context,
                      bool succeeded) {
  BreakpadLinux& self = BreakpadLinux::GetInstance();
  if (self.handling_exception().exchange(true)) {
    LOG(WARNING) << "Already processing another crash";
    return false;
  }

  auto process_uptime =
      base::Time::NowFromSystemTime() - self.process_start_time();
  auto metadata =
      base::Value::Dict()
          .Set(kBreakpadProcessIdKey, self.process_id())
          .Set(kBreakpadProcessNameKey, self.program_name())
          .Set(kBreakpadProcessStartTimeKey,
               base::NumberToString(self.process_start_time().ToTimeT()))
          .Set(kBreakpadProcessUptimeKey,
               base::NumberToString(process_uptime.InMilliseconds()))
          .Set(kBreakpadHostVersionKey, REMOTING_VERSION_STRING);

  auto metadata_file_contents = base::WriteJson(metadata);
  if (!metadata_file_contents.has_value()) {
    LOG(ERROR) << "Failed to convert metadata to JSON.";
    self.handling_exception().exchange(false);
    return false;
  }

  ScopedAllowBlockingForCrashReporting scoped_allow_blocking;
  auto temp_metadata_file_path =
      base::FilePath(descriptor.path()).ReplaceExtension("temp");
  if (!base::WriteFile(temp_metadata_file_path, *metadata_file_contents)) {
    LOG(ERROR) << "Failed to write crash dump metadata to temp file.";
    self.handling_exception().exchange(false);
    return false;
  }

  auto metadata_file_path = temp_metadata_file_path.ReplaceExtension("json");
  if (!base::Move(temp_metadata_file_path, metadata_file_path)) {
    LOG(ERROR) << "Failed to rename temp metadata file.";
    self.handling_exception().exchange(false);
    return false;
  }

  self.handling_exception().exchange(false);
  return succeeded;
}

BreakpadLinux::BreakpadLinux() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  // This includes both executable name and the path, e.g.
  // /opt/google/chrome-remote-desktop/chrome-remote-desktop-host
  program_name_ = cmd_line->GetProgram().value();

  if (!base::CreateDirectory(base::FilePath(kMinidumpPath))) {
    LOG(ERROR) << "Failed to create minidump directory: " << kMinidumpPath;
  }

  google_breakpad::MinidumpDescriptor descriptor(kMinidumpPath);
  breakpad_ = std::make_unique<google_breakpad::ExceptionHandler>(
      descriptor, /*filter_callback=*/nullptr, MinidumpCallback,
      /*callback_context=*/nullptr,
      /*install_handler=*/true,
      /*server_fd=*/-1);
}

// static
BreakpadLinux& BreakpadLinux::GetInstance() {
  static base::NoDestructor<BreakpadLinux> instance;
  return *instance;
}

}  // namespace

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  BreakpadLinux::GetInstance();
}

}  // namespace remoting
