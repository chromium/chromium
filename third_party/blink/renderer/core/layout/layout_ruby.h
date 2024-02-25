/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_H_

#include "third_party/blink/renderer/core/layout/layout_inline.h"

namespace blink {

class LayoutRubyColumn;
class RubyContainer;

// Following the HTML 5 spec, the box object model for a <ruby> element allows
// several runs of ruby
// bases with their respective ruby texts looks as follows:
//
// 1 LayoutRuby object, corresponding to the whole <ruby> HTML element
//      1+ LayoutRubyColumn (anonymous)
//          0 or 1 LayoutRubyText - shuffled to the front in order to re-use
//                                  existing block layouting
//              0-n inline object(s)
//          1 LayoutRubyBase - contains the inline objects that make up the
//                             ruby base
//              0-n inline object(s)
//
// Note: <rp> elements are defined as having 'display:none' and thus normally
// are not assigned a layoutObject.
//
// Generated :before/:after content is shunted into anonymous inline blocks

// <ruby> when used as 'display:inline'
class LayoutRuby final : public LayoutInline {
 public:
  LayoutRuby(Element*);
  ~LayoutRuby() override;
  void Trace(Visitor* visitor) const override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutRuby (inline)";
  }

  void DidRemoveChildFromColumn(LayoutObject& child);

  static LayoutRubyColumn* LastRubyColumn(const LayoutObject& ruby);
  static LayoutRubyColumn* FindRubyColumnParent(LayoutObject* child);

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

 private:
  bool IsRuby() const final {
    NOT_DESTROYED();
    return true;
  }

  Member<RubyContainer> ruby_container_;
};

template <>
struct DowncastTraits<LayoutRuby> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRuby() && object.IsLayoutInline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RUBY_H_
