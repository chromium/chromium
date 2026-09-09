// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_disabling_feature_handle.h"

#include <utility>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

class BackForwardCacheDisablingFeatureHandleTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }
};

TEST_F(BackForwardCacheDisablingFeatureHandleTest, DefaultConstructor) {
  BackForwardCacheDisablingFeatureHandle handle;
  EXPECT_FALSE(handle.IsValid());

  // Resetting an invalid handle should be a safe no-op.
  handle.Reset();
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(BackForwardCacheDisablingFeatureHandleTest, ConstructorAndDestructor) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  {
    BackForwardCacheDisablingFeatureHandle handle =
        rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
            blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth);
    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
        blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
  }

  // Destructor should have called `Reset()`, removing the feature.
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
}

TEST_F(BackForwardCacheDisablingFeatureHandleTest, Reset) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  BackForwardCacheDisablingFeatureHandle handle =
      rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
          blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth);
  EXPECT_TRUE(handle.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  handle.Reset();
  EXPECT_FALSE(handle.IsValid());
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  // Further calls to `Reset()` should be a no-op.
  handle.Reset();
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(BackForwardCacheDisablingFeatureHandleTest, MoveConstructor) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  BackForwardCacheDisablingFeatureHandle handle1 =
      rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
          blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth);
  EXPECT_TRUE(handle1.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  BackForwardCacheDisablingFeatureHandle handle2(std::move(handle1));
  EXPECT_FALSE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  // Resetting the moved-from `handle1` should not affect `handle2` or `rfh`.
  handle1.Reset();
  EXPECT_TRUE(handle2.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  handle2.Reset();
  EXPECT_FALSE(handle2.IsValid());
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
}

TEST_F(BackForwardCacheDisablingFeatureHandleTest,
       MoveAssignmentToEmptyHandle) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  BackForwardCacheDisablingFeatureHandle handle1 =
      rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
          blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth);
  BackForwardCacheDisablingFeatureHandle handle2;
  EXPECT_FALSE(handle2.IsValid());

  handle2 = std::move(handle1);
  EXPECT_FALSE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));

  handle2.Reset();
  EXPECT_FALSE(handle2.IsValid());
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
}

TEST_F(BackForwardCacheDisablingFeatureHandleTest,
       MoveAssignmentToActiveHandle) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  BackForwardCacheDisablingFeatureHandle handle1 =
      rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
          blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth);
  BackForwardCacheDisablingFeatureHandle handle2 =
      rfh->RegisterBackForwardCacheDisablingNonStickyFeature(
          blink::scheduler::WebSchedulerTrackedFeature::kWebHID);

  EXPECT_TRUE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebHID));

  // Move-assign `handle2` into `handle1` while `handle1` already holds an
  // active handle.
  handle1 = std::move(handle2);
  EXPECT_FALSE(handle2.IsValid());
  EXPECT_TRUE(handle1.IsValid());

  // TODO(thestig): Defaulting the move-assignment operator causes a resource
  // leak if `this` already holds an active handle. Because `= default` does not
  // call `Reset()`, any previously held feature registration on
  // `render_frame_host_` is overwritten without calling
  // `OnBackForwardCacheDisablingFeatureRemoved()`, permanently preventing the
  // frame from entering BackForwardCache.
  //
  // `operator=` should call `Reset()` before taking ownership of `other`'s
  // members (after checking `this != &other`).
  //
  // Currently, `kWebBluetooth` was leaked on `rfh` and remains present even
  // though `handle1` no longer tracks it. Once fixed, `kWebBluetooth` should no
  // longer be present here (i.e. EXPECT_FALSE).
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebHID));

  // Resetting `handle1` cleans up `kWebHID`, but `kWebBluetooth` remains
  // leaked.
  handle1.Reset();
  EXPECT_FALSE(handle1.IsValid());
  EXPECT_FALSE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebHID));

  // TODO(thestig): Once the bug is fixed, `kWebBluetooth` will not be leaked
  // and this expectation should be EXPECT_FALSE.
  EXPECT_TRUE(rfh->GetBackForwardCacheDisablingFeatures().Has(
      blink::scheduler::WebSchedulerTrackedFeature::kWebBluetooth));
}

}  // namespace content
