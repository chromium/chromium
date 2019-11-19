// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

class FrameResourceFetcherPropertiesTest : public testing::Test {
 public:
  FrameResourceFetcherPropertiesTest()
      : dummy_page_holder_(std::make_unique<DummyPageHolder>(IntSize(1, 1))),
        properties_(MakeGarbageCollected<FrameResourceFetcherProperties>(
            *MakeGarbageCollected<FrameOrImportedDocument>(
                *dummy_page_holder_->GetDocument().Loader(),
                dummy_page_holder_->GetDocument()))) {}

 protected:
  const std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  const Persistent<FrameResourceFetcherProperties> properties_;
};

TEST_F(FrameResourceFetcherPropertiesTest, SubframeDeprioritization) {
  Settings* settings = dummy_page_holder_->GetDocument().GetSettings();
  GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
      true, WebConnectionType::kWebConnectionTypeCellular3G,
      WebEffectiveConnectionType::kType3G, 1 /* http_rtt_msec */,
      10.0 /* max_bandwidth_mbps */);

  // Experiment is not enabled, expect default values.
  EXPECT_FALSE(properties_->IsSubframeDeprioritizationEnabled());

  // Low priority iframes enabled but network is not slow enough.
  settings->SetLowPriorityIframesThreshold(WebEffectiveConnectionType::kType2G);
  EXPECT_FALSE(properties_->IsSubframeDeprioritizationEnabled());

  // Low priority iframes enabled and network is slow.
  GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
      true, WebConnectionType::kWebConnectionTypeCellular3G,
      WebEffectiveConnectionType::kType2G, 1 /* http_rtt_msec */,
      10.0 /* max_bandwidth_mbps */);
  EXPECT_TRUE(properties_->IsSubframeDeprioritizationEnabled());
}

}  // namespace blink
