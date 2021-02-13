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

#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace blink {

AXMenuListOption::AXMenuListOption(HTMLOptionElement* element,
                                   AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(element, ax_object_cache) {}

Element* AXMenuListOption::ActionElement() const {
  return GetElement();
}

AXObject* AXMenuListOption::ComputeParentImpl() const {
  Node* node = GetNode();
  if (!node) {
    NOTREACHED();
    return nullptr;
  }

  auto* select = To<HTMLOptionElement>(node)->OwnerSelectElement();
  if (!select) {
    NOTREACHED();
    return nullptr;
  }

  AXObject* select_ax_object = AXObjectCache().GetOrCreate(select);
  if (!select_ax_object) {
    NOTREACHED();
    return nullptr;
  }

  // This happens if the <select> is not rendered. Return it and move on.
  auto* menu_list = DynamicTo<AXMenuList>(select_ax_object);
  if (!menu_list)
    return select_ax_object;

  // In order to return the popup, which is a mock object, we need to grab
  // the AXMenuList itself, and get its only child.
  if (menu_list->NeedsToUpdateChildren())
    menu_list->UpdateChildrenIfNecessary();

  const auto& child_objects = menu_list->ChildrenIncludingIgnored();
  if (child_objects.IsEmpty())
    return nullptr;
  DCHECK_EQ(child_objects.size(), 1UL)
      << "A menulist must have a single popup child";
  DCHECK(IsA<AXMenuListPopup>(child_objects[0].Get()));
  To<AXMenuListPopup>(child_objects[0].Get())->UpdateChildrenIfNecessary();

  // Return the popup child, which is the parent of this AXMenuListOption.
  return child_objects[0];
}

bool AXMenuListOption::IsVisible() const {
  if (!parent_)
    return false;

  // In a single-option select with the popup collapsed, only the selected
  // item is considered visible.
  return !parent_->IsOffScreen() ||
         ((IsSelected() == kSelectedStateTrue) ? true : false);
}

bool AXMenuListOption::IsOffScreen() const {
  // Invisible list options are considered to be offscreen.
  return !IsVisible();
}

AccessibilitySelectedState AXMenuListOption::IsSelected() const {
  if (!GetNode() || !CanSetSelectedAttribute())
    return kSelectedStateUndefined;

  AXObject* parent = ParentObject();
  if (!parent || !parent->IsMenuListPopup())
    return kSelectedStateUndefined;

  if (!parent->IsOffScreen()) {
    return ((parent->ActiveDescendant() == this) ? kSelectedStateTrue
                                                 : kSelectedStateFalse);
  }
  return To<HTMLOptionElement>(GetNode())->Selected() ? kSelectedStateTrue
                                                      : kSelectedStateFalse;
}

bool AXMenuListOption::OnNativeClickAction() {
  if (!GetNode())
    return false;

  if (IsA<AXMenuListPopup>(ParentObject())) {
    // Clicking on an option within a menu list should first select that item
    // (which should include firing `input` and `change` events), then toggle
    // whether the menu list is showing.
    GetElement()->AccessKeyAction(true);

    // Calling OnNativeClickAction on the parent select element will toggle
    // it open or closed.
    return ParentObject()->OnNativeClickAction();
  }

  return AXNodeObject::OnNativeClickAction();
}

bool AXMenuListOption::OnNativeSetSelectedAction(bool b) {
  if (!GetElement() || !CanSetSelectedAttribute())
    return false;

  To<HTMLOptionElement>(GetElement())->SetSelected(b);
  return true;
}

bool AXMenuListOption::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  if (IsDetached()) {
    NOTREACHED();
    return true;
  }

  if (DynamicTo<HTMLOptionElement>(GetNode())->FastHasAttribute(
          html_names::kHiddenAttr))
    return true;

  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

void AXMenuListOption::GetRelativeBounds(AXObject** out_container,
                                         FloatRect& out_bounds_in_container,
                                         SkMatrix44& out_container_transform,
                                         bool* clips_children) const {
  DCHECK(!IsDetached());
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  // When a <select> is collapsed, the bounds of its options are the same as
  // that of the containing <select>.
  // It is not necessary to compute the bounds of options in an expanded select.
  // On Mac and Android, the menu list is native and already accessible; those
  // are the platforms where we need AXMenuList so that the options can be part
  // of the accessibility tree when collapsed, and there's never going to be a
  // need to expose the bounds of options on those platforms.
  // On Windows and Linux, AXObjectCacheImpl::UseAXMenuList() will return false,
  // and therefore this code should not be reached.

  auto* select = To<HTMLOptionElement>(GetNode())->OwnerSelectElement();
  AXObject* ax_menu_list = AXObjectCache().GetOrCreate(select);
  if (!ax_menu_list)
    return;
  DCHECK(ax_menu_list->IsMenuList());
  DCHECK(ax_menu_list->GetLayoutObject());
  if (ax_menu_list->GetLayoutObject()) {
    ax_menu_list->GetRelativeBounds(out_container, out_bounds_in_container,
                                    out_container_transform, clips_children);
  }
}

String AXMenuListOption::TextAlternative(bool recursive,
                                         bool in_aria_labelled_by_traversal,
                                         AXObjectSet& visited,
                                         ax::mojom::NameFrom& name_from,
                                         AXRelatedObjectVector* related_objects,
                                         NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  bool found_text_alternative = false;
  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  name_from = ax::mojom::NameFrom::kContents;
  text_alternative = To<HTMLOptionElement>(GetNode())->DisplayLabel();
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative));
    name_sources->back().type = name_from;
    name_sources->back().text = text_alternative;
    found_text_alternative = true;
  }

  return text_alternative;
}

HTMLSelectElement* AXMenuListOption::ParentSelectNode() const {
  if (!GetNode())
    return nullptr;

  if (auto* option = DynamicTo<HTMLOptionElement>(GetNode()))
    return option->OwnerSelectElement();

  return nullptr;
}

}  // namespace blink
