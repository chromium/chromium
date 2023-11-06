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

#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

AXMenuList::AXMenuList(LayoutObject* layout_object,
                       AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {
  DCHECK(IsA<HTMLSelectElement>(layout_object->GetNode()));
}

ax::mojom::blink::Role AXMenuList::NativeRoleIgnoringAria() const {
  return ax::mojom::blink::Role::kComboBoxSelect;
}

bool AXMenuList::OnNativeClickAction() {
  if (!layout_object_)
    return false;

  HTMLSelectElement* select = To<HTMLSelectElement>(GetNode());
  if (select->PopupIsVisible())
    select->HidePopup();
  else
    select->ShowPopup();

  // Send notification that action has been handled.
  AXObjectCache().HandleClicked(GetNode());

  return true;
}

void AXMenuList::Detach() {
  // The default implementation of Detach() calls ClearChildren(), but
  // that is not enough. The popup child needs to be specially removed
  // from the AXObjectCache.
  DCHECK_LE(children_.size(), 1U);

  // Clear the children.
  if (children_.size()) {
    // Do not call Remove() while AXObjectCacheImpl() is detaching all objects,
    // because the hash map of objects does not allow simultaneous iteration and
    // removal of objects.
    CHECK(popup_ == children_[0]);
    children_.clear();
  }

  // Clear the popup.
  if (popup_) {
    if (!AXObjectCache().HasBeenDisposed()) {
      auto& cache = AXObjectCache();
      for (auto* const option_grandchild :
           To<HTMLSelectElement>(GetNode())->GetOptionList()) {
        cache.Remove(option_grandchild, /* notify_parent */ false);
      }
      cache.Remove(popup_, /* notify_parent */ false);
    }
    popup_->Detach();
    popup_ = nullptr;
  }

  AXLayoutObject::Detach();
}

void AXMenuList::ChildrenChangedWithCleanLayout() {
  if (!children_.empty()) {
    CHECK_EQ(children_.size(), 1U);
    CHECK_EQ(children_[0], popup_);
    // If we have a child popup, update its children at the same time.
    popup_->ChildrenChangedWithCleanLayout();
  }

  AXLayoutObject::ChildrenChangedWithCleanLayout();
}

void AXMenuList::SetNeedsToUpdateChildren() const {
  if (!children_.empty()) {
    CHECK_EQ(children_.size(), 1U);
    CHECK_EQ(children_[0], popup_);
    // If we have a child popup, update its children at the same time.
    popup_->SetNeedsToUpdateChildren();
  }

  AXLayoutObject::SetNeedsToUpdateChildren();
}

void AXMenuList::ClearChildren() const {
  if (popup_) {
    popup_->ClearChildren();
  }
  AXLayoutObject::ClearChildren();
}

void AXMenuList::AddChildren() {
#if defined(AX_FAIL_FAST_BUILD)
  DCHECK(!IsDetached());
  DCHECK(!is_adding_children_) << " Reentering method on " << GetNode();
  base::AutoReset<bool> reentrancy_protector(&is_adding_children_, true);
  // ClearChildren() does not clear the menulist popup chld.
  CHECK(children_.empty()) << "Parent still has " << children_.size()
                           << " children before adding:"
                           << "\nParent is " << ToString(true, true)
                           << "\nFirst child is "
                           << children_[0]->ToString(true, true);
#endif

  CHECK(children_dirty_);

  GetOrCreateMockPopupChild();
  CHECK(popup_);
  children_.push_back(popup_);
  popup_->SetParent(this);

  children_dirty_ = false;

  // Update mock AXMenuListPopup children.
  popup_->UpdateChildrenIfNecessary();
}

AXObject* AXMenuList::GetOrCreateMockPopupChild() {
  if (IsDetached()) {
    popup_ = nullptr;
    return nullptr;
  }

  if (!popup_) {
    popup_ = AXObjectCache().CreateAndInit(
        ax::mojom::blink::Role::kMenuListPopup, this);
    CHECK(popup_);
    CHECK(!popup_->IsDetached());
    CHECK_EQ(popup_->CachedParentObject(), this);
    CHECK(popup_->IsMenuListPopup());
  }

  return popup_;
}

bool AXMenuList::IsCollapsed() const {
  // Collapsed is the "default" state, so if the LayoutObject doesn't exist
  // this makes slightly more sense than returning false.
  if (!layout_object_)
    return true;

  return !To<HTMLSelectElement>(GetNode())->PopupIsVisible();
}

AccessibilityExpanded AXMenuList::IsExpanded() const {
  if (IsCollapsed())
    return kExpandedCollapsed;

  return kExpandedExpanded;
}

void AXMenuList::DidUpdateActiveOption() {
  if (!GetNode())
    return;

  bool suppress_notifications = !GetNode()->IsFinishedParsingChildren();

  // TODO(aleventhal) The  NeedsToUpdateChildren() check is necessary to avoid a
  // illegal lifecycle while adding children, since this can be called at any
  // time by AXObjectCacheImpl(). Look into calling with clean layout.
  if (!NeedsToUpdateChildren()) {
    const auto& child_objects = ChildrenIncludingIgnored();
    if (!child_objects.empty()) {
      DCHECK_EQ(child_objects.size(), 1ul);
      DCHECK(IsA<AXMenuListPopup>(child_objects[0].Get()));
      HTMLSelectElement* select = To<HTMLSelectElement>(GetNode());
      DCHECK(select);
      HTMLOptionElement* active_option = select->OptionToBeShown();
      int option_index = active_option ? active_option->index() : -1;
      if (auto* popup = DynamicTo<AXMenuListPopup>(child_objects[0].Get()))
        popup->DidUpdateActiveOption(option_index, !suppress_notifications);
    }
  }

  AXObjectCache().PostNotification(this,
                                   ax::mojom::Event::kMenuListValueChanged);
}

void AXMenuList::DidShowPopup() {
  if (ChildCountIncludingIgnored() != 1)
    return;

  auto* popup = To<AXMenuListPopup>(ChildAtIncludingIgnored(0));
  popup->DidShow();
}

void AXMenuList::DidHidePopup() {
  if (ChildCountIncludingIgnored() != 1)
    return;

  auto* popup = To<AXMenuListPopup>(ChildAtIncludingIgnored(0));
  popup->DidHide();

  if (GetNode() && GetNode()->IsFocused())
    AXObjectCache().PostNotification(this, ax::mojom::Event::kFocus);
}

void AXMenuList::Trace(Visitor* visitor) const {
  visitor->Trace(popup_);
  AXLayoutObject::Trace(visitor);
}

}  // namespace blink
