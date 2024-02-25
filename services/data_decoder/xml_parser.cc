// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/xml_parser.h"

#include <map>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace data_decoder {

using AttributeMap = std::map<std::string, std::string>;
using NamespaceMap = std::map<std::string, std::string>;

namespace {

void ReportError(XmlParser::ParseCallback callback,
                 const std::string& generic_error,
                 const std::string& libxml_error) {
  std::string error;
  if (!libxml_error.empty()) {
    error = base::StrCat({generic_error, ": ", libxml_error});
    // libxml errors have trailing lines, spaces, and a carrot to try and
    // indicate where an error is. For instance, an error string may be:
    // Entity: line 1: parser error : Opening and ending tag mismatch: hello
    // line 1 and goodbye
    // <hello>bad tag</goodbye>
    //                         ^
    // This is helpful in a terminal, but not when gathering and returning the
    // error. Instead, just trim the trailing whitespace and '^'.
    base::TrimString(error, " \n^", &error);
  } else {
    error = generic_error;
  }

  std::move(callback).Run(/*result=*/std::nullopt,
                          std::make_optional(std::move(error)));
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

base::Value::Dict CreateTextNode(const std::string& text,
                                 TextNodeType node_type) {
  base::Value::Dict element;
  element.Set(mojom::XmlParser::kTypeKey,
              node_type == TextNodeType::kText
                  ? mojom::XmlParser::kTextNodeType
                  : mojom::XmlParser::kCDataNodeType);
  element.Set(mojom::XmlParser::kTextKey, text);
  return element;
}

// Creates and returns new element node with the tag name |name|.
base::Value::Dict CreateNewElement(const std::string& name) {
  base::Value::Dict element;
  element.Set(mojom::XmlParser::kTypeKey, mojom::XmlParser::kElementType);
  element.Set(mojom::XmlParser::kTagKey, name);
  return element;
}

// Adds |child| as a child of |element|, creating the children list if
// necessary. Returns a ponter to |child|.
base::Value::Dict* AddChildToElement(base::Value::Dict& element,
                                     base::Value::Dict child) {
  DCHECK(!element.contains(mojom::XmlParser::kChildrenKey) ||
         element.FindList(mojom::XmlParser::kChildrenKey));
  base::Value::List* children =
      element.EnsureList(mojom::XmlParser::kChildrenKey);
  children->Append(std::move(child));
  return &children->back().GetDict();
}

void PopulateNamespaces(base::Value::Dict& node_value, XmlReader* xml_reader) {
  NamespaceMap namespaces;
  if (!xml_reader->GetAllDeclaredNamespaces(&namespaces) || namespaces.empty())
    return;

  base::Value::Dict namespace_dict;
  for (auto ns : namespaces)
    namespace_dict.Set(ns.first, ns.second);

  node_value.Set(mojom::XmlParser::kNamespacesKey, std::move(namespace_dict));
}

void PopulateAttributes(base::Value::Dict& node_value, XmlReader* xml_reader) {
  AttributeMap attributes;
  if (!xml_reader->GetAllNodeAttributes(&attributes) || attributes.empty())
    return;

  base::Value::Dict attribute_dict;
  for (auto attribute : attributes)
    attribute_dict.Set(attribute.first, base::Value(attribute.second));

  node_value.Set(mojom::XmlParser::kAttributesKey, std::move(attribute_dict));
}

// A function to capture XML errors. Otherwise, by default, they are printed to
// stderr. `context` is a pointer to a std::string stack-allocated in the
// Parse(); `message` and the subsequent arguments are passed by libxml.
void CaptureXmlErrors(void* context, const char* message, ...) {
  va_list args;
  va_start(args, message);
  std::string* error = static_cast<std::string*>(context);
  base::StringAppendV(error, message, args);
  va_end(args);
}

}  // namespace

XmlParser::XmlParser() = default;

XmlParser::~XmlParser() = default;

void XmlParser::Parse(const std::string& xml,
                      WhitespaceBehavior whitespace_behavior,
                      ParseCallback callback) {
  std::string errors;
  ScopedXmlErrorFunc error_func(&errors, CaptureXmlErrors);

  XmlReader xml_reader;
  if (!xml_reader.Load(xml)) {
    ReportError(std::move(callback), "Invalid XML: failed to load", errors);
    return;
  }

  base::Value root_element;
  std::vector<base::Value::Dict*> element_stack;
  while (xml_reader.Read()) {
    if (xml_reader.IsClosingElement()) {
      if (element_stack.empty()) {
        ReportError(std::move(callback), "Invalid XML: unbalanced elements",
                    errors);
        return;
      }
      element_stack.pop_back();
      continue;
    }

    std::string text;
    TextNodeType text_node_type = TextNodeType::kText;
    base::Value::Dict* current_element =
        element_stack.empty() ? nullptr : element_stack.back();
    bool push_new_node_to_stack = false;
    base::Value::Dict new_element;
    if (GetTextFromNode(&xml_reader, &text, &text_node_type)) {
      if (!base::IsStringUTF8(text)) {
        ReportError(std::move(callback), "Invalid XML: invalid UTF8 text.",
                    errors);
        return;
      }
      new_element = CreateTextNode(text, text_node_type);
    } else if (xml_reader.IsElement()) {
      new_element = CreateNewElement(xml_reader.NodeFullName());
      PopulateNamespaces(new_element, &xml_reader);
      PopulateAttributes(new_element, &xml_reader);
      // Self-closing (empty) element have no close tag (or children); don't
      // push them on the element stack.
      push_new_node_to_stack = !xml_reader.IsEmptyElement();
    } else if (whitespace_behavior ==
                   WhitespaceBehavior::kPreserveSignificant &&
               xml_reader.GetTextIfSignificantWhitespaceElement(&text)) {
      new_element = CreateTextNode(text, TextNodeType::kText);
    } else {
      // Ignore all other node types (comments, processing instructions,
      // DTDs...).
      continue;
    }

    base::Value::Dict* new_element_ptr;
    if (current_element) {
      new_element_ptr =
          AddChildToElement(*current_element, std::move(new_element));
    } else {
      // First element we are parsing, it becomes the root element.
      DCHECK(xml_reader.IsElement());
      DCHECK(root_element.is_none());
      root_element = base::Value(std::move(new_element));
      new_element_ptr = &root_element.GetDict();
    }
    if (push_new_node_to_stack)
      element_stack.push_back(new_element_ptr);
  }

  if (!element_stack.empty()) {
    ReportError(std::move(callback), "Invalid XML: unbalanced elements",
                errors);
    return;
  }
  if (!root_element.is_dict() || root_element.GetDict().empty()) {
    ReportError(std::move(callback), "Invalid XML: bad content", errors);
    return;
  }
  std::move(callback).Run(std::make_optional(std::move(root_element)),
                          std::optional<std::string>());
}

}  // namespace data_decoder
