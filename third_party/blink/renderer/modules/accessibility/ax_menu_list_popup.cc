/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

AXMenuListPopup::AXMenuListPopup(AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache), active_index_(-1) {}

ax::mojom::blink::Role AXMenuListPopup::NativeRoleIgnoringAria() const {
  return ax::mojom::blink::Role::kMenuListPopup;
}

bool AXMenuListPopup::IsVisible() const {
  return !IsOffScreen();
}

bool AXMenuListPopup::IsOffScreen() const {
  if (!parent_)
    return true;

  return parent_->IsExpanded() == kExpandedCollapsed;
}

AXRestriction AXMenuListPopup::Restriction() const {
  return parent_ && parent_->Restriction() == kRestrictionDisabled
             ? kRestrictionDisabled
             : kRestrictionNone;
}

bool AXMenuListPopup::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  // Base whether the menupopup is ignored on the containing <select>.
  if (parent_)
    return parent_->ComputeAccessibilityIsIgnored(ignored_reasons);

  return kIgnoreObject;
}

AXMenuListOption* AXMenuListPopup::MenuListOptionAXObject(
    HTMLElement* element) {
  DCHECK(element);
  DCHECK(IsA<HTMLOptionElement>(*element));

  AXObject* ax_object = AXObjectCache().GetOrCreate(element, this);

  return DynamicTo<AXMenuListOption>(ax_object);
}

int AXMenuListPopup::GetSelectedIndex() const {
  if (!parent_)
    return -1;

  auto* html_select_element = DynamicTo<HTMLSelectElement>(parent_->GetNode());
  if (!html_select_element)
    return -1;

  return html_select_element->selectedIndex();
}

bool AXMenuListPopup::OnNativeClickAction() {
  if (!parent_)
    return false;

  return parent_->OnNativeClickAction();
}

void AXMenuListPopup::AddChildren() {
#if defined(AX_FAIL_FAST_BUILD)
  DCHECK(!IsDetached());
  DCHECK(!is_adding_children_) << " Reentering method on " << GetNode();
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
  DCHECK_EQ(children_.size(), 0U)
      << "Parent still has " << children_.size() << " children before adding:"
      << "\nParent is " << ToString(true, true) << "\nFirst child is "
      << children_[0]->ToString(true, true);
#endif

  if (!parent_)
    return;

  auto* html_select_element = DynamicTo<HTMLSelectElement>(parent_->GetNode());
  if (!html_select_element)
    return;

  DCHECK(children_.empty());
  DCHECK(children_dirty_);
  children_dirty_ = false;

  if (active_index_ == -1)
    active_index_ = GetSelectedIndex();

  for (auto* const option_element : html_select_element->GetOptionList()) {
#if DCHECK_IS_ON()
    AXObject* ax_preexisting = AXObjectCache().Get(option_element);
    DCHECK(!ax_preexisting ||
           !ax_preexisting->AccessibilityIsIncludedInTree() ||
           !ax_preexisting->CachedParentObject() ||
           ax_preexisting->CachedParentObject() == this)
        << "\nChild = " << ax_preexisting->ToString(true, true)
        << "\n  IsAXMenuListOption? " << IsA<AXMenuListOption>(ax_preexisting)
        << "\nNew parent = " << ToString(true, true)
        << "\nPreexisting parent = "
        << ax_preexisting->CachedParentObject()->ToString(true, true);
#endif
    AXMenuListOption* option = MenuListOptionAXObject(option_element);
    if (option && option->AccessibilityIsIncludedInTree()) {
      DCHECK(!option->IsDetached());
      children_.push_back(option);
    }
  }
}

void AXMenuListPopup::DidUpdateActiveOption(int option_index,
                                            bool fire_notifications) {
  UpdateChildrenIfNecessary();

  int old_index = active_index_;
  active_index_ = option_index;

  if (!fire_notifications)
    return;

  AXObjectCacheImpl& cache = AXObjectCache();
  if (old_index != option_index && old_index >= 0 &&
      old_index < static_cast<int>(children_.size())) {
    AXObject* previous_child = children_[old_index].Get();
    cache.MarkAXObjectDirtyWithCleanLayout(previous_child);
  }

  if (option_index >= 0 && option_index < static_cast<int>(children_.size())) {
    AXObject* child = children_[option_index].Get();
    cache.MarkAXObjectDirtyWithCleanLayout(child);
    cache.PostNotification(this,
                           ax::mojom::blink::Event::kActiveDescendantChanged);
  }
}

void AXMenuListPopup::DidHide() {
  AXObjectCacheImpl& cache = AXObjectCache();
  AXObject* descendant = ActiveDescendant();
  cache.PostNotification(this, ax::mojom::Event::kHide);
  if (descendant)  // TODO(accessibility) Try removing. Line below is enough.
    cache.MarkAXObjectDirtyWithCleanLayout(this);
  cache.MarkAXSubtreeDirtyWithCleanLayout(ParentObject());
}

void AXMenuListPopup::DidShow() {
  UpdateChildrenIfNecessary();

  AXObjectCacheImpl& cache = AXObjectCache();
  cache.PostNotification(this, ax::mojom::Event::kShow);
  int selected_index = GetSelectedIndex();
  if (selected_index >= 0 &&
      selected_index < static_cast<int>(children_.size())) {
    DidUpdateActiveOption(selected_index);
  } else {
    cache.PostNotification(parent_, ax::mojom::Event::kFocus);
  }
  cache.MarkAXSubtreeDirtyWithCleanLayout(ParentObject());
}

AXObject* AXMenuListPopup::ActiveDescendant() {
  // Some Windows screen readers don't work properly if the active descendant
  // gets the focus before they focus the list menu popup.
  if (parent_ && !parent_->IsFocused())
    return nullptr;

  if (active_index_ < 0 || active_index_ >= ChildCountIncludingIgnored())
    return nullptr;

  auto* select = DynamicTo<HTMLSelectElement>(parent_->GetNode());
  if (!select)
    return nullptr;

  HTMLOptionElement* option = select->item(active_index_);
  DCHECK(option);
  return AXObjectCache().Get(option);
}

}  // namespace blink
