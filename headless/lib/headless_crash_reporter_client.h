// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_
#define HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace headless {

class HeadlessCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  HeadlessCrashReporterClient();
  ~HeadlessCrashReporterClient() override;

  void set_crash_dumps_dir(const base::FilePath& dir) {
    crash_dumps_dir_ = dir;
  }
  const base::FilePath& crash_dumps_dir() const { return crash_dumps_dir_; }

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;

  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;

  base::FilePath GetReporterLogFilename() override;
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_WIN)
  bool GetCrashDumpLocation(base::string16* crash_dir) override;
#else
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  base::FilePath crash_dumps_dir_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessCrashReporterClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_
