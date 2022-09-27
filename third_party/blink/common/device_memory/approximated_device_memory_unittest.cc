// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ApproximatedDeviceMemoryTest : public testing::Test {};

TEST_F(ApproximatedDeviceMemoryTest, GetApproximatedDeviceMemory) {
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(128);  // 128MB
  EXPECT_EQ(0.125, ApproximatedDeviceMemory::GetApproximatedDeviceMemory());
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

}  // namespace

}  // namespace blink
