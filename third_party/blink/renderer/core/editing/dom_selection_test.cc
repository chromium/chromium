// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/dom_selection.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class DomSelectionTest : public EditingTestBase {
 protected:
  DomSelection* GetDomSelection() {
    return GetDocument().domWindow()->getSelection();
  }
};

TEST_F(DomSelectionTest, ToStringSkipsUserSelectNone) {
  SetBodyContent(
      "a"
      "<a href=\"#\" style=\"user-select: none; -webkit-user-select: "
      "none;\">b</a>"
      "c");

  GetDomSelection()->selectAllChildren(GetDocument().body(),
                                       ASSERT_NO_EXCEPTION);

  String selection_text = GetDomSelection()->toString();

  EXPECT_EQ("ac", selection_text);
}

TEST_F(DomSelectionTest, ToStringSkipsNestedUserSelectNone) {
  SetBodyContent(
      "<div>start "
      "<span style=\"user-select: none;\">unselectable <strong>nested</strong> "
      "text</span>"
      " end</div>");

  GetDomSelection()->selectAllChildren(GetDocument().body(),
                                       ASSERT_NO_EXCEPTION);

  String selection_text = GetDomSelection()->toString();

  EXPECT_EQ("start  end", selection_text);
}

}  // namespace blink
