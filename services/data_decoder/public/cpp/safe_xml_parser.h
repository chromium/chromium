// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/token.h"
#include "base/values.h"

namespace data_decoder {

// Returns all the children of |element|.
const base::Value::List* GetXmlElementChildren(const base::Value& element);

// Returns the qualified name |name_space|:|name| or simply |name| if
// |name_space| is empty.
std::string GetXmlQualifiedName(const std::string& name_space,
                                const std::string& name);

// Returns true if |element| is an XML element with a tag name |name|, false
// otherwise.
bool IsXmlElementNamed(const base::Value& element, const std::string& name);

// Returns true if |element| is an XML element with a type |type|, false
// otherwise. Valid types are data_decoder::mojom::XmlParser::kElementType,
// kTextNodeType or kCDataNodeType.
bool IsXmlElementOfType(const base::Value& element, const std::string& type);

// Sets |name| with the tag name of |element| and returns true.
// Returns false if |element| does not represent a node with a tag or is not a
// valid XML element.
bool GetXmlElementTagName(const base::Value& element, std::string* name);

// Sets |text| with the text of |element| and returns true.
// Returns false if |element| does not contain any text (if it's not a text or
// CData node).
bool GetXmlElementText(const base::Value& element, std::string* text);

// If a namespace with a URI |namespace_uri| is defined in |element|, sets the
// prefix for that namespace in |prefix| and returns true.
// Returns false if no such namespace was found.
// Note that for the default namespace, the prefix is set to the empty string
// and the function returns true.
bool GetXmlElementNamespacePrefix(const base::Value& element,
                                  const std::string& namespace_uri,
                                  std::string* prefix);

// Returns the number of children of |element| named |name|.
int GetXmlElementChildrenCount(const base::Value& element,
                               const std::string& name);

// Returns the first child of |element| with the type |type|, or null if there
// are no children with that type.
// |type| are data_decoder::mojom::XmlParser::kElementType, kTextNodeType or
// kCDataNodeType.
const base::Value* GetXmlElementChildWithType(const base::Value& element,
                                              const std::string& type);

// Returns the first child of |element| with the tag |tag|, or null if there
// are no children with that tag.
const base::Value* GetXmlElementChildWithTag(const base::Value& element,
                                             const std::string& tag);

// Populates |children| with all the children of |element| with the tag |tag|.
// Returns true if such elements were found, false otherwise.
bool GetAllXmlElementChildrenWithTag(const base::Value& element,
                                     const std::string& tag,
                                     std::vector<const base::Value*>* children);

// Returns the value of the element path |path| starting at |element|, or null
// if any element in |path| is missing. Note that if there are more than one
// element matching any part of the path, the first one is used and
// |unique_path| is set to false. It is set to true otherwise and can be null if
// not needed.
const base::Value* FindXmlElementPath(
    const base::Value& element,
    std::initializer_list<std::string_view> path,
    bool* unique_path);

// Returns the value of the attribute named |attribute_name| in |element|, or
// an empty string if there is no such attribute.
std::string GetXmlElementAttribute(const base::Value& element,
                                   const std::string& attribute_name);

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_XML_PARSER_H_
