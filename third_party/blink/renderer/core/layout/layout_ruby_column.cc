// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"

namespace blink {

LayoutRubyColumn::LayoutRubyColumn() : LayoutNGBlockFlow(nullptr) {
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutRubyColumn::~LayoutRubyColumn() = default;

bool LayoutRubyColumn::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRubyColumn || LayoutBlockFlow::IsOfType(type);
}

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
  auto& new_base = CreateRubyBase();
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
    } else if (before_child->IsRubyText()) {
      // New text is inserted just before another.
      // In this case the new text takes the place of the old one, and
      // the old text goes into a new column that is inserted as next sibling.
      DCHECK_EQ(before_child->Parent(), this);
      LayoutObject* ruby = Parent();
      DCHECK(ruby->IsRuby());
      auto& new_column = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_column, NextSibling());
      new_column.EnsureRubyBase();
      // Add the new ruby text and move the old one to the new column
      // Note: Doing it in this order and not using LayoutRubyColumn's methods,
      // in order to avoid automatic removal of the ruby column in case there is
      // no other child besides the old ruby text.
      LayoutBlockFlow::AddChild(child, before_child);
      LayoutBlockFlow::RemoveChild(before_child);
      new_column.AddChild(before_child);
    } else if (RubyBase()->FirstChild()) {
      // Insertion before a ruby base object.
      // In this case we need insert a new column before the current one and
      // split the base.
      LayoutObject* ruby = Parent();
      LayoutRubyColumn& new_column = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_column, this);
      auto& new_base = new_column.EnsureRubyBase();
      new_column.AddChild(child);

      EnsureRubyBase().MoveChildren(new_base, before_child);
    }
  } else {
    // child is not a text -> insert it into the base
    // (append it instead if beforeChild is the ruby text)
    auto& base = EnsureRubyBase();
    if (before_child == &base) {
      before_child = base.FirstChild();
    }
    if (before_child && before_child->IsRubyText()) {
      before_child = nullptr;
    }
    DCHECK(!before_child || before_child->IsDescendantOf(&base));
    base.AddChild(child, before_child);
  }
}

void LayoutRubyColumn::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child is a ruby text, then merge the ruby base with the base of
  // the right sibling column, if possible.
  if (!BeingDestroyed() && !DocumentBeingDestroyed() && child->IsRubyText()) {
    auto* base = RubyBase();
    LayoutObject* right_neighbour = NextSibling();
    if (base->FirstChild() && right_neighbour &&
        right_neighbour->IsRubyColumn()) {
      auto* right_column = To<LayoutRubyColumn>(right_neighbour);
      auto& right_base = right_column->EnsureRubyBase();
      if (right_base.FirstChild()) {
        // Collect all children in a single base, then swap the bases.
        right_base.MoveChildren(*base);
        MoveChildTo(right_column, base);
        right_column->MoveChildTo(this, &right_base);
        DCHECK(!RubyBase()->FirstChild());
      }
    }
  }

  LayoutBlockFlow::RemoveChild(child);

  if (!BeingDestroyed() && !DocumentBeingDestroyed()) {
    // If this has only an empty LayoutRubyBase, destroy this sub-tree.
    LayoutBlockFlow* base = RubyBase();
    if (!HasRubyText() && !base->FirstChild()) {
      LayoutBlockFlow::RemoveChild(base);
      base->Destroy();
      Destroy();
    }
  }
}

LayoutRubyBase& LayoutRubyColumn::CreateRubyBase() const {
  NOT_DESTROYED();
  auto* layout_object = MakeGarbageCollected<LayoutRubyBase>();
  layout_object->SetDocumentForAnonymous(&GetDocument());
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          StyleRef(), EDisplay::kBlock);
  UpdateAnonymousChildStyle(layout_object, new_style_builder);
  layout_object->SetStyle(new_style_builder.TakeStyle());
  return *layout_object;
}

void LayoutRubyColumn::UpdateAnonymousChildStyle(
    const LayoutObject* child,
    ComputedStyleBuilder& builder) const {
  NOT_DESTROYED();
  if (child->IsRubyBase()) {
    // FIXME: use WEBKIT_CENTER?
    builder.SetTextAlign(ETextAlign::kCenter);
    builder.SetHasLineIfEmpty(true);
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
