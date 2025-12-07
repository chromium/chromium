// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_names.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"

namespace blink {

class SanitizerNamesTest : public testing::Test {};

TEST_F(SanitizerNamesTest, NameSet) {
  // We have a name prefix:localname, in namespace "namespace". We'll check
  // whether the hash set will correctly match names (except for the prefix).
  QualifiedName qname(AtomicString("prefix"), AtomicString("localname"),
                      AtomicString("namespace"));
  QualifiedName different_prefix(AtomicString("other"), qname.LocalName(),
                                 qname.NamespaceURI());
  QualifiedName different_localname(qname.Prefix(), AtomicString("other"),
                                    qname.NamespaceURI());
  QualifiedName different_namespace(qname.Prefix(), qname.LocalName(),
                                    AtomicString("other"));

  SanitizerNameSet names;
  names.insert(qname);
  EXPECT_TRUE(names.Contains(qname));
  EXPECT_TRUE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_localname);
  EXPECT_FALSE(names.Contains(qname));
  EXPECT_FALSE(names.Contains(different_prefix));
  EXPECT_TRUE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_prefix);
  EXPECT_TRUE(names.Contains(qname));
  EXPECT_TRUE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_namespace);
  EXPECT_FALSE(names.Contains(qname));
  EXPECT_FALSE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_TRUE(names.Contains(different_namespace));
}

TEST_F(SanitizerNamesTest, NameMap) {
  // Same setup as above, but now with SanitizerNameMap.
  QualifiedName qname(AtomicString("prefix"), AtomicString("localname"),
                      AtomicString("namespace"));
  QualifiedName different_prefix(AtomicString("other"), qname.LocalName(),
                                 qname.NamespaceURI());
  QualifiedName different_localname(qname.Prefix(), AtomicString("other"),
                                    qname.NamespaceURI());
  QualifiedName different_namespace(qname.Prefix(), qname.LocalName(),
                                    AtomicString("other"));

  SanitizerNameMap names;
  names.insert(qname, SanitizerNameSet());
  EXPECT_TRUE(names.Contains(qname));
  EXPECT_TRUE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_localname, SanitizerNameSet());
  EXPECT_FALSE(names.Contains(qname));
  EXPECT_FALSE(names.Contains(different_prefix));
  EXPECT_TRUE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_prefix, SanitizerNameSet());
  EXPECT_TRUE(names.Contains(qname));
  EXPECT_TRUE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_FALSE(names.Contains(different_namespace));

  names.clear();
  names.insert(different_namespace, SanitizerNameSet());
  EXPECT_FALSE(names.Contains(qname));
  EXPECT_FALSE(names.Contains(different_prefix));
  EXPECT_FALSE(names.Contains(different_localname));
  EXPECT_TRUE(names.Contains(different_namespace));
}

}  // namespace blink
