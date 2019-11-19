// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/xml_parser.h"

#include <map>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace data_decoder {

using AttributeMap = std::map<std::string, std::string>;
using NamespaceMap = std::map<std::string, std::string>;

namespace {

void ReportError(XmlParser::ParseCallback callback, const std::string& error) {
  std::move(callback).Run(/*result=*/base::nullopt, base::make_optional(error));
}

enum class TextNodeType { kText, kCData };

// Returns false if the current node in |xml_reader| is not text or CData.
// Otherwise returns true and sets |text| to the text/CData of the current node
// and |node_type| to kText or kCData.
bool GetTextFromNode(XmlReader* xml_reader,
                     std::string* text,
                     TextNodeType* node_type) {
  if (xml_reader->GetTextIfTextElement(text)) {
    *node_type = TextNodeType::kText;
    return true;
  }
  if (xml_reader->GetTextIfCDataElement(text)) {
    *node_type = TextNodeType::kCData;
    return true;
  }
  return false;
}

base::Value CreateTextNode(const std::string& text, TextNodeType node_type) {
  base::Value element(base::Value::Type::DICTIONARY);
  element.SetKey(mojom::XmlParser::kTypeKey,
                 base::Value(node_type == TextNodeType::kText
                                 ? mojom::XmlParser::kTextNodeType
                                 : mojom::XmlParser::kCDataNodeType));
  element.SetKey(mojom::XmlParser::kTextKey, base::Value(text));
  return element;
}

// Creates and returns new element node with the tag name |name|.
base::Value CreateNewElement(const std::string& name) {
  base::Value element(base::Value::Type::DICTIONARY);
  element.SetKey(mojom::XmlParser::kTypeKey,
                 base::Value(mojom::XmlParser::kElementType));
  element.SetKey(mojom::XmlParser::kTagKey, base::Value(name));
  return element;
}

// Adds |child| as a child of |element|, creating the children list if
// necessary. Returns a ponter to |child|.
base::Value* AddChildToElement(base::Value* element, base::Value child) {
  DCHECK(element->is_dict());
  base::Value* children = element->FindKey(mojom::XmlParser::kChildrenKey);
  DCHECK(!children || children->is_list());
  if (!children)
    children = element->SetKey(mojom::XmlParser::kChildrenKey,
                               base::Value(base::Value::Type::LIST));
  children->Append(std::move(child));
  return &children->GetList().back();
}

void PopulateNamespaces(base::Value* node_value, XmlReader* xml_reader) {
  DCHECK(node_value->is_dict());
  NamespaceMap namespaces;
  if (!xml_reader->GetAllDeclaredNamespaces(&namespaces) || namespaces.empty())
    return;

  base::Value namespace_dict(base::Value::Type::DICTIONARY);
  for (auto ns : namespaces)
    namespace_dict.SetKey(ns.first, base::Value(ns.second));

  node_value->SetKey(mojom::XmlParser::kNamespacesKey,
                     std::move(namespace_dict));
}

void PopulateAttributes(base::Value* node_value, XmlReader* xml_reader) {
  DCHECK(node_value->is_dict());
  AttributeMap attributes;
  if (!xml_reader->GetAllNodeAttributes(&attributes) || attributes.empty())
    return;

  base::Value attribute_dict(base::Value::Type::DICTIONARY);
  for (auto attribute : attributes)
    attribute_dict.SetKey(attribute.first, base::Value(attribute.second));

  node_value->SetKey(mojom::XmlParser::kAttributesKey,
                     std::move(attribute_dict));
}

}  // namespace

XmlParser::XmlParser() = default;

XmlParser::~XmlParser() = default;

void XmlParser::Parse(const std::string& xml, ParseCallback callback) {
  XmlReader xml_reader;
  if (!xml_reader.Load(xml)) {
    ReportError(std::move(callback), "Invalid XML: failed to load");
    return;
  }

  base::Value root_element;
  std::vector<base::Value*> element_stack;
  while (xml_reader.Read()) {
    if (xml_reader.IsClosingElement()) {
      if (element_stack.empty()) {
        ReportError(std::move(callback), "Invalid XML: unbalanced elements");
        return;
      }
      element_stack.pop_back();
      continue;
    }

    std::string text;
    TextNodeType text_node_type = TextNodeType::kText;
    base::Value* current_element =
        element_stack.empty() ? nullptr : element_stack.back();
    bool push_new_node_to_stack = false;
    base::Value new_element;
    if (GetTextFromNode(&xml_reader, &text, &text_node_type)) {
      if (!base::IsStringUTF8(text)) {
        ReportError(std::move(callback), "Invalid XML: invalid UTF8 text.");
        return;
      }
      new_element = CreateTextNode(text, text_node_type);
    } else if (xml_reader.IsElement()) {
      new_element = CreateNewElement(xml_reader.NodeFullName());
      PopulateNamespaces(&new_element, &xml_reader);
      PopulateAttributes(&new_element, &xml_reader);
      // Self-closing (empty) element have no close tag (or children); don't
      // push them on the element stack.
      push_new_node_to_stack = !xml_reader.IsEmptyElement();
    } else {
      // Ignore all other node types (spaces, comments, processing instructions,
      // DTDs...).
      continue;
    }

    base::Value* new_element_ptr;
    if (current_element) {
      new_element_ptr =
          AddChildToElement(current_element, std::move(new_element));
    } else {
      // First element we are parsing, it becomes the root element.
      DCHECK(xml_reader.IsElement());
      DCHECK(root_element.is_none());
      root_element = std::move(new_element);
      new_element_ptr = &root_element;
    }
    if (push_new_node_to_stack)
      element_stack.push_back(new_element_ptr);
  }

  if (!element_stack.empty()) {
    ReportError(std::move(callback), "Invalid XML: unbalanced elements");
    return;
  }
  base::DictionaryValue* dictionary = nullptr;
  root_element.GetAsDictionary(&dictionary);
  if (!dictionary || dictionary->empty()) {
    ReportError(std::move(callback), "Invalid XML: bad content");
    return;
  }
  std::move(callback).Run(base::make_optional(std::move(root_element)),
                          base::Optional<std::string>());
}

}  // namespace data_decoder
