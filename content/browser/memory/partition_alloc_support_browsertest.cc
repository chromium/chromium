// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_alloc_features.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {
namespace {

class PartitionAllocTighterAlignedAllocBoundBrowserTest
    : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(PartitionAllocTighterAlignedAllocBoundBrowserTest,
                       FeatureFlagPlumbing) {
  // On Android (particularly 32-bit x86 emulator), navigating to a page ensures
  // that the browser window, UI framework, and graphics surface initialization
  // (libgui.so / libandroid_runtime.so) complete properly before test teardown,
  // avoiding a race condition / SIGSEGV during activity destruction.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* allocator = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  ASSERT_TRUE(allocator);
  EXPECT_EQ(allocator->use_tighter_aligned_alloc_bound_for_testing(),
            base::FeatureList::IsEnabled(
                base::features::kPartitionAllocTighterAlignedAllocBound));
#endif
}

}  // namespace
}  // namespace content
