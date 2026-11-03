// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// Parameterized on whether Container Timing is enabled.
class HTMLElementContainerTimingAttributesTest
    : public PageTestBase,
      public testing::WithParamInterface<bool> {
 public:
  HTMLElementContainerTimingAttributesTest() : scoped_feature_(GetParam()) {}

 private:
  ScopedContainerTimingForTest scoped_feature_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HTMLElementContainerTimingAttributesTest,
                         testing::Bool());

// The attributes are counted whether or not Container Timing is enabled (e.g.
// outside the origin trial), so adoption can be measured before it ships.
TEST_P(HTMLElementContainerTimingAttributesTest, UseCounters) {
  SetBodyContent(R"HTML(
    <div containertiming="root">
      <div containertimingignore></div>
    </div>
  )HTML");

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kContainerTimingAttribute));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kContainerTimingAttributeHasRootName));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kContainerTimingIgnoreAttribute));
}

}  // namespace blink
