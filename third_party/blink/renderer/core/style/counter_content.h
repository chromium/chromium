/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_CONTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_CONTENT_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CounterContent {
  USING_FAST_MALLOC(CounterContent);

 public:
  CounterContent(const AtomicString& identifier,
                 EListStyleType style,
                 const AtomicString& separator)
      : identifier_(identifier), list_style_(style), separator_(separator) {
    DCHECK_NE(style, EListStyleType::kString);
  }

  const AtomicString& Identifier() const { return identifier_; }
  EListStyleType ListStyle() const { return list_style_; }
  const AtomicString& Separator() const { return separator_; }

 private:
  AtomicString identifier_;
  EListStyleType list_style_;
  AtomicString separator_;
};

static inline bool operator==(const CounterContent& a,
                              const CounterContent& b) {
  return a.Identifier() == b.Identifier() && a.ListStyle() == b.ListStyle() &&
         a.Separator() == b.Separator();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_CONTENT_H_
