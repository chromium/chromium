// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_BREAKPAD_UTILS_H_
#define REMOTING_BASE_BREAKPAD_UTILS_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace remoting {

// base::Value keys used in multiple crash components.
extern const char kBreakpadProductVersionKey[];
extern const char kBreakpadProcessStartTimeKey[];
extern const char kBreakpadProcessIdKey[];
extern const char kBreakpadProcessNameKey[];
extern const char kBreakpadProcessUptimeKey[];

// Returns the path to the directory to use when generating or processing
// minidumps. Does not attempt to create the directory or check R/W permissions.
extern base::FilePath GetMinidumpDirectoryPath();

extern bool CreateMinidumpDirectoryIfNeeded(
    const base::FilePath& minidump_directory);

extern bool WriteMetadataForMinidump(const base::FilePath& minidump_file_path,
                                     base::Value::Dict custom_client_info);

#if BUILDFLAG(IS_WIN)

// The name of the pipe to use for OOP crash reporting.
extern const wchar_t kCrashServerPipeName[];

base::win::ScopedHandle GetClientHandleForCrashServerPipe();
#endif  // BUILDFLAG(IS_WIN)

// Helper for generating and uploading minidumps.
class BreakpadHelper {
 public:
  BreakpadHelper();

  BreakpadHelper(const BreakpadHelper&) = delete;
  BreakpadHelper& operator=(const BreakpadHelper&) = delete;

  ~BreakpadHelper();

  // Configures the instance for handling minidumps, must be called before
  // |OnMinidumpGenerated|.
  bool Initialize(const base::FilePath& minidump_directory);

  // Called in the Breakpad client's filter callback, this function is used to
  // ensure multiple threads do not report an exception concurrently.
  void OnException();

  // Prepares a newly generated minidump for upload. Will block when writing
  // the upload metadata file.
  bool OnMinidumpGenerated(const base::FilePath& minidump_file_path);

 private:
  // Indicates whether an exception is already being handled.
  std::atomic<bool> handling_exception_{false};

  // Indicates whether the instance is ready to begin handling minidumps.
  bool initialized_ = false;

  // ID of the current process.
  base::ProcessId process_id_ = base::GetCurrentProcId();
  // Path and name of the executable for the current process.
  base::FilePath process_name_;
  // The BreakpadHelper singleton is created soon after the process has started,
  // this means the time the singleton was created can be used as a cheap
  // approximation for the time the process actually started.
  base::Time process_start_time_ = base::Time::NowFromSystemTime();
};

}  // namespace remoting

#endif  // REMOTING_BASE_BREAKPAD_UTILS_H_
