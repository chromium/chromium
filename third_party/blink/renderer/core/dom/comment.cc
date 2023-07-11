/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/comment.h"

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

inline Comment::Comment(Document& document, const String& text)
    : CharacterData(document, text, kCreateComment) {}

Comment* Comment::Create(Document& document, const String& text) {
  return MakeGarbageCollected<Comment>(document, text);
}

String Comment::nodeName() const {
  return "#comment";
}

CharacterData* Comment::CloneWithData(Document& factory,
                                      const String& data) const {
  return Create(factory, data);
}

}  // namespace blink
