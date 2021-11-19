// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_platform_page_allocator.h"

#include "base/cpu.h"

#include "testing/gtest/include/gtest/gtest.h"

// includes for Branch Target Instruction tests
#if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)
// BTI is only available for AArch64, relevant platform are Android and Linux

#include "base/allocator/partition_allocator/arm_bti_test_functions.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#if defined(OS_POSIX)
#include <signal.h>
#include "testing/gtest/include/gtest/gtest-death-test.h"
#endif
#endif  // defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

namespace gin {

TEST(V8PlatformPageAllocatorTest, VerifyGetPageConfig) {
  auto sut = gin::PageAllocator();

  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kNoAccess),
           base::PageInaccessible);
  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kRead),
           base::PageRead);
  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kReadWrite),
           base::PageReadWrite);
  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kReadWriteExecute),
           base::PageReadWriteExecute);

#if defined(__ARM_FEATURE_BTI_DEFAULT)
  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kReadExecute),
           base::CPU::GetInstanceNoAllocation().has_bti()
               ? base::PageReadExecuteProtected
               : base::PageReadExecute);
#else
  CHECK_EQ(sut.GetPageConfigForTesting(v8::PageAllocator::kReadExecute),
           base::PageReadExecute);
#endif

  CHECK_EQ(
      sut.GetPageConfigForTesting(v8::PageAllocator::kNoAccessWillJitLater),
      base::PageInaccessible);
}

#if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

using BTITestFunction = int64_t (*)(int64_t);

TEST(V8PlatformPageAllocatorBTITest, VerifyReadExecutePagesAreProtected) {
  auto page_allocator = gin::PageAllocator();

  auto const memory_size = base::PageAllocationGranularity();
  auto const memory_alignment = base::PageAllocationGranularity();

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
  page_allocator.SetPermissions(buffer, memory_size,
                                v8::PageAllocator::Permission::kReadExecute);

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
#if defined(OS_POSIX)  // signal handling is available on POSIX compliant
                       // systems only
    EXPECT_EXIT({ bti_invalid_fn(15); }, testing::KilledBySignal(SIGILL),
                "");  // Should crash with SIGILL.
#endif                // defined(OS_POSIX)
  } else {
    EXPECT_EQ(bti_invalid_fn(15), 17);
  }

  page_allocator.FreePages(buffer, memory_size);
}

TEST(V8PlatformAllocatorBTITest, VerifyReadWriteExecutePagesAreNotProtected) {
  auto page_allocator = gin::PageAllocator();

  auto const memory_size = base::PageAllocationGranularity();
  auto const memory_alignment = base::PageAllocationGranularity();

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

  // Attempt to call a function with BTI landing pad.
  BTITestFunction const bti_enabled_fn =
      reinterpret_cast<BTITestFunction>(buffer);

  // bti_enabled_fn must return 18, no matter if BTI is actually enabled or not.
  EXPECT_EQ(bti_enabled_fn(15), 18);

  // Next, attempt to call a function without BTI landing pad.
  BTITestFunction const bti_invalid_fn =
      reinterpret_cast<BTITestFunction>(buffer + invalid_offset);

  // Since permission kReadWriteExecute wont actually cause BTI to be enabled
  // for the allocated page, calling this function must return without error.
  EXPECT_EQ(bti_invalid_fn(15), 17);

  page_allocator.FreePages(buffer, memory_size);
}
#endif  // if defined(ARCH_CPU_ARM64) && (OS_LINUX || OS_ANDROID)

}  // namespace gin
