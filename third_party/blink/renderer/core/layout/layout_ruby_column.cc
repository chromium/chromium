// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

namespace blink {

namespace {

void UpdateRubyBaseStyle(const LayoutObject* child,
                         ComputedStyleBuilder& builder) {
  DCHECK(child->IsRubyBase());
  // FIXME: use WEBKIT_CENTER?
  builder.SetTextAlign(ETextAlign::kCenter);
  builder.SetHasLineIfEmpty(true);
}

}  // namespace

LayoutRubyColumn::LayoutRubyColumn() : LayoutBlockFlow(nullptr) {
  DCHECK(!RuntimeEnabledFeatures::RubyLineBreakableEnabled());
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutRubyColumn::~LayoutRubyColumn() = default;

void LayoutRubyColumn::RemoveLeftoverAnonymousBlock(LayoutBlock*) {
  NOT_DESTROYED();
}

bool LayoutRubyColumn::HasRubyText() const {
  NOT_DESTROYED();
  // The only place where a ruby text can be is in the first position
  // Note: As anonymous blocks, ruby columns do not have ':before' or ':after'
  // content themselves.
  return FirstChild() && FirstChild()->IsRubyText();
}

bool LayoutRubyColumn::HasRubyBase() const {
  NOT_DESTROYED();
  // The only place where a ruby base can be is in the last position
  // Note: As anonymous blocks, ruby columns do not have ':before' or ':after'
  // content themselves.
  return LastChild() && LastChild()->IsRubyBase();
}

LayoutRubyText* LayoutRubyColumn::RubyText() const {
  NOT_DESTROYED();
  LayoutObject* child = FirstChild();
  // If in future it becomes necessary to support floating or positioned ruby
  // text, layout will have to be changed to handle them properly.
  DCHECK(!child || !child->IsRubyText() ||
         !child->IsFloatingOrOutOfFlowPositioned());
  return DynamicTo<LayoutRubyText>(child);
}

LayoutRubyBase* LayoutRubyColumn::RubyBase() const {
  NOT_DESTROYED();
  return DynamicTo<LayoutRubyBase>(LastChild());
}

LayoutRubyBase& LayoutRubyColumn::EnsureRubyBase() {
  NOT_DESTROYED();
  if (auto* base = RubyBase()) {
    return *base;
  }
  auto& new_base = CreateRubyBase(*this);
  LayoutBlockFlow::AddChild(&new_base);
  return new_base;
}

bool LayoutRubyColumn::IsChildAllowed(LayoutObject* child,
                                      const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsRubyText() || child->IsInline();
}

void LayoutRubyColumn::AddChild(LayoutObject* child,
                                LayoutObject* before_child) {
  NOT_DESTROYED();
  DCHECK(child);

  if (child->IsRubyText()) {
    if (!before_child) {
      // LayoutRuby has already ascertained that we can add the child here.
      DCHECK(!HasRubyText());
      // prepend ruby texts as first child
      LayoutBlockFlow::AddChild(child, FirstChild());
    } else {
      NOTREACHED_IN_MIGRATION() << before_child;
    }
  } else if (child->IsRubyBase()) {
    DCHECK(!before_child);
    DCHECK(!RubyBase());
    LayoutBlockFlow::AddChild(child);
  } else {
    NOTREACHED_IN_MIGRATION() << child;
  }
}

void LayoutRubyColumn::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlockFlow::RemoveChild(child);

  if (!DocumentBeingDestroyed()) {
    DCHECK(child->IsRubyBase() || child->IsRubyText());
    if (auto* inline_ruby = DynamicTo<LayoutRuby>(Parent())) {
      inline_ruby->DidRemoveChildFromColumn(*child);
    }
    // Do nothing here! `this` might be destroyed by RubyContainer::Repair().
  }
}

void LayoutRubyColumn::RemoveAllChildren() {
  if (auto* text = RubyText()) {
    LayoutBlockFlow::RemoveChild(text);
  }
  if (auto* base = RubyBase()) {
    LayoutBlockFlow::RemoveChild(base);
    if (base->IsPlaceholder()) {
      // This RubyBase was created for a RubyText without a corresponding
      // RubyBase.  It should be destroyed here.
      base->Destroy();
    }
  }
}

// static
LayoutRubyBase& LayoutRubyColumn::CreateRubyBase(
    const LayoutObject& reference) {
  auto* layout_object = MakeGarbageCollected<LayoutRubyBase>();
  layout_object->SetDocumentForAnonymous(&reference.GetDocument());
  ComputedStyleBuilder new_style_builder =
      reference.GetDocument()
          .GetStyleResolver()
          .CreateAnonymousStyleBuilderWithDisplay(reference.StyleRef(),
                                                  EDisplay::kBlock);
  UpdateRubyBaseStyle(layout_object, new_style_builder);
  layout_object->SetStyle(new_style_builder.TakeStyle());
  return *layout_object;
}

void LayoutRubyColumn::UpdateAnonymousChildStyle(
    const LayoutObject* child,
    ComputedStyleBuilder& builder) const {
  NOT_DESTROYED();
  if (child->IsRubyBase()) {
    UpdateRubyBaseStyle(child, builder);
  }
}

// static
LayoutRubyColumn& LayoutRubyColumn::Create(
    const LayoutObject* parent_ruby,
    const LayoutBlock& containing_block) {
  DCHECK(parent_ruby);
  DCHECK(parent_ruby->IsRuby());
  LayoutRubyColumn* column = MakeGarbageCollected<LayoutRubyColumn>();
  column->SetDocumentForAnonymous(&parent_ruby->GetDocument());
  const ComputedStyle* new_style =
      parent_ruby->GetDocument()
          .GetStyleResolver()
          .CreateAnonymousStyleWithDisplay(parent_ruby->StyleRef(),
                                           EDisplay::kInlineBlock);
  column->SetStyle(new_style);
  return *column;
}

}  // namespace blink
