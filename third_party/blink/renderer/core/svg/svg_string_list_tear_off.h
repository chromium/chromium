/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_STRING_LIST_TEAR_OFF_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_STRING_LIST_TEAR_OFF_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_string_list.h"

namespace blink {

class SVGStringListTearOff : public SVGPropertyTearOff<SVGStringListBase> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SVGStringListTearOff(SVGStringListBase*,
                       SVGAnimatedPropertyBase* binding,
                       PropertyIsAnimValType);

  // SVGStringList DOM interface:

  // WebIDL requires "unsigned long" type which is uint32_t.
  uint32_t length() { return Target()->length(); }

  void clear(ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return;
    }
    Target()->clear();
    CommitChange();
  }

  String initialize(const String& item, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->Initialize(item);
    CommitChange();
    return item;
  }

  String getItem(uint32_t index, ExceptionState& exception_state) {
    return Target()->GetItem(index, exception_state);
  }

  String insertItemBefore(const String& item,
                          uint32_t index,
                          ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->InsertItemBefore(item, index);
    CommitChange();
    return item;
  }

  String replaceItem(const String& item,
                     uint32_t index,
                     ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->ReplaceItem(item, index, exception_state);
    CommitChange();
    return item;
  }

  bool AnonymousIndexedSetter(uint32_t index,
                              const String& item,
                              ExceptionState& exception_state) {
    replaceItem(item, index, exception_state);
    return true;
  }

  String removeItem(uint32_t index, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    String removed_item = Target()->RemoveItem(index, exception_state);
    CommitChange();
    return removed_item;
  }

  String appendItem(const String& item, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->AppendItem(item);
    CommitChange();
    return item;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_STRING_LIST_TEAR_OFF_H_
