// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class AnchorElementMetricsSenderTest : public SimTest {
 protected:
  AnchorElementMetricsSenderTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    feature_list_.InitAndEnableFeature(features::kNavigationPredictor);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Test that duplicate anchor elements are not added to
// AnchorElementMetricsSender.
TEST_F(AnchorElementMetricsSenderTest, AddAnhcorElement) {
  String source("http://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href=''>example</a><a id='anchor2' href=''>example</a>");
  auto* anchor_element_1 =
      To<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  auto* anchor_element_2 =
      To<HTMLAnchorElement>(GetDocument().getElementById("anchor2"));

  AnchorElementMetricsSender* sender =
      AnchorElementMetricsSender::From(GetDocument());
  EXPECT_EQ(2u, sender->GetAnchorElements().size());

  // Adding the anchor elements again should not change the size.
  sender->AddAnchorElement(*anchor_element_1);
  EXPECT_EQ(2u, sender->GetAnchorElements().size());
  sender->AddAnchorElement(*anchor_element_2);
  EXPECT_EQ(2u, sender->GetAnchorElements().size());
}

}  // namespace blink
