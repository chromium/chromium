/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static bool IsValidAttributeName(const String& name) {
  if (!name.StartsWith("data-"))
    return false;

  unsigned length = name.length();
  for (unsigned i = 5; i < length; ++i) {
    if (IsASCIIUpper(name[i]))
      return false;
  }

  return true;
}

static String ConvertAttributeNameToPropertyName(const String& name) {
  StringBuilder string_builder;

  unsigned length = name.length();
  for (unsigned i = 5; i < length; ++i) {
    UChar character = name[i];
    if (character != '-') {
      string_builder.Append(character);
    } else {
      if ((i + 1 < length) && IsASCIILower(name[i + 1])) {
        string_builder.Append(ToASCIIUpper(name[i + 1]));
        ++i;
      } else {
        string_builder.Append(character);
      }
    }
  }

  return string_builder.ReleaseString();
}

template <typename CharType1, typename CharType2>
static bool PropertyNameMatchesAttributeName(
    base::span<const CharType1> property_name,
    base::span<const CharType2> attribute_name) {
  size_t a = 5;
  size_t p = 0;
  bool word_boundary = false;
  while (a < attribute_name.size() && p < property_name.size()) {
    const CharType2 current_attribute_char = attribute_name[a];
    if (current_attribute_char == '-' && a + 1 < attribute_name.size() &&
        IsASCIILower(attribute_name[a + 1])) {
      word_boundary = true;
    } else {
      const CharType2 current_attribute_char_to_compare =
          word_boundary ? ToASCIIUpper(current_attribute_char)
                        : current_attribute_char;
      if (current_attribute_char_to_compare != property_name[p]) {
        return false;
      }
      p++;
      word_boundary = false;
    }
    a++;
  }

  return (a == attribute_name.size() && p == property_name.size());
}

static bool PropertyNameMatchesAttributeName(const String& property_name,
                                             const String& attribute_name) {
  if (!attribute_name.StartsWith("data-"))
    return false;

  if (property_name.Is8Bit()) {
    if (attribute_name.Is8Bit()) {
      return PropertyNameMatchesAttributeName(property_name.Span8(),
                                              attribute_name.Span8());
    }
    return PropertyNameMatchesAttributeName(property_name.Span8(),
                                            attribute_name.Span16());
  }

  if (attribute_name.Is8Bit()) {
    return PropertyNameMatchesAttributeName(property_name.Span16(),
                                            attribute_name.Span8());
  }
  return PropertyNameMatchesAttributeName(property_name.Span16(),
                                          attribute_name.Span16());
}

static bool IsValidPropertyName(const String& name) {
  unsigned length = name.length();
  for (unsigned i = 0; i < length; ++i) {
    if (name[i] == '-' && (i + 1 < length) && IsASCIILower(name[i + 1]))
      return false;
  }
  return true;
}

// This returns an AtomicString because attribute names are always stored
// as AtomicString types in Element (see setAttribute()).
static AtomicString ConvertPropertyNameToAttributeName(const String& name) {
  StringBuilder builder;
  builder.Append("data-");

  unsigned length = name.length();
  for (unsigned i = 0; i < length; ++i) {
    UChar character = name[i];
    if (IsASCIIUpper(character)) {
      builder.Append('-');
      builder.Append(ToASCIILower(character));
    } else {
      builder.Append(character);
    }
  }

  return builder.ToAtomicString();
}

void DatasetDOMStringMap::GetNames(Vector<String>& names) {
  AttributeCollection attributes = element_->Attributes();
  for (const Attribute& attr : attributes) {
    if (IsValidAttributeName(attr.LocalName()))
      names.push_back(ConvertAttributeNameToPropertyName(attr.LocalName()));
  }
}

String DatasetDOMStringMap::item(const String& name) {
  AttributeCollection attributes = element_->Attributes();
  for (const Attribute& attr : attributes) {
    if (PropertyNameMatchesAttributeName(name, attr.LocalName()))
      return attr.Value();
  }

  return String();
}

bool DatasetDOMStringMap::Contains(const String& name) {
  AttributeCollection attributes = element_->Attributes();
  for (const Attribute& attr : attributes) {
    if (PropertyNameMatchesAttributeName(name, attr.LocalName()))
      return true;
  }
  return false;
}

void DatasetDOMStringMap::SetItem(const String& name,
                                  const String& value,
                                  ExceptionState& exception_state) {
  if (!IsValidPropertyName(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + name + "' is not a valid property name.");
    return;
  }

  element_->setAttribute(ConvertPropertyNameToAttributeName(name),
                         AtomicString(value), exception_state);
}

bool DatasetDOMStringMap::DeleteItem(const String& name) {
  if (IsValidPropertyName(name)) {
    AtomicString attribute_name = ConvertPropertyNameToAttributeName(name);
    if (element_->hasAttribute(attribute_name)) {
      element_->removeAttribute(attribute_name);
      return true;
    }
  }
  return false;
}

void DatasetDOMStringMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  ElementRareDataField::Trace(visitor);
  DOMStringMap::Trace(visitor);
}

}  // namespace blink
