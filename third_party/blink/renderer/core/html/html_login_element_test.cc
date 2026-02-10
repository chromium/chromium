// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_login_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HTMLLoginElementTest : public PageTestBase {};

TEST_F(HTMLLoginElementTest, TagName) {
  auto* element = MakeGarbageCollected<HTMLLoginElement>(GetDocument());
  EXPECT_EQ(element->tagName(), "LOGIN");
  EXPECT_EQ(element->localName(), "login");
}

}  // namespace blink
