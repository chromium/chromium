// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor_hash.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;

TEST(CustomElementDescriptorTest, equal) {
  CustomElementDescriptor my_type_extension("my-button", "button");
  CustomElementDescriptor again("my-button", "button");
  EXPECT_TRUE(my_type_extension == again)
      << "two descriptors with the same name and local name should be equal";
}

TEST(CustomElementDescriptorTest, notEqual) {
  CustomElementDescriptor my_type_extension("my-button", "button");
  CustomElementDescriptor colliding_new_type("my-button", "my-button");
  EXPECT_FALSE(my_type_extension == colliding_new_type)
      << "type extension should not be equal to a non-type extension";
}

TEST(CustomElementDescriptorTest, hashable) {
  HashSet<CustomElementDescriptor> descriptors;
  descriptors.insert(CustomElementDescriptor("foo-bar", "foo-bar"));
  EXPECT_TRUE(
      descriptors.Contains(CustomElementDescriptor("foo-bar", "foo-bar")))
      << "the identical descriptor should be found in the hash set";
  EXPECT_FALSE(
      descriptors.Contains(CustomElementDescriptor("bad-poetry", "blockquote")))
      << "an unrelated descriptor should not be found in the hash set";
}

TEST(CustomElementDescriptorTest, matches_autonomous) {
  CustomElementDescriptor descriptor("a-b", "a-b");
  Element* element = CreateElement("a-b");
  EXPECT_TRUE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_autonomous_shouldNotMatchCustomizedBuiltInElement) {
  CustomElementDescriptor descriptor("a-b", "a-b");
  Element* element = CreateElement("futuretag").WithIsValue("a-b");
  EXPECT_FALSE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest, matches_customizedBuiltIn) {
  CustomElementDescriptor descriptor("a-b", "button");
  Element* element = CreateElement("button").WithIsValue("a-b");
  EXPECT_TRUE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_customizedBuiltIn_shouldNotMatchAutonomousElement) {
  CustomElementDescriptor descriptor("a-b", "button");
  Element* element = CreateElement("a-b");
  EXPECT_FALSE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_elementNotInHTMLNamespaceDoesNotMatch) {
  CustomElementDescriptor descriptor("a-b", "a-b");
  Element* element = CreateElement("a-b").InNamespace("data:text/plain,foo");
  EXPECT_FALSE(descriptor.Matches(*element));
}

}  // namespace blink
