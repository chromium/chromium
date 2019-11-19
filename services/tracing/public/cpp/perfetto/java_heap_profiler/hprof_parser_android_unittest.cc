// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_parser_android.h"
#include "base/android/java_heap_dump_generator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(HprofParserTest, ValidateEmptyParser) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    VLOG(0) << "Failed to create unique temporary directory.";
    return;
  }
  const std::string file_path_str =
      temp_dir.GetPath().Append("temp_hprof.hprof").value();

  base::android::WriteJavaHeapDumpToPath(file_path_str);
  HprofParser parser(file_path_str);
  parser.Parse();
  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::PARSE_SUCCESS);
}

TEST(HprofParserTest, InvalidPathWithNoDump) {
  HprofParser parser("invalid_file");
  parser.Parse();
  EXPECT_EQ(parser.parse_stats().result,
            HprofParser::ParseResult::FAILED_TO_OPEN_FILE);
}

}  // namespace tracing
