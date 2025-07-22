// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLGeolocationElementTestBase : public PageTestBase {
 protected:
  HTMLGeolocationElementTestBase() = default;

  explicit HTMLGeolocationElementTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : PageTestBase(time_source) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kGeolocationElement);
    PageTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedGeolocationElementForTest scoped_feature_{true};
};

TEST_F(HTMLGeolocationElementTestBase, GetTypeAttribute) {
  auto* geolocation_element =
      MakeGarbageCollected<HTMLGeolocationElement>(GetDocument());
  EXPECT_EQ(AtomicString("geolocation"), geolocation_element->GetType());
}
}  // namespace blink
