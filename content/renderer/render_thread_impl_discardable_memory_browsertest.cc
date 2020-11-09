// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/madv_free_discardable_memory_allocator_posix.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/discardable_memory_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/buffer_format_util.h"
#include "url/gurl.h"

namespace content {
namespace {

class RenderThreadImplDiscardableMemoryBrowserTest : public ContentBrowserTest {
 public:
  RenderThreadImplDiscardableMemoryBrowserTest()
      : discardable_memory_allocator_(nullptr) {}

  // Overridden from BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUpOnMainThread() override {
    EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
    PostTaskToInProcessRendererAndWait(base::BindOnce(
        &RenderThreadImplDiscardableMemoryBrowserTest::SetUpOnRenderThread,
        base::Unretained(this)));
  }

  base::DiscardableMemoryAllocator* discardable_memory_allocator() {
    return discardable_memory_allocator_;
  }

 private:
  void SetUpOnRenderThread() {
    discardable_memory_allocator_ =
        RenderThreadImpl::current()->GetDiscardableMemoryAllocatorForTest();
  }

  base::DiscardableMemoryAllocator* discardable_memory_allocator_;
};

IN_PROC_BROWSER_TEST_F(RenderThreadImplDiscardableMemoryBrowserTest,
                       LockDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  std::unique_ptr<base::DiscardableMemory> memory =
      discardable_memory_allocator()->AllocateLockedDiscardableMemory(kSize);

  ASSERT_TRUE(memory);
  void* addr = memory->data();
  ASSERT_NE(nullptr, addr);

  memory->Unlock();

  // Simulate memory being discarded as if under memory pressure.
  memory->DiscardForTesting();

  // Should fail as memory should have been purged.
  EXPECT_FALSE(memory->Lock());
}

// Ensure that address space mapped by allocating discardable memory is unmapped
// after discarding under memory pressure, by creating and discarding a large
// amount of discardable memory.
//
// Disable the test for the Android asan build.
// See http://crbug.com/667837 for detail.
#if !(defined(OS_ANDROID) && defined(ADDRESS_SANITIZER))
IN_PROC_BROWSER_TEST_F(RenderThreadImplDiscardableMemoryBrowserTest,
                       DiscardableMemoryAddressSpace) {
  const size_t kLargeSize = 4 * 1024 * 1024;   // 4MiB.
  const size_t kNumberOfInstances = 1024 + 1;  // >4GiB total.

  base::DiscardableMemoryBacking impl = base::GetDiscardableMemoryBacking();

  // TODO(gordonguan): When MADV_FREE DiscardableMemory is discarded, the
  // backing memory is freed, but remains mapped in memory. It is only
  // unmapped when the object is destroyed, or on the next Lock() after
  // discard. Therefore, an abundance of discarded but mapped discardable
  // memory instances may cause an out-of-memory condition.
  if (impl != base::DiscardableMemoryBacking::kSharedMemory)
    return;

  std::vector<std::unique_ptr<base::DiscardableMemory>> instances;
  for (size_t i = 0; i < kNumberOfInstances; ++i) {
    std::unique_ptr<base::DiscardableMemory> memory =
        discardable_memory_allocator()->AllocateLockedDiscardableMemory(
            kLargeSize);
    ASSERT_TRUE(memory);
    void* addr = memory->data();
    ASSERT_NE(nullptr, addr);
    memory->Unlock();
    instances.push_back(std::move(memory));
  }
}
#endif

IN_PROC_BROWSER_TEST_F(RenderThreadImplDiscardableMemoryBrowserTest,
                       ReleaseFreeDiscardableMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  base::DiscardableMemoryBacking impl = base::GetDiscardableMemoryBacking();

  std::unique_ptr<base::DiscardableMemory> memory =
      discardable_memory_allocator()->AllocateLockedDiscardableMemory(kSize);

  EXPECT_TRUE(memory);
  EXPECT_GE(discardable_memory_allocator()->GetBytesAllocated(), kSize);
  memory.reset();

  EXPECT_EQ(discardable_memory_allocator()->GetBytesAllocated(), 0U);
  if (impl != base::DiscardableMemoryBacking::kSharedMemory)
    return;

  EXPECT_GE(discardable_memory::DiscardableSharedMemoryManager::Get()
                ->GetBytesAllocated(),
            kSize);

  static_cast<discardable_memory::ClientDiscardableSharedMemoryManager*>(
      discardable_memory_allocator())
      ->ReleaseFreeMemory();

  // Busy wait for host memory usage to be reduced.
  base::TimeTicks end =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(5);
  while (base::TimeTicks::Now() < end) {
    if (!discardable_memory::DiscardableSharedMemoryManager::Get()
             ->GetBytesAllocated())
      break;
  }

  EXPECT_LT(base::TimeTicks::Now(), end);
}

// TODO(crbug.com/974850): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(RenderThreadImplDiscardableMemoryBrowserTest,
                       DISABLED_ReleaseFreeMemory) {
  const size_t kSize = 1024 * 1024;  // 1MiB.

  std::unique_ptr<base::DiscardableMemory> memory =
      discardable_memory_allocator()->AllocateLockedDiscardableMemory(kSize);

  EXPECT_TRUE(memory);
  memory.reset();

  EXPECT_GE(discardable_memory_allocator()->GetBytesAllocated(), kSize);

  // Call RenderThreadImpl::ReleaseFreeMemory through a fake memory pressure
  // notification.
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  RunAllTasksUntilIdle();

  EXPECT_EQ(0U, discardable_memory_allocator()->GetBytesAllocated());
}

IN_PROC_BROWSER_TEST_F(RenderThreadImplDiscardableMemoryBrowserTest,
                       CheckReleaseMemory) {
  std::vector<std::unique_ptr<base::DiscardableMemory>> all_memory;
  auto* allocator =
      static_cast<discardable_memory::ClientDiscardableSharedMemoryManager*>(
          discardable_memory_allocator());
  constexpr size_t kMaxRegions = 10;
  constexpr size_t kRegionSize = 4 * 1024 * 1024;

  allocator->SetBytesAllocatedLimitForTesting(kMaxRegions * kRegionSize);

  // Allocate the maximum amount of memory.
  for (size_t i = 0; i < kMaxRegions; i++) {
    auto region = allocator->AllocateLockedDiscardableMemoryWithRetryOrDie(
        kRegionSize, base::DoNothing());
    all_memory.push_back(std::move(region));
  }

  auto region = allocator->AllocateLockedDiscardableMemoryWithRetryOrDie(
      kRegionSize, base::BindLambdaForTesting([&]() { all_memory.clear(); }));

  // Checks that the memory reclaim callback was called, and that the allocation
  // then succeeded. Allocation success is checked because the test has not
  // crashed.
  EXPECT_TRUE(all_memory.empty());

  allocator->SetBytesAllocatedLimitForTesting(0);
}

}  // namespace
}  // namespace content
