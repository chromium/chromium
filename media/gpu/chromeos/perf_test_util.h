// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_PERF_TEST_UTIL_H_
#define MEDIA_GPU_CHROMEOS_PERF_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "media/gpu/test/video_test_environment.h"

namespace media {

extern media::test::VideoTestEnvironment* g_env;
// Default output folder used to store performance metrics.
extern base::FilePath g_output_directory;
extern base::FilePath g_source_directory;

void WriteJsonResult(std::vector<std::pair<std::string, double>> data);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_PERF_TEST_UTIL_H_
