// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AtomicHTMLTokenTest, EmptyAttributeValueFromHTMLToken) {
  test::TaskEnvironment task_environment;
  HTMLToken token;
  token.BeginStartTag('a');
  token.AddNewAttribute('b');
  token.AddNewAttribute('c');

  AtomicHTMLToken atoken(token);

  const blink::Attribute* attribute_b = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), AtomicString("b"), AtomicString()));
  ASSERT_TRUE(attribute_b);
  EXPECT_FALSE(attribute_b->Value().IsNull());
  EXPECT_TRUE(attribute_b->Value().empty());

  const blink::Attribute* attribute_c = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), AtomicString("c"), AtomicString()));
  ASSERT_TRUE(attribute_c);
  EXPECT_FALSE(attribute_c->Value().IsNull());
  EXPECT_TRUE(attribute_c->Value().empty());

  const blink::Attribute* attribute_d = atoken.GetAttributeItem(
      QualifiedName(AtomicString(), AtomicString("d"), AtomicString()));
  EXPECT_FALSE(attribute_d);
}

}  // namespace blink
