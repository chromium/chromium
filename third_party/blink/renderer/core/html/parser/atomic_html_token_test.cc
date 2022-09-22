// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AtomicHTMLTokenTest, EmptyAttributeValueFromHTMLToken) {
  HTMLToken token;
  token.BeginStartTag('a');
  token.AddNewAttribute();
  token.BeginAttributeName(3);
  token.AppendToAttributeName('b');
  token.EndAttributeName(4);
  token.AddNewAttribute();
  token.BeginAttributeName(5);
  token.AppendToAttributeName('c');
  token.EndAttributeName(6);
  token.BeginAttributeValue(8);
  token.EndAttributeValue(8);

  AtomicHTMLToken atoken(token);

  const blink::Attribute* attribute_b = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), "b", AtomicString()));
  ASSERT_TRUE(attribute_b);
  EXPECT_FALSE(attribute_b->Value().IsNull());
  EXPECT_TRUE(attribute_b->Value().IsEmpty());

  const blink::Attribute* attribute_c = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), "c", AtomicString()));
  ASSERT_TRUE(attribute_c);
  EXPECT_FALSE(attribute_c->Value().IsNull());
  EXPECT_TRUE(attribute_c->Value().IsEmpty());

  const blink::Attribute* attribute_d = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), "d", AtomicString()));
  EXPECT_FALSE(attribute_d);
}

}  // namespace blink
