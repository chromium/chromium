// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histogram_shared_memory_config.h"
#include "content/public/common/process_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using Config = base::HistogramSharedMemoryConfig;

struct ProcessTypeToOptionalConfig {
  int process_type;
  absl::optional<base::HistogramSharedMemoryConfig> expected;
};

using HistogramSharedMemoryConfigTest =
    testing::TestWithParam<ProcessTypeToOptionalConfig>;

}  // namespace

TEST_P(HistogramSharedMemoryConfigTest, GetHistogramSharedMemoryConfig) {
  const auto& process_type = GetParam().process_type;
  const auto& expected = GetParam().expected;

  const auto config = GetHistogramSharedMemoryConfig(process_type);
  ASSERT_EQ(config.has_value(), expected.has_value());
  if (config.has_value()) {
    EXPECT_EQ(config->allocator_name, expected->allocator_name);
    EXPECT_EQ(config->memory_size_bytes, expected->memory_size_bytes);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HistogramSharedMemoryConfigTest,
    testing::ValuesIn(std::vector<ProcessTypeToOptionalConfig>({
        {PROCESS_TYPE_UNKNOWN, absl::nullopt},
        {PROCESS_TYPE_BROWSER, absl::nullopt},
        {PROCESS_TYPE_RENDERER, Config{"RendererMetrics", 2 << 20}},
        {PROCESS_TYPE_PLUGIN_DEPRECATED, absl::nullopt},
        {PROCESS_TYPE_WORKER_DEPRECATED, absl::nullopt},
        {PROCESS_TYPE_UTILITY, Config{"UtilityMetrics", 256 << 10}},
        {PROCESS_TYPE_ZYGOTE, Config{"ZygoteMetrics", 64 << 10}},
        {PROCESS_TYPE_SANDBOX_HELPER, Config{"SandboxHelperMetrics", 64 << 10}},
        {PROCESS_TYPE_GPU, Config{"GpuMetrics", 256 << 10}},
        {PROCESS_TYPE_PPAPI_PLUGIN, Config{"PpapiPluginMetrics", 64 << 10}},
        {PROCESS_TYPE_PPAPI_BROKER, Config{"PpapiBrokerMetrics", 64 << 10}},
    })));

}  // namespace content
