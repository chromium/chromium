// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

using HTMLAnchorElementTest = PageTestBase;

TEST_F(HTMLAnchorElementTest, UnchangedHrefDoesNotInvalidateStyle) {
  SetBodyInnerHTML("<a href=\"https://www.chromium.org/\">Chromium</a>");
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* anchor =
      To<HTMLAnchorElement>(GetDocument().QuerySelector(AtomicString("a")));
  anchor->setAttribute(html_names::kHrefAttr,
                       AtomicString("https://www.chromium.org/"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

}  // namespace
}  // namespace blink
