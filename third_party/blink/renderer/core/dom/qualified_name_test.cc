// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/qualified_name.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"

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

TEST_F(QualifiedNameTest, HashFitsInBitfield) {
  // HashComponents() hashes StringImpl* pointer addresses, which vary across
  // runs. A single unmasked 32-bit hash has a 1/256 chance of having its top 8
  // bits be zero by coincidence, so test multiple names to avoid false passes.
  for (wtf_size_t i = 0; i < 100; ++i) {
    QualifiedName name(AtomicString::Number(i));
    uint32_t computed_hash = name.Impl()->ComputeHash();
    EXPECT_LE(computed_hash, 0xFFFFFFu);
    EXPECT_EQ(blink::GetHash(name), computed_hash);
  }
}

TEST_F(QualifiedNameTest, CaseInsensitiveBloomFilter) {
  EXPECT_EQ(svg_names::kViewBoxAttr.bloom_filter,
            svg_names::kViewBoxAttr.BloomFilter());
  EXPECT_EQ(svg_names::kViewBoxAttr.BloomFilter(),
            QualifiedName(AtomicString("viewbox")).BloomFilter());
  EXPECT_EQ(svg_names::kViewBoxAttr.BloomFilter(),
            QualifiedName(AtomicString("VIEWBOX")).BloomFilter());

  QualifiedName mixed_case(AtomicString("data-CustomAttr"));
  QualifiedName lower_case(AtomicString("data-customattr"));
  QualifiedName upper_case(AtomicString("DATA-CUSTOMATTR"));
  EXPECT_EQ(mixed_case.BloomFilter(), lower_case.BloomFilter());
  EXPECT_EQ(mixed_case.BloomFilter(), upper_case.BloomFilter());
  // Computing the Bloom filter should not eagerly materialize
  // local_name_upper_.
  EXPECT_TRUE(mixed_case.Impl()->local_name_upper_.IsNull());
  EXPECT_EQ(mixed_case.LocalNameUpper(), AtomicString("DATA-CUSTOMATTR"));
  EXPECT_EQ(mixed_case.BloomFilter(),
            Element::FilterForString(mixed_case.LocalNameUpper()));
}

TEST_F(QualifiedNameTest, TagToQualifiedName) {
  EXPECT_EQ(&html_names::TagToQualifiedName(html_names::HTMLTag::kUnknown),
            &g_null_name);
  EXPECT_EQ(&html_names::TagToQualifiedName(html_names::HTMLTag::kA),
            &html_names::kATag);
  EXPECT_EQ(&html_names::TagToQualifiedName(html_names::HTMLTag::kXmp),
            &html_names::kXmpTag);
}

TEST_F(QualifiedNameTest, HashIgnoringPrefixAgreesWithMatches) {
  const AtomicString kNsA("urn:example:a");
  const AtomicString kNsB("urn:example:b");
  const AtomicString kPrefix1("p1");
  const AtomicString kPrefix2("p2");

  const QualifiedName kNames[] = {
      QualifiedName::Null(),
      QualifiedName(g_null_atom, g_null_atom, g_null_atom),
      html_names::kDivTag,
      QualifiedName(kPrefix1, AtomicString("div"), html_names::xhtmlNamespaceURI),
      QualifiedName(kPrefix2, AtomicString("div"), html_names::xhtmlNamespaceURI),
      QualifiedName(g_null_atom, AtomicString("DIV"), kNsA),
      QualifiedName(kPrefix1, AtomicString("DIV"), kNsA),
      QualifiedName(g_null_atom, AtomicString("div"), kNsA),
      QualifiedName(g_null_atom, AtomicString("div"), kNsB),
      QualifiedName(g_null_atom, AtomicString("div"), g_null_atom),
      QualifiedName(kPrefix1, AtomicString("div"), g_empty_atom),
      html_names::kATag,
      svg_names::kATag,
  };

  for (const QualifiedName& a : kNames) {
    for (const QualifiedName& b : kNames) {
      if (a.Matches(b)) {
        EXPECT_EQ(a.HashIgnoringPrefix(), b.HashIgnoringPrefix())
            << a << " vs " << b;
      }
    }
  }

  // Spot-check key pairs directly.
  EXPECT_TRUE(html_names::kDivTag.Matches(
      QualifiedName(kPrefix1, AtomicString("div"), html_names::xhtmlNamespaceURI)));
  EXPECT_EQ(html_names::kDivTag.HashIgnoringPrefix(),
            QualifiedName(kPrefix1, AtomicString("div"), html_names::xhtmlNamespaceURI)
                .HashIgnoringPrefix());

  const QualifiedName foreign_div(g_null_atom, AtomicString("DIV"), kNsA);
  EXPECT_FALSE(html_names::kDivTag.Matches(foreign_div));
  EXPECT_NE(html_names::kDivTag.HashIgnoringPrefix(),
            foreign_div.HashIgnoringPrefix());

  EXPECT_FALSE(html_names::kATag.Matches(svg_names::kATag));
  EXPECT_NE(html_names::kATag.HashIgnoringPrefix(),
            svg_names::kATag.HashIgnoringPrefix());

  const QualifiedName ns_a_div(g_null_atom, AtomicString("div"), kNsA);
  const QualifiedName ns_b_div(g_null_atom, AtomicString("div"), kNsB);
  EXPECT_FALSE(ns_a_div.Matches(ns_b_div));
  EXPECT_NE(ns_a_div.HashIgnoringPrefix(), ns_b_div.HashIgnoringPrefix());
}

}  // namespace blink
