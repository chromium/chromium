/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2011, 2012 Apple Inc. All rights
 * reserved.
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
 *
 */

#include "third_party/blink/renderer/core/html/html_name_collection.h"

#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"

namespace blink {

HTMLNameCollection::HTMLNameCollection(ContainerNode& document,
                                       CollectionType type,
                                       const AtomicString& name)
    : HTMLCollection(document, type, kDoesNotOverrideItemAfter), name_(name) {}

HTMLNameCollection::~HTMLNameCollection() {
  DCHECK(GetType() == kWindowNamedItems || GetType() == kDocumentNamedItems ||
         GetType() == kDocumentAllNamedItems);
}

}  // namespace blink
