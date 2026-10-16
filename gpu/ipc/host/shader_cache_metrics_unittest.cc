// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/shader_cache_metrics.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

constexpr char kHistogram[] = "GPU.ShaderCache.DiskSize.GlShaders";

TEST(ShaderCacheMetricsTest, RecordsDirectorySizeInKB) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath cache_dir = temp_dir.GetPath().AppendASCII("GPUCache");
  const base::FilePath sub_dir = cache_dir.AppendASCII("index-dir");
  ASSERT_TRUE(base::CreateDirectory(sub_dir));
  ASSERT_TRUE(base::WriteFile(cache_dir.AppendASCII("data"),
                              std::string(10 * 1024, 'x')));
  ASSERT_TRUE(base::WriteFile(sub_dir.AppendASCII("index"),
                              std::string(20 * 1024, 'x')));

  base::HistogramTester histogram_tester;
  RecordShaderCacheDiskSize("GlShaders", cache_dir);

  // Files in subdirectories are included.
  histogram_tester.ExpectUniqueSample(kHistogram, 30, 1);
}

TEST(ShaderCacheMetricsTest, DoesNotRecordMissingDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::HistogramTester histogram_tester;
  RecordShaderCacheDiskSize("GlShaders",
                            temp_dir.GetPath().AppendASCII("GPUCache"));

  histogram_tester.ExpectTotalCount(kHistogram, 0);
}

}  // namespace
}  // namespace gpu
