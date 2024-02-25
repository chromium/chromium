// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor_hash.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;

TEST(CustomElementDescriptorTest, equal) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor my_type_extension(AtomicString("my-button"),
                                            AtomicString("button"));
  CustomElementDescriptor again(AtomicString("my-button"),
                                AtomicString("button"));
  EXPECT_TRUE(my_type_extension == again)
      << "two descriptors with the same name and local name should be equal";
}

TEST(CustomElementDescriptorTest, notEqual) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor my_type_extension(AtomicString("my-button"),
                                            AtomicString("button"));
  CustomElementDescriptor colliding_new_type(AtomicString("my-button"),
                                             AtomicString("my-button"));
  EXPECT_FALSE(my_type_extension == colliding_new_type)
      << "type extension should not be equal to a non-type extension";
}

TEST(CustomElementDescriptorTest, hashable) {
  test::TaskEnvironment task_environment;
  HashSet<CustomElementDescriptor> descriptors;
  descriptors.insert(CustomElementDescriptor(AtomicString("foo-bar"),
                                             AtomicString("foo-bar")));
  EXPECT_TRUE(descriptors.Contains(CustomElementDescriptor(
      AtomicString("foo-bar"), AtomicString("foo-bar"))))
      << "the identical descriptor should be found in the hash set";
  EXPECT_FALSE(descriptors.Contains(CustomElementDescriptor(
      AtomicString("bad-poetry"), AtomicString("blockquote"))))
      << "an unrelated descriptor should not be found in the hash set";
}

TEST(CustomElementDescriptorTest, matches_autonomous) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor descriptor(AtomicString("a-b"), AtomicString("a-b"));
  Element* element = CreateElement(AtomicString("a-b"));
  EXPECT_TRUE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_autonomous_shouldNotMatchCustomizedBuiltInElement) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor descriptor(AtomicString("a-b"), AtomicString("a-b"));
  Element* element =
      CreateElement(AtomicString("futuretag")).WithIsValue(AtomicString("a-b"));
  EXPECT_FALSE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest, matches_customizedBuiltIn) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor descriptor(AtomicString("a-b"),
                                     AtomicString("button"));
  Element* element =
      CreateElement(AtomicString("button")).WithIsValue(AtomicString("a-b"));
  EXPECT_TRUE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_customizedBuiltIn_shouldNotMatchAutonomousElement) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor descriptor(AtomicString("a-b"),
                                     AtomicString("button"));
  Element* element = CreateElement(AtomicString("a-b"));
  EXPECT_FALSE(descriptor.Matches(*element));
}

TEST(CustomElementDescriptorTest,
     matches_elementNotInHTMLNamespaceDoesNotMatch) {
  test::TaskEnvironment task_environment;
  CustomElementDescriptor descriptor(AtomicString("a-b"), AtomicString("a-b"));
  Element* element = CreateElement(AtomicString("a-b"))
                         .InNamespace(AtomicString("data:text/plain,foo"));
  EXPECT_FALSE(descriptor.Matches(*element));
}

}  // namespace blink
