// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/xml_dom.h"

#include <utility>
#include <variant>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace data_decoder::xml {

namespace {

template <typename F>
const Node* DepthFirstSearch(const Node& node, const F& f) {
  if (f(node)) {
    return &node;
  }
  if (node.GetType() != Node::Type::kElement) {
    return nullptr;
  }
  for (const auto& child : node.GetChildren()) {
    if (const auto* result = DepthFirstSearch(*child, f)) {
      return result;
    }
  }
  return nullptr;
}

}  // namespace

Document::Document(std::unique_ptr<Node> root) : root_(std::move(root)) {
  CHECK(root_);
}

Document::~Document() = default;
Document::Document(Document&&) = default;
Document& Document::operator=(Document&&) = default;

const Node* Document::GetRoot() const {
  return root_.get();
}

const Node* Document::FindFirstElementByTagName(Name name) const {
  if (!root_) {
    return nullptr;
  }

  return DepthFirstSearch(*root_, [&](const Node& node) {
    return node.GetType() == Node::Type::kElement && node.GetName() == name;
  });
}

Node::Element::Element() = default;
Node::Element::Element(OwnedName name) : name(std::move(name)) {}
Node::Element::Element(Element&&) = default;
Node::Element& Node::Element::operator=(Element&&) = default;
Node::Element::~Element() = default;

Node::~Node() = default;

Node::Type Node::GetType() const {
  return std::visit(
      absl::Overload{[](const Element&) { return Type::kElement; },
                     [](const Text&) { return Type::kText; },
                     [](const Cdata&) { return Type::kCdata; }},
      data_);
}

const OwnedName& Node::GetName() const {
  return std::get<Element>(data_).name;
}
const std::string& Node::GetLocalName() const {
  return GetName().local_name;
}

const std::string& Node::GetNamespacePrefix() const {
  return GetName().prefix;
}

const absl::flat_hash_map<OwnedName, std::string>& Node::GetAttributes() const {
  return std::get<Element>(data_).attributes;
}

const std::string* Node::GetAttribute(Name name) const {
  return base::FindOrNull(GetAttributes(), name);
}

const absl::flat_hash_map<std::string, std::string>& Node::GetNamespaces()
    const {
  return std::get<Element>(data_).namespaces;
}

const std::vector<std::unique_ptr<Node>>& Node::GetChildren() const {
  return std::get<Element>(data_).children;
}

std::vector<const Node*> Node::GetChildrenByTagName(Name name) const {
  std::vector<const Node*> result;
  for (const auto& child : GetChildren()) {
    if (child->GetType() == Type::kElement && child->GetName() == name) {
      result.push_back(child.get());
    }
  }
  return result;
}

const Node* Node::FindFirstChildByTagName(Name name) const {
  for (const auto& child : GetChildren()) {
    if (child->GetType() == Type::kElement && child->GetName() == name) {
      return child.get();
    }
  }
  return nullptr;
}

const std::string& Node::GetTextContent() const {
  return std::visit(
      absl::Overload{
          [](const Element&) -> const std::string& {
            NOTREACHED()
                << "GetTextContent() cannot be called on element nodes";
          },
          [](const auto& node) -> const std::string& { return node.text; },
      },
      data_);
}

// static
std::unique_ptr<Node> Node::CreateElement(
    base::PassKey<ffi::DomBuilder> pass_key,
    std::string local_name,
    std::string prefix) {
  return base::WrapUnique(
      new Node(Element(OwnedName{std::move(local_name), std::move(prefix)})));
}

// static
std::unique_ptr<Node> Node::CreateTextNode(
    base::PassKey<ffi::DomBuilder> pass_key,
    std::string text) {
  return base::WrapUnique(new Node(Text{std::move(text)}));
}

// static
std::unique_ptr<Node> Node::CreateCdataNode(
    base::PassKey<ffi::DomBuilder> pass_key,
    std::string text) {
  return base::WrapUnique(new Node(Cdata{std::move(text)}));
}

void Node::SetAttribute(base::PassKey<ffi::DomBuilder>,
                        std::string local_name,
                        std::string prefix,
                        std::string value) {
  std::get<Element>(data_)
      .attributes[OwnedName{std::move(local_name), std::move(prefix)}] =
      std::move(value);
}

void Node::SetNamespace(base::PassKey<ffi::DomBuilder>,
                        std::string prefix,
                        std::string uri) {
  std::get<Element>(data_).namespaces[std::move(prefix)] = std::move(uri);
}

void Node::AddChild(base::PassKey<ffi::DomBuilder>,
                    std::unique_ptr<Node> child) {
  DCHECK(child);
  DCHECK(!child->parent_);
  child->parent_ = this;
  std::get<Element>(data_).children.push_back(std::move(child));
}

}  // namespace data_decoder::xml
