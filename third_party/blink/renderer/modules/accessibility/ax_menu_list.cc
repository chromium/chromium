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

ax::mojom::Role AXMenuList::DetermineAccessibilityRole() {
  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;

  return ax::mojom::Role::kPopUpButton;
}

bool AXMenuList::OnNativeClickAction() {
  if (!layout_object_)
    return false;

  HTMLSelectElement* select = To<HTMLSelectElement>(GetNode());
  if (select->PopupIsVisible())
    select->HidePopup();
  else
    select->ShowPopup();
  return true;
}

void AXMenuList::ClearChildren() {
  children_dirty_ = false;
  if (children_.IsEmpty())
    return;

  // There's no reason to clear our AXMenuListPopup child. If we get a
  // call to clearChildren, it's because the options might have changed,
  // so call it on our popup.
  DCHECK_EQ(ChildCountIncludingIgnored(), 1);
  children_[0]->ClearChildren();
}

void AXMenuList::AddChildren() {
  DCHECK(!IsDetached());
  have_children_ = true;

  AXObjectCacheImpl& cache = AXObjectCache();
  AXObject* popup = cache.Create(ax::mojom::blink::Role::kMenuListPopup, this);
  DCHECK(popup);

  if (!popup->AccessibilityIsIncludedInTree()) {
    cache.Remove(popup->AXObjectID());
    return;
  }

  children_.push_back(popup);
  popup->AddChildren();
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

void AXMenuList::DidUpdateActiveOption(int option_index) {
  bool suppress_notifications =
      (GetNode() && !GetNode()->IsFinishedParsingChildren());

  if (HasChildren()) {
    const auto& child_objects = ChildrenIncludingIgnored();
    if (!child_objects.IsEmpty()) {
      DCHECK_EQ(child_objects.size(), 1ul);
      DCHECK(IsA<AXMenuListPopup>(child_objects[0].Get()));

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

}  // namespace blink
