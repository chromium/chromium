// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/safe_xml_parser.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

namespace data_decoder {

const base::Value::List* GetXmlElementChildren(const base::Value& element) {
  if (!element.is_dict())
    return nullptr;
  return element.GetDict().FindList(mojom::XmlParser::kChildrenKey);
}

std::string GetXmlQualifiedName(const std::string& name_space,
                                const std::string& name) {
  return name_space.empty() ? name : name_space + ":" + name;
}

bool IsXmlElementNamed(const base::Value& element, const std::string& name) {
  if (!element.is_dict())
    return false;
  const std::string* tag_text =
      element.GetDict().FindString(mojom::XmlParser::kTagKey);
  return tag_text ? *tag_text == name : false;
}

bool IsXmlElementOfType(const base::Value& element, const std::string& type) {
  if (!element.is_dict())
    return false;
  const std::string* type_text =
      element.GetDict().FindString(mojom::XmlParser::kTypeKey);
  return type_text ? *type_text == type : false;
}

bool GetXmlElementTagName(const base::Value& element, std::string* tag_name) {
  DCHECK(tag_name);
  if (!element.is_dict())
    return false;
  const std::string* tag_text =
      element.GetDict().FindString(mojom::XmlParser::kTagKey);
  if (!tag_text)
    return false;
  *tag_name = *tag_text;
  return true;
}

bool GetXmlElementText(const base::Value& element, std::string* text) {
  DCHECK(text);
  const base::Value::List* children = GetXmlElementChildren(element);
  if (!children)
    return false;

  const base::Value* text_node = nullptr;
  for (const base::Value& value : *children) {
    if (IsXmlElementOfType(value, mojom::XmlParser::kTextNodeType) ||
        IsXmlElementOfType(value, mojom::XmlParser::kCDataNodeType)) {
      text_node = &value;
      break;
    }
  }
  if (!text_node || !text_node->is_dict()) {
    return false;
  }

  const std::string* text_value =
      text_node->GetDict().FindString(mojom::XmlParser::kTextKey);
  *text = text_value ? *text_value : "";
  return true;
}

bool GetXmlElementNamespacePrefix(const base::Value& element,
                                  const std::string& namespace_uri,
                                  std::string* prefix) {
  if (!element.is_dict()) {
    return false;
  }

  prefix->clear();
  const base::Value::Dict* namespaces =
      element.GetDict().FindDict(mojom::XmlParser::kNamespacesKey);
  if (!namespaces)
    return false;

  // The namespaces dictionary is prefix -> URI, so we have to do a reverse
  // lookup.
  for (auto item : *namespaces) {
    if (item.second.GetString() == namespace_uri) {
      *prefix = item.first;
      return true;
    }
  }
  return false;
}

int GetXmlElementChildrenCount(const base::Value& element,
                               const std::string& name) {
  const base::Value::List* children = GetXmlElementChildren(element);
  if (!children)
    return 0;
  int child_count = 0;
  for (const base::Value& value : *children) {
    DCHECK(value.is_dict());
    std::string tag_name;
    bool success = GetXmlElementTagName(value, &tag_name);
    if (success && tag_name == name)
      child_count++;
  }
  return child_count;
}

const base::Value* GetXmlElementChildWithType(const base::Value& element,
                                              const std::string& type) {
  const base::Value::List* children = GetXmlElementChildren(element);
  if (!children)
    return nullptr;
  for (const base::Value& value : *children) {
    DCHECK(value.is_dict());
    if (IsXmlElementOfType(value, type)) {
      return &value;
    }
  }
  return nullptr;
}

const base::Value* GetXmlElementChildWithTag(const base::Value& element,
                                             const std::string& tag) {
  const base::Value::List* children = GetXmlElementChildren(element);
  if (!children)
    return nullptr;
  for (const base::Value& value : *children) {
    DCHECK(value.is_dict());
    if (IsXmlElementNamed(value, tag))
      return &value;
  }
  return nullptr;
}

bool GetAllXmlElementChildrenWithTag(
    const base::Value& element,
    const std::string& tag,
    std::vector<const base::Value*>* children_out) {
  const base::Value::List* children = GetXmlElementChildren(element);
  if (!children)
    return false;
  bool found = false;
  for (const base::Value& child : *children) {
    DCHECK(child.is_dict());
    if (IsXmlElementNamed(child, tag)) {
      found = true;
      children_out->push_back(&child);
    }
  }
  return found;
}

const base::Value* FindXmlElementPath(
    const base::Value& element,
    std::initializer_list<std::string_view> path,
    bool* unique_path) {
  const base::Value* cur = nullptr;
  if (unique_path)
    *unique_path = true;

  for (const std::string_view component_piece : path) {
    std::string component(component_piece);
    if (!cur) {
      // First element has to match the current node.
      if (!IsXmlElementNamed(element, component))
        return nullptr;
      cur = &element;
      continue;
    }

    const base::Value* new_cur = GetXmlElementChildWithTag(*cur, component);
    if (!new_cur)
      return nullptr;

    if (unique_path && *unique_path &&
        GetXmlElementChildrenCount(*cur, component) > 1)
      *unique_path = false;

    cur = new_cur;
  }
  return cur;
}

std::string GetXmlElementAttribute(const base::Value& element,
                                   const std::string& element_name) {
  if (!element.is_dict())
    return "";

  const base::Value::Dict* attributes =
      element.GetDict().FindDict(mojom::XmlParser::kAttributesKey);
  if (!attributes)
    return "";

  const std::string* value = attributes->FindString(element_name);
  return value ? *value : "";
}

}  // namespace data_decoder
