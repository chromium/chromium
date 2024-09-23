// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gin/v8_platform_page_allocator.h"

#include "base/cpu.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-platform.h"

// includes for Branch Target Instruction tests
#if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)
// BTI is only available for AArch64, relevant platform are Android and Linux

#include "partition_alloc/arm_bti_test_functions.h"
#include "partition_alloc/page_allocator_constants.h"
#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#include "testing/gtest/include/gtest/gtest-death-test.h"
#endif
#endif  // defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

namespace gin {

TEST(V8PlatformPageAllocatorTest, VerifyGetPageConfig) {
  auto page_allocator = gin::PageAllocator();

  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kNoAccess),
            partition_alloc::PageAccessibilityConfiguration::kInaccessible);
  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kRead),
            partition_alloc::PageAccessibilityConfiguration::kRead);
  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kReadWrite),
            partition_alloc::PageAccessibilityConfiguration::kReadWrite);
#if defined(__ARM_FEATURE_BTI_DEFAULT)
  EXPECT_EQ(
      page_allocator.GetPageConfigPermissionsForTesting(
          v8::PageAllocator::kReadExecute),
      base::CPU::GetInstanceNoAllocation().has_bti()
          ? partition_alloc::PageAccessibilityConfiguration::
                kReadExecuteProtected
          : partition_alloc::PageAccessibilityConfiguration::kReadExecute);
  EXPECT_EQ(
      page_allocator.GetPageConfigPermissionsForTesting(
          v8::PageAllocator::kReadWriteExecute),
      base::CPU::GetInstanceNoAllocation().has_bti()
          ? partition_alloc::PageAccessibilityConfiguration::
                kReadWriteExecuteProtected
          : partition_alloc::PageAccessibilityConfiguration::kReadWriteExecute);
#else
  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kReadExecute),
            partition_alloc::PageAccessibilityConfiguration::kReadExecute);
  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kReadWriteExecute),
            partition_alloc::PageAccessibilityConfiguration::kReadWriteExecute);
#endif

  EXPECT_EQ(page_allocator.GetPageConfigPermissionsForTesting(
                v8::PageAllocator::kNoAccessWillJitLater),
            partition_alloc::PageAccessibilityConfiguration::
                kInaccessibleWillJitLater);
}

#if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

#ifdef GTEST_HAS_DEATH_TEST
using BTITestFunction = int64_t (*)(int64_t);

using V8PlatformPageAllocatorBTIDeathTest =
    ::testing::TestWithParam<v8::PageAllocator::Permission>;

TEST_P(V8PlatformPageAllocatorBTIDeathTest, VerifyExecutablePagesAreProtected) {
  const v8::PageAllocator::Permission permission_to_test = GetParam();

  auto page_allocator = gin::PageAllocator();

  auto const memory_size =
      partition_alloc::internal::PageAllocationGranularity();
  auto const memory_alignment =
      partition_alloc::internal::PageAllocationGranularity();

  // Next, map some read-write memory and copy some test helper functions there.
  char* const buffer = reinterpret_cast<char*>(page_allocator.AllocatePages(
      nullptr, memory_size, memory_alignment,
      v8::PageAllocator::Permission::kReadWriteExecute));

  ptrdiff_t const function_range =
      reinterpret_cast<char*>(arm_bti_test_function_end) -
      reinterpret_cast<char*>(arm_bti_test_function);
  ptrdiff_t const invalid_offset =
      reinterpret_cast<char*>(arm_bti_test_function_invalid_offset) -
      reinterpret_cast<char*>(arm_bti_test_function);

  // ensure alignment to 4 bytes required by function call convention
  EXPECT_EQ(0u, ((uint64_t)buffer) % 4);
  EXPECT_EQ(0u, ((uint64_t)function_range) % 4);
  EXPECT_EQ(0u, ((uint64_t)invalid_offset) % 4);

  memcpy(buffer, reinterpret_cast<void*>(arm_bti_test_function),
         function_range);

  // Next re-protect the page to the permission level to test
  page_allocator.SetPermissions(buffer, memory_size, permission_to_test);

  // Attempt to call a function with BTI landing pad.
  BTITestFunction const bti_enabled_fn =
      reinterpret_cast<BTITestFunction>(buffer);

  // bti_enabled_fn must return 18, no matter if BTI is actually enabled or not.
  EXPECT_EQ(bti_enabled_fn(15), 18);

  // Next, attempt to call a function without BTI landing pad.
  BTITestFunction const bti_invalid_fn =
      reinterpret_cast<BTITestFunction>(buffer + invalid_offset);

  // Expectation for behaviour of bti_invalid_fn depends on the capabilities of
  // the actual CPU we are running on. The code that were are trying to execute
  // is assembly code and always has BTI enabled.
  if (base::CPU::GetInstanceNoAllocation().has_bti()) {
#if BUILDFLAG(IS_POSIX)  // signal handling is available on POSIX compliant
                         // systems only
    EXPECT_EXIT({ bti_invalid_fn(15); }, testing::KilledBySignal(SIGILL),
                "");  // Should crash with SIGILL.
#endif                // BUILDFLAG(IS_POSIX)
  } else {
    EXPECT_EQ(bti_invalid_fn(15), 17);
  }

  page_allocator.FreePages(buffer, memory_size);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    V8PlatformPageAllocatorBTIDeathTest,
    testing::Values(v8::PageAllocator::Permission::kReadExecute,
                    v8::PageAllocator::Permission::kReadWriteExecute));

#endif  // GTEST_HAS_DEATH_TEST
#endif  // if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

}  // namespace gin
