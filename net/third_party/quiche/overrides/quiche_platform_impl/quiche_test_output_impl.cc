// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quiche_test_output_impl.h"

#include <stdlib.h>
#include <time.h>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quiche {

void QuicheRecordTestOutputToFile(std::string_view filename,
                                  std::string_view data) {
  std::string output_dir;
  if (!base::Environment::Create()->GetVar("QUIC_TEST_OUTPUT_DIR",
                                           &output_dir) ||
      output_dir.empty()) {
    return;
  }

  auto path = base::FilePath::FromUTF8Unsafe(output_dir)
                  .Append(base::FilePath::FromUTF8Unsafe(filename));

  if (!base::WriteFile(path, base::as_byte_span(data))) {
    QUIC_LOG(WARNING) << "Failed to write into " << path;
    return;
  }
  QUIC_LOG(INFO) << "Recorded test output into " << path;
}

void QuicheSaveTestOutputImpl(std::string_view filename,
                              std::string_view data) {
  QuicheRecordTestOutputToFile(filename, data);
}

bool QuicheLoadTestOutputImpl(std::string_view filename, std::string* data) {
  std::string output_dir;
  if (!base::Environment::Create()->GetVar("QUIC_TEST_OUTPUT_DIR",
                                           &output_dir) ||
      output_dir.empty()) {
    QUIC_LOG(WARNING) << "Failed to load " << filename
                      << " because QUIC_TEST_OUTPUT_DIR is not set";
    return false;
  }

  auto path = base::FilePath::FromUTF8Unsafe(output_dir)
                  .Append(base::FilePath::FromUTF8Unsafe(filename));

  return base::ReadFileToString(path, data);
}

void QuicheRecordTraceImpl(std::string_view identifier, std::string_view data) {
  const testing::TestInfo* test_info =
      testing::UnitTest::GetInstance()->current_test_info();

  // TODO(vasilvv): replace this with absl::Time once it's usable in Chromium.
  time_t now_ts = time(nullptr);
  tm now;
#if BUILDFLAG(IS_WIN)
  gmtime_s(&now, &now_ts);
#else
  gmtime_r(&now_ts, &now);
#endif

  char timestamp[2048];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &now);

  std::string filename = base::StringPrintf(
      "%s.%s.%s.%s.qtr", test_info->name(), test_info->test_suite_name(),
      identifier.data(), timestamp);

  QuicheRecordTestOutputToFile(filename, data);
}

}  // namespace quiche
