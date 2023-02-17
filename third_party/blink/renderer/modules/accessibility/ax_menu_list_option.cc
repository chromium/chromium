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
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

AXMenuListOption::AXMenuListOption(HTMLOptionElement* element,
                                   AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(element, ax_object_cache) {}

Element* AXMenuListOption::ActionElement() const {
  return GetElement();
}

// Return a parent if this is an <option> for an AXMenuList, otherwise null.
// Returns null means that a parent will be computed from the DOM.
// static
AXObject* AXMenuListOption::ComputeParentAXMenuPopupFor(
    AXObjectCacheImpl& cache,
    HTMLOptionElement* option) {
  // Note: In a <select> size=1, AXObjects are not created for <optgroup>'s.
  DCHECK(option);

  HTMLSelectElement* select = option->OwnerSelectElement();
  if (!select || !select->UsesMenuList()) {
    // If it's an <option> that is not inside of a menulist, we want it to
    // return to the caller and use the default logic.
    return nullptr;
  }

  // If there is a <select> ancestor, return the popup for it, if rendered.
  if (AXObject* select_ax_object = cache.GetOrCreate(select)) {
    if (auto* menu_list = DynamicTo<AXMenuList>(select_ax_object))
      return menu_list->GetOrCreateMockPopupChild();
  }

  // Otherwise, just return an AXObject for the parent node.
  // This could be the <select> if it was not rendered.
  // Or, any parent node if the <option> was not inside an AXMenuList.
  return cache.GetOrCreate(select);
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
    GetElement()->AccessKeyAction(
        SimulatedClickCreationScope::kFromAccessibility);

    // It's possible that the call to `AccessKeyAction` right above modified the
    // tree structure (e.g., by collapsing the list of options and removing them
    // from the tree), effectively detaching the current node from the tree. In
    // this case, `ParentObject` will now return nullptr.
    AXObject* parent = ParentObject();
    if (!parent)
      return false;

    // Calling OnNativeClickAction on the parent select element will toggle
    // it open or closed.
    return parent->OnNativeClickAction();
  }

  return AXNodeObject::OnNativeClickAction();
}

bool AXMenuListOption::OnNativeSetSelectedAction(bool b) {
  if (!GetElement() || !CanSetSelectedAttribute())
    return false;

  To<HTMLOptionElement>(GetElement())->SetSelected(b);
  return true;
}

// TODO(aleventhal) This override could go away, but it will cause a lot of
// test changes, as invisible options inside of a collapsed <select> will become
// ignored since they have no layout object.
bool AXMenuListOption::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  if (IsDetached()) {
    NOTREACHED();
    return true;
  }

  if (DynamicTo<HTMLOptionElement>(GetNode())->FastHasAttribute(
          html_names::kHiddenAttr))
    return true;

  if (IsAriaHidden()) {
    if (ignored_reasons) {
      ComputeIsAriaHidden(ignored_reasons);
    }
    return true;
  }

  return ParentObject()->ComputeAccessibilityIsIgnored(ignored_reasons);
}

void AXMenuListOption::GetRelativeBounds(
    AXObject** out_container,
    gfx::RectF& out_bounds_in_container,
    gfx::Transform& out_container_transform,
    bool* clips_children) const {
  DCHECK(!IsDetached());
  *out_container = nullptr;
  out_bounds_in_container = gfx::RectF();
  out_container_transform.MakeIdentity();

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

String AXMenuListOption::TextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
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
      recursive, aria_label_or_description_root, visited, name_from,
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

}  // namespace blink
