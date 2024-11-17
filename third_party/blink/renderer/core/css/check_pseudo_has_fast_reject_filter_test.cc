// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_fast_reject_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CheckPseudoHasFastRejectFilterTest : public PageTestBase {
 protected:
  struct ElementInfo {
    const char* tag_name;
    const char* id;
    const char* class_names;
    const char* attribute_name;
    const char* attribute_value;
  };

  void AddElementIdentifierHashes(
      CheckPseudoHasFastRejectFilter& filter,
      const base::span<const ElementInfo> element_info_list) {
    for (const ElementInfo& element_info : element_info_list) {
      NonThrowableExceptionState no_exceptions;
      Element* element = GetDocument().CreateElementForBinding(
          AtomicString(element_info.tag_name), nullptr, no_exceptions);
      element->setAttribute(html_names::kIdAttr, AtomicString(element_info.id));
      element->setAttribute(html_names::kClassAttr,
                            AtomicString(element_info.class_names));
      element->setAttribute(AtomicString(element_info.attribute_name),
                            AtomicString(element_info.attribute_value));
      filter.AddElementIdentifierHashes(*element);
    }
  }

  bool CheckFastReject(CheckPseudoHasFastRejectFilter& filter,
                       const char* selector_text) {
    CSSSelectorList* selector_list =
        css_test_helpers::ParseSelectorList(selector_text);

    EXPECT_EQ(selector_list->First()->GetPseudoType(), CSSSelector::kPseudoHas);

    CheckPseudoHasArgumentContext context(
        selector_list->First()->SelectorList()->First(),
        /* match_in_shadow_tree */ false);

    return filter.FastReject(context.GetPseudoHasArgumentHashes());
  }
};

TEST_F(CheckPseudoHasFastRejectFilterTest, CheckFastReject) {
  CheckPseudoHasFastRejectFilter filter;

  EXPECT_FALSE(filter.BloomFilterAllocated());
  filter.AllocateBloomFilter();
  EXPECT_TRUE(filter.BloomFilterAllocated());

  const ElementInfo element_infos[] = {
      {/* tag_name */ "div", /* id */ "d1", /* class_names */ "a",
       /* attribute_name */ "attr1", /* attribute_value */ "val1"},
      {/* tag_name */ "div", /* id */ "d2", /* class_names */ "b",
       /* attribute_name */ "attr2", /* attribute_value */ "val2"},
      {/* tag_name */ "span", /* id */ "s1", /* class_names */ "c",
       /* attribute_name */ "attr3", /* attribute_value */ "val3"},
      {/* tag_name */ "span", /* id */ "s2", /* class_names */ "d",
       /* attribute_name */ "attr4", /* attribute_value */ "val4"}};
  AddElementIdentifierHashes(filter, element_infos);

  EXPECT_FALSE(CheckFastReject(filter, ":has(div)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(span)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(h1)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(#div)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(.div)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([div])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([div=div])"));

  EXPECT_FALSE(CheckFastReject(filter, ":has(#d1)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(#d2)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(#d3)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(#s1)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(#s2)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(#s3)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(d1)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(.d1)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([d1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([d1=d1])"));

  EXPECT_FALSE(CheckFastReject(filter, ":has(.a)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(.b)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(.c)"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(.d)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(.e)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(a)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(#a)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([a])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has([a=a])"));

  EXPECT_FALSE(CheckFastReject(filter, ":has([attr1])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr2])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr3])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr4])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr1=x])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr2=x])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr3=x])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has([attr4=x])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(attr1)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(#attr1)"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(.attr1)"));

  EXPECT_FALSE(CheckFastReject(filter, ":has(div#d1.a[attr1=val1])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(span#d1.a[attr1=val1])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(div#s1.a[attr1=val1])"));
  EXPECT_FALSE(CheckFastReject(filter, ":has(div#d1.c[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(h1#d1.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d3.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.e[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.a[attr5=val1])"));

  EXPECT_TRUE(CheckFastReject(filter, ":has(div#div.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.div[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.a[div=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(d1#d1.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.d1[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.a[d1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(a#d1.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#a.a[attr1=val1])"));
  EXPECT_TRUE(CheckFastReject(filter, ":has(div#d1.a[a=val1])"));
}

}  // namespace blink
