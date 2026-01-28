// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

class ApproximatedDeviceMemoryTest : public testing::Test {};

TEST_F(ApproximatedDeviceMemoryTest, GetApproximatedDeviceMemoryOld) {
  // Test with old limits. See https://crbug.com/454354290
  // TODO: remove this whole section when feature flag is retired.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kUpdatedDeviceMemoryLimitsFor2026);

  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(128);  // 128MB
  EXPECT_EQ(0.25, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(256);  // 256MB
  EXPECT_EQ(0.25, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(510);  // <512MB
  EXPECT_EQ(0.5, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(512);  // 512MB
  EXPECT_EQ(0.5, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(640);  // 512+128MB
  EXPECT_EQ(0.5, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);  // 512+256MB
  EXPECT_EQ(0.5, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1000);  // <1GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1024);  // 1GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1536);  // 1.5GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2000);  // <2GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2048);  // 2GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(3000);  // <3GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(5120);  // 5GB
  EXPECT_EQ(4, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(8192);  // 8GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(16384);  // 16GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(32768);  // 32GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);  // <64GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
}

TEST_F(ApproximatedDeviceMemoryTest, GetApproximatedDeviceMemory) {
  // Test with new, 2026 limits - see https://crbug.com/454354290
  // TODO Remove next two lines when ready to retire feature flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kUpdatedDeviceMemoryLimitsFor2026);

#if BUILDFLAG(IS_ANDROID)
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(128);  // 128MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(256);  // 256MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(510);  // <512MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(512);  // 512MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(640);  // 512+128MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);  // 512+256MB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1000);  // <1GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1024);  // 1GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1536);  // 1.5GB
  EXPECT_EQ(1, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
#else
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(128);  // 128MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(256);  // 256MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(510);  // <512MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(512);  // 512MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(640);  // 512+128MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);  // 512+256MB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1000);  // <1GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1024);  // 1GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(1536);  // 1.5GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
#endif
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2000);  // <2GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2048);  // 2GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(3000);  // <3GB
  EXPECT_EQ(2, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(5120);  // 5GB
  EXPECT_EQ(4, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(8192);  // 8GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
#if BUILDFLAG(IS_ANDROID)
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(16384);  // 16GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(32768);  // 32GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);  // <64GB
  EXPECT_EQ(8, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
#else
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(16384);  // 16GB
  EXPECT_EQ(16, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(32768);  // 32GB
  EXPECT_EQ(32, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);  // <64GB
  EXPECT_EQ(32, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
#endif
}

}  // namespace

}  // namespace blink
