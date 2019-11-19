// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_link_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class HTMLLinkElementSizesAttributeTest : public testing::Test {};

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesPropertyValue_updatesAttribute) {
  auto* document = MakeGarbageCollected<Document>();
  auto* link =
      MakeGarbageCollected<HTMLLinkElement>(*document, CreateElementFlags());
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(g_null_atom, sizes->value());
  sizes->setValue("   a b  c ");
  EXPECT_EQ("   a b  c ", link->FastGetAttribute(html_names::kSizesAttr));
  EXPECT_EQ("   a b  c ", sizes->value());
}

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesAttribute_updatesSizesPropertyValue) {
  auto* document = MakeGarbageCollected<Document>();
  auto* link =
      MakeGarbageCollected<HTMLLinkElement>(*document, CreateElementFlags());
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(g_null_atom, sizes->value());
  link->setAttribute(html_names::kSizesAttr, "y  x ");
  EXPECT_EQ("y  x ", sizes->value());
}

}  // namespace blink
