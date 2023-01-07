// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_
#define HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace headless {

class HeadlessCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  HeadlessCrashReporterClient();

  HeadlessCrashReporterClient(const HeadlessCrashReporterClient&) = delete;
  HeadlessCrashReporterClient& operator=(const HeadlessCrashReporterClient&) =
      delete;

  ~HeadlessCrashReporterClient() override;

  void set_crash_dumps_dir(const base::FilePath& dir) {
    crash_dumps_dir_ = dir;
  }
  const base::FilePath& crash_dumps_dir() const { return crash_dumps_dir_; }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
#else
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

 private:
  base::FilePath crash_dumps_dir_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CRASH_REPORTER_CLIENT_H_
