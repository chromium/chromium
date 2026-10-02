// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/qualified_name.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class QualifiedNameTest : public testing::Test {};

TEST_F(QualifiedNameTest, Constructor1) {
  QualifiedName name{AtomicString("foo")};
  EXPECT_EQ(name.Prefix(), g_null_atom);
  EXPECT_EQ(name.LocalName(), AtomicString("foo"));
  EXPECT_EQ(name.NamespaceURI(), g_null_atom);

  EXPECT_EQ(name, QualifiedName(g_null_atom, AtomicString("foo"), g_null_atom));
}

TEST_F(QualifiedNameTest, StaticQualifiedName) {
  QualifiedName dynamic_name{AtomicString("dynamic_foo")};
  EXPECT_FALSE(dynamic_name.IsDefinedName());

  EXPECT_TRUE(html_names::kDivTag.IsDefinedName());

  QualifiedNameWithHash static_name(g_null_atom, AtomicString("static_foo"),
                                    g_null_atom, /*is_static=*/true);
  EXPECT_TRUE(static_name.IsDefinedName());
}

}  // namespace blink
