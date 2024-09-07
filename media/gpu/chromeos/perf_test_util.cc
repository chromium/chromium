// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/chromeos/perf_test_util.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

media::test::VideoTestEnvironment* g_env;
base::FilePath g_output_directory =
    base::FilePath(base::FilePath::kCurrentDirectory);
base::FilePath g_source_directory =
    base::FilePath(base::FilePath::kCurrentDirectory);

void WriteJsonResult(std::vector<std::pair<std::string, double>> data) {
  base::Value::Dict metrics;
  for (auto i : data) {
    metrics.Set(i.first, i.second);
  }

  const auto output_folder_path = base::FilePath(g_output_directory);
  std::string metrics_str;
  ASSERT_TRUE(base::JSONWriter::WriteWithOptions(
      metrics, base::JSONWriter::OPTIONS_PRETTY_PRINT, &metrics_str));
  const base::FilePath metrics_file_path = output_folder_path.Append(
      g_env->GetTestOutputFilePath().AddExtension(FILE_PATH_LITERAL(".json")));
  // Make sure that the directory into which json is saved is created.
  LOG_ASSERT(base::CreateDirectory(metrics_file_path.DirName()));
  base::File metrics_output_file(
      base::FilePath(metrics_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const int bytes_written = metrics_output_file.WriteAtCurrentPos(
      metrics_str.data(), metrics_str.length());
  ASSERT_EQ(bytes_written, static_cast<int>(metrics_str.length()));
  LOG(INFO) << "Wrote performance metrics to: " << metrics_file_path;
}

}  // namespace media
