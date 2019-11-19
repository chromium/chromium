// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_test_output_impl.h"

#include <stdlib.h>
#include <time.h>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_str_cat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {

void QuicRecordTestOutputToFile(QuicStringPiece filename,
                                QuicStringPiece data) {
  std::string output_dir;
  if (!base::Environment::Create()->GetVar("QUIC_TEST_OUTPUT_DIR",
                                           &output_dir) ||
      output_dir.empty()) {
    return;
  }

  auto path = base::FilePath::FromUTF8Unsafe(output_dir)
                  .Append(base::FilePath::FromUTF8Unsafe(filename));

  int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written < 0) {
    QUIC_LOG(WARNING) << "Failed to write into " << path;
    return;
  }
  QUIC_LOG(INFO) << "Recorded test output into " << path;
}

void QuicRecordTestOutputImpl(QuicStringPiece identifier,
                              QuicStringPiece data) {
  const testing::TestInfo* test_info =
      testing::UnitTest::GetInstance()->current_test_info();

  // TODO(vasilvv): replace this with absl::Time once it's usable in Chromium.
  time_t now_ts = time(nullptr);
  tm now;
#ifdef OS_WIN
  gmtime_s(&now, &now_ts);
#else
  gmtime_r(&now_ts, &now);
#endif

  char timestamp[2048];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &now);

  std::string filename = QuicStringPrintf("%s.%s.%s.%s.qtr", test_info->name(),
                                          test_info->test_case_name(),
                                          identifier.data(), timestamp);

  QuicRecordTestOutputToFile(filename, data);
}

}  // namespace quic
