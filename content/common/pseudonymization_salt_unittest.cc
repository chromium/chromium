// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pseudonymization_salt.h"

#include <tuple>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/gtest_util.h"
#include "build/blink_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PseudonymizationSaltTest : public ::testing::Test {
  void SetUp() override { ResetSaltForTesting(); }
};

using PseudonymizationSaltDeathTest = PseudonymizationSaltTest;

TEST_F(PseudonymizationSaltTest, SetAndGetSalt) {
  SetPseudonymizationSalt(0xDEADBEEF);
  uint32_t salt = GetPseudonymizationSalt();
  EXPECT_EQ(salt, 0xDEADBEEF);
}

// Early during process startup (before Browser process reaches salt
// initialization;  or before a Child process receives the salt over IPC), the
// pseudonymization salt is not initialized and is invalid - this test verifies
// presence of a DCHECK in GetPseudonymizationSalt that verifies that
// GetPseudonymizationSalt is only called after salt initialization.
//
// See also https://crbug.com/1339992 which should move salt initialization
// much earlier in Renderer processes (and other Child and Utility processes).
TEST_F(PseudonymizationSaltDeathTest, GetEmptySalt) {
  EXPECT_DCHECK_DEATH({ std::ignore = GetPseudonymizationSalt(); });
}

TEST_F(PseudonymizationSaltDeathTest, SetTwoDifferentSalts) {
  SetPseudonymizationSalt(0xDEADBEEF);
  EXPECT_DCHECK_DEATH({ SetPseudonymizationSalt(0xBEEFDEAD); });
}

TEST_F(PseudonymizationSaltDeathTest, SetZeroSalt) {
  EXPECT_DCHECK_DEATH({ SetPseudonymizationSalt(0x0u); });
}

TEST_F(PseudonymizationSaltTest, SetTwoSaltsAllowed) {
  SetPseudonymizationSalt(0xDEADBEEF);
  SetPseudonymizationSalt(0xDEADBEEF);
  uint32_t salt = GetPseudonymizationSalt();
  EXPECT_EQ(salt, 0xDEADBEEF);
}

TEST_F(PseudonymizationSaltTest, IsSaltInitialized) {
  EXPECT_FALSE(IsSaltInitialized());
  SetPseudonymizationSalt(0xDEADBEEF);
  EXPECT_TRUE(IsSaltInitialized());
}

#if BUILDFLAG(USE_BLINK)
// Verifies shared memory round-trip for passing salt to child processes.
// See https://crbug.com/40850085.
TEST_F(PseudonymizationSaltTest, SharedMemoryRoundTrip) {
  SetPseudonymizationSalt(0xDEADBEEF);

  const base::ReadOnlySharedMemoryRegion& region =
      GetPseudonymizationSaltSharedMemoryRegion();
  ASSERT_TRUE(region.IsValid());

  // Duplicate the region to simulate passing to child process.
  base::ReadOnlySharedMemoryRegion region_copy = region.Duplicate();
  ASSERT_TRUE(region_copy.IsValid());

  // Simulate child process: reset and initialize from shared memory.
  ResetSaltForTesting();
  EXPECT_FALSE(IsSaltInitialized());
  EXPECT_TRUE(internal::InitializeSaltFromSharedMemory(std::move(region_copy)));
  EXPECT_TRUE(IsSaltInitialized());
  EXPECT_EQ(GetPseudonymizationSalt(), 0xDEADBEEFu);
}

TEST_F(PseudonymizationSaltTest, SharedMemoryInvalidRegion) {
  base::ReadOnlySharedMemoryRegion invalid_region;
  EXPECT_FALSE(
      internal::InitializeSaltFromSharedMemory(std::move(invalid_region)));
  EXPECT_FALSE(IsSaltInitialized());
}
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace content
