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
  const auto* children = node.GetChildren();
  if (children) {
    for (const auto& child : *children) {
      if (const auto* result = DepthFirstSearch(*child, f)) {
        return result;
      }
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
    const auto* node_name = node.GetName();
    return node_name && *node_name == name;
  });
}

base::Value Document::ToValueForTesting() const {
  return root_->ToValueForTesting();  // IN-TEST
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

const OwnedName* Node::GetName() const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->name;
}

const std::string* Node::GetLocalName() const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->name.local_name;
}

const std::string* Node::GetNamespacePrefix() const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->name.prefix;
}

const absl::flat_hash_map<OwnedName, std::string>* Node::GetAttributes() const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->attributes;
}

const std::string* Node::GetAttribute(Name name) const {
  const auto* attributes = GetAttributes();
  if (!attributes) {
    return nullptr;
  }
  return base::FindOrNull(*attributes, name);
}

const absl::flat_hash_map<std::string, std::string>* Node::GetNamespaces()
    const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->namespaces;
}

const std::vector<std::unique_ptr<Node>>* Node::GetChildren() const {
  const auto* element = std::get_if<Element>(&data_);
  if (!element) {
    return nullptr;
  }
  return &element->children;
}

std::vector<const Node*> Node::GetChildrenByTagName(Name name) const {
  std::vector<const Node*> result;
  const auto* children = GetChildren();
  if (!children) {
    return result;
  }
  for (const auto& child : *children) {
    const auto* child_name = child->GetName();
    if (child_name && *child_name == name) {
      result.push_back(child.get());
    }
  }
  return result;
}

const Node* Node::FindFirstChildByTagName(Name name) const {
  const auto* children = GetChildren();
  if (!children) {
    return nullptr;
  }
  for (const auto& child : *children) {
    const auto* child_name = child->GetName();
    if (child_name && *child_name == name) {
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

base::Value Node::ToValueForTesting() const {
  base::DictValue dict;
  switch (GetType()) {
    case Type::kElement: {
      dict.Set(mojom::XmlParser::kTypeKey, mojom::XmlParser::kElementType);
      const std::string* prefix = GetNamespacePrefix();
      const std::string* local_name = GetLocalName();
      std::string tag =
          prefix->empty() ? *local_name : *prefix + ":" + *local_name;
      dict.Set(mojom::XmlParser::kTagKey, std::move(tag));

      const auto* attributes = GetAttributes();
      if (!attributes->empty()) {
        base::DictValue attr_dict;
        for (const auto& [name, value] : *attributes) {
          std::string key = name.prefix.empty()
                                ? name.local_name
                                : name.prefix + ":" + name.local_name;
          attr_dict.Set(key, value);
        }
        dict.Set(mojom::XmlParser::kAttributesKey, std::move(attr_dict));
      }

      const auto* namespaces = GetNamespaces();
      if (!namespaces->empty()) {
        base::DictValue ns_dict;
        for (const auto& [ns_prefix, uri] : *namespaces) {
          ns_dict.Set(ns_prefix, uri);
        }
        dict.Set(mojom::XmlParser::kNamespacesKey, std::move(ns_dict));
      }

      const auto* children = GetChildren();
      if (!children->empty()) {
        base::ListValue children_list;
        for (const auto& child : *children) {
          children_list.Append(child->ToValueForTesting());  // IN-TEST
        }
        dict.Set(mojom::XmlParser::kChildrenKey, std::move(children_list));
      }
      break;
    }
    case Type::kText:
      dict.Set(mojom::XmlParser::kTypeKey, mojom::XmlParser::kTextNodeType);
      dict.Set(mojom::XmlParser::kTextKey, GetTextContent());
      break;
    case Type::kCdata:
      dict.Set(mojom::XmlParser::kTypeKey, mojom::XmlParser::kCDataNodeType);
      dict.Set(mojom::XmlParser::kTextKey, GetTextContent());
      break;
  }
  return base::Value(std::move(dict));
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
