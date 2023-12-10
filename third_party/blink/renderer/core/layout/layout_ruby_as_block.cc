// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_as_block.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/layout/ruby_container.h"

namespace blink {

LayoutRubyAsBlock::LayoutRubyAsBlock(Element* element)
    : LayoutNGBlockFlow(element),
      ruby_container_(RuntimeEnabledFeatures::RubySimplePairingEnabled()
                          ? MakeGarbageCollected<RubyContainer>(*this)
                          : nullptr) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutRubyAsBlock::~LayoutRubyAsBlock() = default;

void LayoutRubyAsBlock::Trace(Visitor* visitor) const {
  visitor->Trace(ruby_container_);
  LayoutNGBlockFlow::Trace(visitor);
}

bool LayoutRubyAsBlock::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRuby || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutRubyAsBlock::AddChild(LayoutObject* child,
                                 LayoutObject* before_child) {
  NOT_DESTROYED();
  // If the child is a ruby column, just add it normally.
  if (child->IsRubyColumn()) {
    LayoutNGBlockFlow::AddChild(child, before_child);
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
  auto* last_column = LayoutRubyAsInline::LastRubyColumn(*this);
  if (!last_column || last_column->HasRubyText()) {
    last_column = &LayoutRubyColumn::Create(this, *this);
    LayoutNGBlockFlow::AddChild(last_column, before_child);
    last_column->EnsureRubyBase();
  }
  last_column->AddChild(child);
}

void LayoutRubyAsBlock::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child's parent is *this (must be a ruby column), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyColumn());
    LayoutNGBlockFlow::RemoveChild(child);
    return;
  }

  if (RuntimeEnabledFeatures::RubySimplePairingEnabled()) {
    NOTREACHED();
    return;
  }

  // Otherwise find the containing column and remove it from there.
  auto* column = LayoutRubyAsInline::FindRubyColumnParent(child);
  DCHECK(column);
  column->RemoveChild(child);
}

void LayoutRubyAsBlock::DidRemoveChildFromColumn(LayoutObject& child) {
  ruby_container_->DidRemoveChildFromColumn(child);
}

void LayoutRubyAsBlock::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

void LayoutRubyAsBlock::RemoveLeftoverAnonymousBlock(LayoutBlock*) {
  NOT_DESTROYED();
  NOTREACHED();
}

}  // namespace blink
