// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/xml_dom.h"

#include <memory>
#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

class XmlDomTest : public ::testing::Test {
 public:
  // Helper functions to access the static testing methods on XmlDocument
  static std::unique_ptr<XmlNode> CreateElementNode(
      const std::string& tag_name) {
    return XmlDocument::CreateElementNodeForTesting(tag_name);
  }

  static std::unique_ptr<XmlNode> CreateTextNode(
      const std::string& text_content) {
    return XmlDocument::CreateTextNodeForTesting(text_content);
  }
};

namespace {

TEST_F(XmlDomTest, ElementNodeCreation) {
  auto node = CreateElementNode("myTag");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetType(), XmlNode::Type::kELEMENT);
  EXPECT_EQ(node->GetTagName(), "myTag");
  EXPECT_EQ(node->GetParent(), nullptr);
  EXPECT_TRUE(node->GetChildren().empty());
}

TEST_F(XmlDomTest, TextNodeCreation) {
  auto node = CreateTextNode("some text");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetType(), XmlNode::Type::kTEXT);
  EXPECT_EQ(node->GetTextContent(), "some text");
  EXPECT_EQ(node->GetParent(), nullptr);
}

TEST_F(XmlDomTest, Attributes) {
  auto node = CreateElementNode("a");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetAttribute("href"), std::nullopt);

  node->SetAttributeForTesting("href", "https://example.com");
  node->SetAttributeForTesting("target", "_blank");

  EXPECT_EQ(node->GetAttribute("href"), "https://example.com");
  EXPECT_EQ(node->GetAttribute("target"), "_blank");
  EXPECT_EQ(node->GetAttribute("class"), std::nullopt);
}

TEST_F(XmlDomTest, ChildrenAndParent) {
  auto root = CreateElementNode("root");
  auto child1 = CreateElementNode("child1");
  XmlNode* child1_ptr = child1.get();
  auto child2 = CreateElementNode("child2");
  XmlNode* child2_ptr = child2.get();

  root->AddChildForTesting(std::move(child1));
  root->AddChildForTesting(std::move(child2));

  ASSERT_EQ(root->GetChildren().size(), static_cast<size_t>(2));
  EXPECT_EQ(child1_ptr->GetParent(), root.get());
  EXPECT_EQ(child2_ptr->GetParent(), root.get());

  EXPECT_EQ(root->GetChildren()[0].get(), child1_ptr);
  EXPECT_EQ(root->GetChildren()[1].get(), child2_ptr);
}

TEST_F(XmlDomTest, GetChildrenByTagName) {
  auto root = CreateElementNode("root");
  root->AddChildForTesting(CreateElementNode("a"));
  root->AddChildForTesting(CreateElementNode("b"));
  root->AddChildForTesting(CreateElementNode("a"));

  auto a_nodes = root->GetChildrenByTagName("a");
  ASSERT_EQ(a_nodes.size(), static_cast<size_t>(2));
  EXPECT_EQ(a_nodes[0]->GetTagName(), "a");
  EXPECT_EQ(a_nodes[1]->GetTagName(), "a");

  auto b_nodes = root->GetChildrenByTagName("b");
  ASSERT_EQ(b_nodes.size(), static_cast<size_t>(1));
  EXPECT_EQ(b_nodes[0]->GetTagName(), "b");

  auto c_nodes = root->GetChildrenByTagName("c");
  EXPECT_TRUE(c_nodes.empty());
}

TEST_F(XmlDomTest, GetFirstChildByTagName) {
  auto root = CreateElementNode("root");
  auto first_a = CreateElementNode("a");
  XmlNode* first_a_ptr = first_a.get();
  root->AddChildForTesting(std::move(first_a));
  root->AddChildForTesting(CreateElementNode("b"));
  root->AddChildForTesting(CreateElementNode("a"));

  const XmlNode* found_a = root->GetFirstChildByTagName("a");
  ASSERT_NE(found_a, nullptr);
  EXPECT_EQ(found_a->GetTagName(), "a");
  EXPECT_EQ(found_a, first_a_ptr);

  const XmlNode* found_b = root->GetFirstChildByTagName("b");
  ASSERT_NE(found_b, nullptr);
  EXPECT_EQ(found_b->GetTagName(), "b");

  EXPECT_EQ(root->GetFirstChildByTagName("c"), nullptr);
}

std::unique_ptr<XmlDocument> CreateTestDocument() {
  auto doc = std::make_unique<XmlDocument>();
  auto root = XmlDocument::CreateElementNodeForTesting("root");
  XmlNode* root_ptr = root.get();

  auto item1 = XmlDocument::CreateElementNodeForTesting("item");
  item1->SetAttributeForTesting("id", "1");
  item1->AddChildForTesting(XmlDocument::CreateTextNodeForTesting("Text1"));
  root_ptr->AddChildForTesting(std::move(item1));

  auto item2 = XmlDocument::CreateElementNodeForTesting("item");
  item2->SetAttributeForTesting("id", "2");
  item2->AddChildForTesting(XmlDocument::CreateTextNodeForTesting("Text2"));
  root_ptr->AddChildForTesting(std::move(item2));

  auto container = XmlDocument::CreateElementNodeForTesting("container");
  XmlNode* container_ptr = container.get();
  auto item3 = XmlDocument::CreateElementNodeForTesting("item");
  item3->SetAttributeForTesting("id", "3");
  item3->AddChildForTesting(XmlDocument::CreateTextNodeForTesting("Text3"));
  container_ptr->AddChildForTesting(std::move(item3));
  root_ptr->AddChildForTesting(std::move(container));

  doc->SetRootForTesting(std::move(root));
  return doc;
}

TEST_F(XmlDomTest, GetRoot) {
  auto doc = CreateTestDocument();
  const XmlNode* root = doc->GetRoot();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetTagName(), "root");
}

TEST_F(XmlDomTest, FindFirstElementByTagName) {
  auto doc = CreateTestDocument();

  const XmlNode* item = doc->FindFirstElementByTagName("item");
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->GetTagName(), "item");
  EXPECT_EQ(item->GetAttribute("id"), "1");

  const XmlNode* container = doc->FindFirstElementByTagName("container");
  ASSERT_NE(container, nullptr);
  EXPECT_EQ(container->GetTagName(), "container");

  const XmlNode* root = doc->FindFirstElementByTagName("root");
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetTagName(), "root");

  EXPECT_EQ(doc->FindFirstElementByTagName("nonexistent"), nullptr);
}

TEST_F(XmlDomTest, FindFirstElementByTagNameDeep) {
  auto doc = CreateTestDocument();
  const XmlNode* item = doc->FindFirstElementByTagName("item");
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->GetAttribute("id"), "1");
}

}  // namespace
}  // namespace data_decoder
