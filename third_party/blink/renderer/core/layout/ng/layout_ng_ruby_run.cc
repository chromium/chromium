// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_base.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_text.h"

namespace blink {

LayoutNGRubyRun::LayoutNGRubyRun() : LayoutNGBlockFlow(nullptr) {
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutNGRubyRun::~LayoutNGRubyRun() = default;

bool LayoutNGRubyRun::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRubyRun || LayoutBlockFlow::IsOfType(type);
}

bool LayoutNGRubyRun::CreatesAnonymousWrapper() const {
  NOT_DESTROYED();
  return true;
}

void LayoutNGRubyRun::RemoveLeftoverAnonymousBlock(LayoutBlock*) {
  NOT_DESTROYED();
}

bool LayoutNGRubyRun::HasRubyText() const {
  NOT_DESTROYED();
  // The only place where a ruby text can be is in the first position
  // Note: As anonymous blocks, ruby runs do not have ':before' or ':after'
  // content themselves.
  return FirstChild() && FirstChild()->IsRubyText();
}

bool LayoutNGRubyRun::HasRubyBase() const {
  NOT_DESTROYED();
  // The only place where a ruby base can be is in the last position
  // Note: As anonymous blocks, ruby runs do not have ':before' or ':after'
  // content themselves.
  return LastChild() && LastChild()->IsRubyBase();
}

LayoutNGRubyText* LayoutNGRubyRun::RubyText() const {
  NOT_DESTROYED();
  LayoutObject* child = FirstChild();
  // If in future it becomes necessary to support floating or positioned ruby
  // text, layout will have to be changed to handle them properly.
  DCHECK(!child || !child->IsRubyText() ||
         !child->IsFloatingOrOutOfFlowPositioned());
  return DynamicTo<LayoutNGRubyText>(child);
}

LayoutNGRubyBase* LayoutNGRubyRun::RubyBase() const {
  NOT_DESTROYED();
  return DynamicTo<LayoutNGRubyBase>(LastChild());
}

LayoutNGRubyBase& LayoutNGRubyRun::EnsureRubyBase() {
  NOT_DESTROYED();
  if (auto* base = RubyBase()) {
    return *base;
  }
  auto& new_base = CreateRubyBase();
  LayoutBlockFlow::AddChild(&new_base);
  return new_base;
}

bool LayoutNGRubyRun::IsChildAllowed(LayoutObject* child,
                                     const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsRubyText() || child->IsInline();
}

void LayoutNGRubyRun::AddChild(LayoutObject* child,
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
      // the old text goes into a new run that is inserted as next sibling.
      DCHECK_EQ(before_child->Parent(), this);
      LayoutObject* ruby = Parent();
      DCHECK(ruby->IsRuby());
      auto& new_run = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_run, NextSibling());
      new_run.EnsureRubyBase();
      // Add the new ruby text and move the old one to the new run
      // Note: Doing it in this order and not using LayoutNGRubyRun's methods,
      // in order to avoid automatic removal of the ruby run in case there is no
      // other child besides the old ruby text.
      LayoutBlockFlow::AddChild(child, before_child);
      LayoutBlockFlow::RemoveChild(before_child);
      new_run.AddChild(before_child);
    } else if (RubyBase()->FirstChild()) {
      // Insertion before a ruby base object.
      // In this case we need insert a new run before the current one and split
      // the base.
      LayoutObject* ruby = Parent();
      LayoutNGRubyRun& new_run = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_run, this);
      auto& new_base = new_run.EnsureRubyBase();
      new_run.AddChild(child);

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

void LayoutNGRubyRun::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child is a ruby text, then merge the ruby base with the base of
  // the right sibling run, if possible.
  if (!BeingDestroyed() && !DocumentBeingDestroyed() && child->IsRubyText()) {
    auto* base = RubyBase();
    LayoutObject* right_neighbour = NextSibling();
    if (base->FirstChild() && right_neighbour && right_neighbour->IsRubyRun()) {
      auto* right_run = To<LayoutNGRubyRun>(right_neighbour);
      auto& right_base = right_run->EnsureRubyBase();
      if (right_base.FirstChild()) {
        // Collect all children in a single base, then swap the bases.
        right_base.MoveChildren(*base);
        MoveChildTo(right_run, base);
        right_run->MoveChildTo(this, &right_base);
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

LayoutNGRubyBase& LayoutNGRubyRun::CreateRubyBase() const {
  NOT_DESTROYED();
  auto* layout_object = MakeGarbageCollected<LayoutNGRubyBase>();
  layout_object->SetDocumentForAnonymous(&GetDocument());
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          StyleRef(), EDisplay::kBlock);
  new_style_builder.SetTextAlign(
      ETextAlign::kCenter);  // FIXME: use WEBKIT_CENTER?
  new_style_builder.SetHasLineIfEmpty(true);
  layout_object->SetStyle(new_style_builder.TakeStyle());
  return *layout_object;
}

// static
LayoutNGRubyRun& LayoutNGRubyRun::Create(const LayoutObject* parent_ruby,
                                         const LayoutBlock& containing_block) {
  DCHECK(parent_ruby);
  DCHECK(parent_ruby->IsRuby());
  LayoutNGRubyRun* rr = MakeGarbageCollected<LayoutNGRubyRun>();
  rr->SetDocumentForAnonymous(&parent_ruby->GetDocument());
  scoped_refptr<const ComputedStyle> new_style =
      parent_ruby->GetDocument()
          .GetStyleResolver()
          .CreateAnonymousStyleWithDisplay(parent_ruby->StyleRef(),
                                           EDisplay::kInlineBlock);
  rr->SetStyle(std::move(new_style));
  return *rr;
}

}  // namespace blink
