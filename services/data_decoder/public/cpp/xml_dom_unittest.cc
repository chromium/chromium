// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/xml_dom.h"

#include <memory>
#include <optional>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Pointee;

namespace data_decoder::xml {

TEST(XmlDocumentTest, FindFirstElementByTagName) {
  {
    auto doc = Document::FromUtf8("<root />");
    ASSERT_TRUE(doc.has_value());

    const auto* node = doc->FindFirstElementByTagName(Name{"root"});
    ASSERT_TRUE(node);
    ASSERT_EQ(node->GetType(), Node::Type::kElement);
    EXPECT_EQ(node->GetName(), Name{"root"});

    EXPECT_EQ(doc->FindFirstElementByTagName(Name{"a"}), nullptr);
  }

  {
    auto doc =
        Document::FromUtf8("<root><a><b id='1'></b></a><b id='2' /></root>");
    ASSERT_TRUE(doc.has_value());

    const auto* a_node = doc->FindFirstElementByTagName(Name{"a"});
    ASSERT_TRUE(a_node);
    ASSERT_EQ(a_node->GetType(), Node::Type::kElement);
    EXPECT_EQ(a_node->GetName(), Name{"a"});

    const auto* b_node = doc->FindFirstElementByTagName(Name{"b"});
    ASSERT_TRUE(b_node);
    ASSERT_EQ(b_node->GetType(), Node::Type::kElement);
    EXPECT_THAT(b_node->GetAttribute(Name{"id"}), Pointee(Eq("1")));
  }

  {
    auto doc = Document::FromUtf8("<root><a id='1' /><a id='2' /></root>");
    ASSERT_TRUE(doc.has_value());

    const auto* node = doc->FindFirstElementByTagName(Name{"a"});
    ASSERT_TRUE(node);
    ASSERT_EQ(node->GetType(), Node::Type::kElement);
    EXPECT_THAT(node->GetAttribute(Name{"id"}), Pointee(Eq("1")));
  }
}

TEST(XmlNodeTest, GetChildrenByTagName) {
  {
    auto doc = Document::FromUtf8("<root><a id='1' /><a id='2' /></root>");
    ASSERT_TRUE(doc.has_value());

    const auto nodes = doc->GetRoot()->GetChildrenByTagName(Name{"a"});
    ASSERT_EQ(nodes.size(), 2u);
    ASSERT_EQ(nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_THAT(nodes[0]->GetAttribute(Name{"id"}), Pointee(Eq("1")));
    ASSERT_EQ(nodes[1]->GetType(), Node::Type::kElement);
    EXPECT_THAT(nodes[1]->GetAttribute(Name{"id"}), Pointee(Eq("2")));

    EXPECT_TRUE(doc->GetRoot()->GetChildrenByTagName(Name{"b"}).empty());
  }

  {
    auto doc = Document::FromUtf8(
        "<root><b /><a id='1' /><c /><a id='2' /><d /></root>");
    ASSERT_TRUE(doc.has_value());

    const auto a_nodes = doc->GetRoot()->GetChildrenByTagName(Name{"a"});
    ASSERT_EQ(a_nodes.size(), 2u);
    ASSERT_EQ(a_nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_THAT(a_nodes[0]->GetAttribute(Name{"id"}), Pointee(Eq("1")));
    ASSERT_EQ(a_nodes[1]->GetType(), Node::Type::kElement);
    EXPECT_THAT(a_nodes[1]->GetAttribute(Name{"id"}), Pointee(Eq("2")));

    const auto b_nodes = doc->GetRoot()->GetChildrenByTagName(Name{"b"});
    ASSERT_EQ(b_nodes.size(), 1u);
    ASSERT_EQ(b_nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(b_nodes[0]->GetName(), Name{"b"});

    const auto c_nodes = doc->GetRoot()->GetChildrenByTagName(Name{"c"});
    ASSERT_EQ(c_nodes.size(), 1u);
    ASSERT_EQ(c_nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(c_nodes[0]->GetName(), Name{"c"});

    const auto d_nodes = doc->GetRoot()->GetChildrenByTagName(Name{"d"});
    ASSERT_EQ(d_nodes.size(), 1u);
    ASSERT_EQ(d_nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_EQ(d_nodes[0]->GetName(), Name{"d"});
  }

  {
    auto doc = Document::FromUtf8("<root><a id='1'><a id='2'></a></a></root>");
    ASSERT_TRUE(doc.has_value());

    const auto nodes = doc->GetRoot()->GetChildrenByTagName(Name{"a"});
    ASSERT_EQ(nodes.size(), 1u);
    ASSERT_EQ(nodes[0]->GetType(), Node::Type::kElement);
    EXPECT_THAT(nodes[0]->GetAttribute(Name{"id"}), Pointee(Eq("1")));
  }
}

TEST(XmlNodeTest, FindFirstChildByTagName) {
  {
    auto doc = Document::FromUtf8("<root><a /><b /></root>");
    ASSERT_TRUE(doc.has_value());

    const auto* a_node = doc->GetRoot()->FindFirstChildByTagName(Name{"a"});
    ASSERT_TRUE(a_node);
    ASSERT_EQ(a_node->GetType(), Node::Type::kElement);
    EXPECT_EQ(a_node->GetName(), Name{"a"});

    const auto* b_node = doc->GetRoot()->FindFirstChildByTagName(Name{"b"});
    ASSERT_TRUE(b_node);
    ASSERT_EQ(b_node->GetType(), Node::Type::kElement);
    EXPECT_EQ(b_node->GetName(), Name{"b"});

    EXPECT_EQ(doc->GetRoot()->FindFirstChildByTagName(Name{"c"}), nullptr);
  }

  {
    auto doc =
        Document::FromUtf8("<root><b><a id='1' /></b><a id='2' /></root>");
    const auto* node = doc->GetRoot()->FindFirstChildByTagName(Name{"a"});
    ASSERT_TRUE(node);
    ASSERT_EQ(node->GetType(), Node::Type::kElement);
    // `FindFirstChildByTagName()` should only consider direct children.
    EXPECT_THAT(node->GetAttribute(Name{"id"}), Pointee(Eq("2")));
  }
}

}  // namespace data_decoder::xml
