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
LayoutRubyColumn* LayoutRubyAsInline::LastRubyColumn(const LayoutObject& ruby) {
  return To<LayoutRubyColumn>(ruby.SlowLastChild());
}

// static
LayoutRubyColumn* LayoutRubyAsInline::FindRubyColumnParent(
    LayoutObject* child) {
  while (child && !child->IsRubyColumn()) {
    child = child->Parent();
  }
  return To<LayoutRubyColumn>(child);
}

// === ruby as inline object ===

LayoutRubyAsInline::LayoutRubyAsInline(Element* element)
    : LayoutInline(element),
      ruby_container_(RuntimeEnabledFeatures::RubySimplePairingEnabled()
                          ? MakeGarbageCollected<RubyContainer>(*this)
                          : nullptr) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutRubyAsInline::~LayoutRubyAsInline() = default;

void LayoutRubyAsInline::Trace(Visitor* visitor) const {
  visitor->Trace(ruby_container_);
  LayoutInline::Trace(visitor);
}

void LayoutRubyAsInline::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutInline::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

void LayoutRubyAsInline::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  NOT_DESTROYED();
  // If the child is a ruby column, just add it normally.
  if (child->IsRubyColumn()) {
    LayoutInline::AddChild(child, before_child);
    return;
  }

  if (RuntimeEnabledFeatures::RubySimplePairingEnabled()) {
    ruby_container_->AddChild(child, before_child);
    return;
  }

  if (before_child) {
    // Insert the child into a column.
    LayoutObject* column = before_child;
    while (column && !column->IsRubyColumn()) {
      column = column->Parent();
    }
    if (column) {
      if (before_child == column) {
        before_child = To<LayoutRubyColumn>(before_child)->FirstChild();
      }
      DCHECK(!before_child || before_child->IsDescendantOf(column));
      column->AddChild(child, before_child);
      return;
    }
    NOTREACHED();  // before_child should always have a column as parent!
                   // Emergency fallback: fall through and just append.
  }

  // If the new child would be appended, try to add the child to the previous
  // column if possible, or create a new column otherwise.
  // (The LayoutRubyColumn object will handle the details)
  auto* last_column = LastRubyColumn(*this);
  if (!last_column || last_column->HasRubyText()) {
    last_column = &LayoutRubyColumn::Create(this, *ContainingBlock());
    LayoutInline::AddChild(last_column, before_child);
    last_column->EnsureRubyBase();
  }
  last_column->AddChild(child);
}

void LayoutRubyAsInline::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child's parent is *this (must be a ruby column), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyColumn());
    LayoutInline::RemoveChild(child);
    return;
  }

  if (RuntimeEnabledFeatures::RubySimplePairingEnabled()) {
    NOTREACHED();
    return;
  }

  // Otherwise find the containing column and remove it from there.
  auto* column = FindRubyColumnParent(child);
  DCHECK(column);
  column->RemoveChild(child);
}

void LayoutRubyAsInline::DidRemoveChildFromColumn(LayoutObject& child) {
  ruby_container_->DidRemoveChildFromColumn(child);
}

}  // namespace blink
