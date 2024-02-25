// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/permission_shadow_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class PermissionShadowElementTest : public PageTestBase {
 protected:
  PermissionShadowElementTest() = default;

 private:
  ScopedPermissionElementForTest scoped_feature_{true};
};

TEST_F(PermissionShadowElementTest, PropagateCSSPropertyInnerElement) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      permission {
        display: table-cell;
        height: 40px;
        vertical-align: middle;
        width: 200px;
      }
    </style>
    <permission id='test' type='camera'>
  )HTML");

  auto* permission_element = To<HTMLPermissionElement>(GetElementById("test"));
  auto* container =
      To<HTMLDivElement>(permission_element->GetShadowRoot()->firstChild());
  EXPECT_TRUE(container);
  const ComputedStyle& element_style = permission_element->ComputedStyleRef();
  const ComputedStyle& container_style = container->ComputedStyleRef();
  EXPECT_EQ(element_style.Width(), container_style.Width());
  EXPECT_EQ(element_style.Height(), container_style.Height());
  auto* permission_text_span = To<HTMLSpanElement>(container->firstChild());
  EXPECT_TRUE(permission_text_span);
  const ComputedStyle& text_style = permission_text_span->ComputedStyleRef();
  EXPECT_EQ(EDisplay::kTableCell, text_style.Display());
  EXPECT_EQ(EVerticalAlign::kMiddle, text_style.VerticalAlign());
}

}  // namespace blink
