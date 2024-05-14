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

#include "third_party/blink/renderer/core/layout/layout_ruby.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/layout/ruby_container.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// === generic helper functions to avoid excessive code duplication ===

// static
LayoutRubyColumn* LayoutRuby::LastRubyColumn(const LayoutObject& ruby) {
  return To<LayoutRubyColumn>(ruby.SlowLastChild());
}

// static
LayoutRubyColumn* LayoutRuby::FindRubyColumnParent(LayoutObject* child) {
  while (child && !child->IsRubyColumn()) {
    child = child->Parent();
  }
  return To<LayoutRubyColumn>(child);
}

// === ruby as inline object ===

LayoutRuby::LayoutRuby(Element* element)
    : LayoutInline(element),
      ruby_container_(MakeGarbageCollected<RubyContainer>(*this)) {
  DCHECK(!RuntimeEnabledFeatures::RubyLineBreakableEnabled());
  if (element) {
    UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
  }
}

LayoutRuby::~LayoutRuby() = default;

void LayoutRuby::Trace(Visitor* visitor) const {
  visitor->Trace(ruby_container_);
  LayoutInline::Trace(visitor);
}

void LayoutRuby::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutInline::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

void LayoutRuby::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  // If the child is a ruby column, just add it normally.
  if (child->IsRubyColumn()) {
    LayoutInline::AddChild(child, before_child);
    return;
  }

  ruby_container_->AddChild(child, before_child);
}

void LayoutRuby::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child's parent is *this (must be a ruby column), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyColumn());
    LayoutInline::RemoveChild(child);
    return;
  }

  NOTREACHED_IN_MIGRATION() << child;
}

void LayoutRuby::DidRemoveChildFromColumn(LayoutObject& child) {
  ruby_container_->DidRemoveChildFromColumn(child);
}

}  // namespace blink
