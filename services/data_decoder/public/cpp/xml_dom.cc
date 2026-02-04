// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/xml_dom.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"

// -- XmlNode --

XmlNode::ElementData::ElementData() = default;
XmlNode::ElementData::ElementData(ElementData&&) = default;
XmlNode::ElementData& XmlNode::ElementData::operator=(ElementData&&) = default;
XmlNode::ElementData::~ElementData() = default;

// Private Constructor
XmlNode::XmlNode(base::PassKey<XmlDocument>,
                 Type type,
                 const std::string& tag_name,
                 const std::string& text_content)
    : parent_(nullptr) {
  if (type == Type::kELEMENT) {
    data_ = ElementData();
    std::get<ElementData>(data_).tag = tag_name;
  } else {
    data_ = text_content;
  }
}

XmlNode::~XmlNode() = default;

XmlNode::Type XmlNode::GetType() const {
  return std::holds_alternative<ElementData>(data_) ? Type::kELEMENT
                                                    : Type::kTEXT;
}

const std::string& XmlNode::GetTagName() const {
  DCHECK(GetType() == Type::kELEMENT);
  return std::get<ElementData>(data_).tag;
}

const std::string& XmlNode::GetTextContent() const {
  DCHECK(GetType() == Type::kTEXT);
  return std::get<std::string>(data_);
}

std::optional<std::string> XmlNode::GetAttribute(std::string_view name) const {
  DCHECK(GetType() == Type::kELEMENT);
  const auto& attributes = std::get<ElementData>(data_).attributes;
  auto it = attributes.find(std::string(name));
  if (it != attributes.end()) {
    return it->second;
  }
  return std::nullopt;
}

const std::vector<std::unique_ptr<XmlNode>>& XmlNode::GetChildren() const {
  CHECK(GetType() == Type::kELEMENT)
      << "GetChildren can only be called on ELEMENT nodes";
  return std::get<ElementData>(data_).children;
}

const XmlNode* XmlNode::GetParent() const {
  return parent_;
}

std::vector<const XmlNode*> XmlNode::GetChildrenByTagName(
    std::string_view tag_name) const {
  CHECK(GetType() == Type::kELEMENT)
      << "GetChildrenByTagName can only be called on ELEMENT nodes";
  std::vector<const XmlNode*> result;
  const auto& children = std::get<ElementData>(data_).children;
  for (const auto& child : children) {
    if (child->GetType() == Type::kELEMENT && child->GetTagName() == tag_name) {
      result.push_back(child.get());
    }
  }
  return result;
}

const XmlNode* XmlNode::GetFirstChildByTagName(
    std::string_view tag_name) const {
  CHECK(GetType() == Type::kELEMENT)
      << "GetFirstChildByTagName can only be called on ELEMENT nodes";
  for (const auto& child : std::get<ElementData>(data_).children) {
    if (child->GetTagName() == tag_name) {
      return child.get();
    }
  }
  return nullptr;
}

void XmlNode::AddChildForTesting(std::unique_ptr<XmlNode> child) {
  CHECK(GetType() == Type::kELEMENT)
      << "AddChildForTesting can only be called on ELEMENT nodes";
  DCHECK(child);
  DCHECK(!child->parent_);
  child->parent_ = this;
  std::get<ElementData>(data_).children.push_back(std::move(child));
}

void XmlNode::SetAttributeForTesting(const std::string& name,
                                     const std::string& value) {
  CHECK(GetType() == Type::kELEMENT)
      << "SetAttributeForTesting can only be called on ELEMENT nodes";
  std::get<ElementData>(data_).attributes[name] = value;
}

XmlDocument::XmlDocument() : root_(nullptr) {}

XmlDocument::~XmlDocument() = default;

XmlDocument::XmlDocument(XmlDocument&&) = default;
XmlDocument& XmlDocument::operator=(XmlDocument&&) = default;

const XmlNode* XmlDocument::GetRoot() const {
  return root_.get();
}

const XmlNode* XmlDocument::FindFirstElementByTagName(
    std::string_view tag_name) const {
  if (!root_) {
    return nullptr;
  }
  return FindFirstElementByTagNameRecursive(root_.get(), tag_name);
}

const XmlNode* XmlDocument::FindFirstElementByTagNameRecursive(
    const XmlNode* node,
    std::string_view tag_name) const {
  if (node->GetType() == XmlNode::Type::kELEMENT) {
    if (node->GetTagName() == tag_name) {
      return node;
    }
    for (const auto& child : node->GetChildren()) {
      const XmlNode* found =
          FindFirstElementByTagNameRecursive(child.get(), tag_name);
      if (found) {
        return found;
      }
    }
  }
  return nullptr;
}

void XmlDocument::SetRootForTesting(std::unique_ptr<XmlNode> root) {
  root_ = std::move(root);
}

// static
std::unique_ptr<XmlNode> XmlDocument::CreateElementNodeForTesting(
    const std::string& tag_name) {
  return base::WrapUnique(new XmlNode(base::PassKey<XmlDocument>(),
                                      XmlNode::Type::kELEMENT, tag_name,
                                      /*text_content=*/""));
}

// static
std::unique_ptr<XmlNode> XmlDocument::CreateTextNodeForTesting(
    const std::string& text_content) {
  return base::WrapUnique(new XmlNode(base::PassKey<XmlDocument>(),
                                      XmlNode::Type::kTEXT, /*tag_name=*/"",
                                      text_content));
}
