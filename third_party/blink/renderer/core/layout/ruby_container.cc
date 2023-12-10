// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ruby_container.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"

namespace blink {

namespace {

LayoutRubyBase* FindAncestorBase(LayoutObject* object) {
  LayoutObject* parent = object;
  while ((parent = parent->Parent())) {
    if (auto* base = DynamicTo<LayoutRubyBase>(parent)) {
      return base;
    }
  }
  return nullptr;
}

}  // namespace

RubyContainer::RubyContainer(LayoutBoxModelObject& ruby) : ruby_object_(ruby) {}

void RubyContainer::Trace(Visitor* visitor) const {
  visitor->Trace(ruby_object_);
  visitor->Trace(content_list_);
}

void RubyContainer::AddChild(LayoutObject* child, LayoutObject* before_child) {
  if (!before_child) {
    if (InsertChildAt(child, content_list_.size())) {
      Repair();
    }
    return;
  }

  if (before_child->IsRubyBase() || before_child->IsRubyText()) {
    wtf_size_t index = content_list_.Find(before_child);
    DCHECK_NE(index, kNotFound) << before_child;
    if (InsertChildAt(child, index)) {
      Repair();
    }
    return;
  }

  if (child->IsRubyBase() || child->IsRubyText()) {
    content_list_.reserve(content_list_.size() + 2);
    // `before_child` is a descendant of LayoutRubyBase. We need to split the
    // ancestor LayoutRubyBase into two, and insert the `child` between them.
    auto* current_base = FindAncestorBase(before_child);
    DCHECK(current_base->IsAnonymous());
    wtf_size_t index = content_list_.Find(current_base);
    auto& new_base = LayoutRubyColumn::CreateRubyBase(*ruby_object_);
    current_base->MoveChildren(new_base, before_child);
    if (new_base.FirstChild()) {
      content_list_.insert(index++, new_base);
    } else {
      new_base.Destroy();
    }
    content_list_.insert(index, child);
    Repair();
    return;
  }
  DCHECK(!child->IsRubyBase());
  DCHECK(!child->IsRubyText());
  DCHECK(!before_child->IsRubyBase());
  DCHECK(!before_child->IsRubyText());
  FindAncestorBase(before_child)->AddChild(child, before_child);
}

bool RubyContainer::InsertChildAt(LayoutObject* child, wtf_size_t index) {
  if (child->IsRubyBase() || child->IsRubyText()) {
    content_list_.insert(index, child);
    return true;
  }

  LayoutObject* parent_base = nullptr;
  if (index > 0) {
    parent_base = DynamicTo<LayoutRubyBase>(content_list_[index - 1].Get());
    if (parent_base && !parent_base->IsAnonymous()) {
      parent_base = nullptr;
    }
  }
  if (!parent_base) {
    parent_base = &LayoutRubyColumn::CreateRubyBase(*ruby_object_);
    content_list_.insert(index, parent_base);
    parent_base->AddChild(child);
    return true;
  }
  parent_base->AddChild(child);
  // content_list_ was not updated. No need to call Repair() in this case.
  return false;
}

void RubyContainer::DidRemoveChildFromColumn(LayoutObject& child) {
  DCHECK(child.IsRubyBase() || child.IsRubyText()) << child;
  wtf_size_t index = content_list_.Find(&child);
  DCHECK_NE(index, kNotFound) << child;
  content_list_.EraseAt(index);
  MergeAnonymousBases(index);
  Repair();
}

void RubyContainer::Repair() {
  // Optimization for a typical case.
  if (auto* column = To<LayoutRubyColumn>(ruby_object_->SlowFirstChild())) {
    if (!column->NextSibling() && content_list_.size() == 2 &&
        column->RubyBase() == content_list_[0] && !column->RubyText() &&
        content_list_[1]->IsRubyText()) {
      column->AddChild(content_list_[1]);
      return;
    }
  }

  // Remove all LayoutRubyColumn children, and make pairs of a RubyBase and a
  // RubyText from scratch.

  while (LayoutObject* child = ruby_object_->SlowFirstChild()) {
    To<LayoutRubyColumn>(child)->RemoveAllChildren();
    ruby_object_->RemoveChild(child);
    if (!child->BeingDestroyed()) {
      child->Destroy();
    }
  }

  for (wtf_size_t i = 0; i < content_list_.size();) {
    auto& column = LayoutRubyColumn::Create(ruby_object_.Get(),
                                            *ruby_object_->ContainingBlock());
    ruby_object_->AddChild(&column);
    LayoutObject* object = content_list_[i++];
    if (object->IsRubyBase()) {
      column.AddChild(object);
      if (i < content_list_.size() && content_list_[i]->IsRubyText()) {
        column.AddChild(content_list_[i++]);
      }
    } else {
      DCHECK(object->IsRubyText());
      column.EnsureRubyBase().SetPlaceholder();
      column.AddChild(object);
    }
  }
}

void RubyContainer::MergeAnonymousBases(wtf_size_t index) {
  if (index == 0 || content_list_.size() <= index) {
    return;
  }
  auto* base1 = DynamicTo<LayoutRubyBase>(content_list_[index - 1].Get());
  auto* base2 = DynamicTo<LayoutRubyBase>(content_list_[index].Get());
  if (base1 && base1->IsAnonymous() && base2 && base2->IsAnonymous()) {
    base2->MoveChildren(*base1);
    base2->Destroy();
    // Destroy() will call this->DidRemoveChildFromColumn(base2).
  }
}

}  // namespace blink
