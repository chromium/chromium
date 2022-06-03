/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_param_element.h"

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLParamElement::HTMLParamElement(Document& document)
    : HTMLElement(html_names::kParamTag, document) {}

const AtomicString& HTMLParamElement::GetName() const {
  if (HasName())
    return GetNameAttribute();
  return IsA<HTMLDocument>(GetDocument()) ? g_empty_atom : GetIdAttribute();
}

const AtomicString& HTMLParamElement::Value() const {
  return FastGetAttribute(html_names::kValueAttr);
}

// HTML5 says that an object resource's URL is specified by the object's
// data attribute, not by a param element. However, for compatibility, also
// allow the resource's URL to be given by a param of the named "code",
// "data", "movie", "src" or "url".
bool HTMLParamElement::IsURLParameter(const String& name) {
  return EqualIgnoringASCIICase(name, "code") ||
         EqualIgnoringASCIICase(name, "data") ||
         EqualIgnoringASCIICase(name, "movie") ||
         EqualIgnoringASCIICase(name, "src") ||
         EqualIgnoringASCIICase(name, "url");
}

bool HTMLParamElement::IsURLAttribute(const Attribute& attribute) const {
  if (attribute.GetName() == html_names::kValueAttr &&
      IsURLParameter(GetName()))
    return true;
  return HTMLElement::IsURLAttribute(attribute);
}

}  // namespace blink
