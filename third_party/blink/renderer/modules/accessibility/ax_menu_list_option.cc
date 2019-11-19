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
    : AXMockObject(ax_object_cache), element_(element) {}

AXMenuListOption::~AXMenuListOption() {
  DCHECK(!element_);
}

void AXMenuListOption::Detach() {
  element_ = nullptr;
  AXMockObject::Detach();
}

LocalFrameView* AXMenuListOption::DocumentFrameView() const {
  if (IsDetached())
    return nullptr;
  return element_->GetDocument().View();
}

ax::mojom::Role AXMenuListOption::RoleValue() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsEmpty())
    return ax::mojom::Role::kMenuListOption;

  ax::mojom::Role role = AriaRoleToWebCoreRole(aria_role);
  if (role != ax::mojom::Role::kUnknown)
    return role;
  return ax::mojom::Role::kMenuListOption;
}

Element* AXMenuListOption::ActionElement() const {
  return element_;
}

AXObject* AXMenuListOption::ComputeParent() const {
  Node* node = GetNode();
  if (!node)
    return nullptr;
  auto* select = To<HTMLOptionElement>(node)->OwnerSelectElement();
  if (!select)
    return nullptr;
  AXObject* select_ax_object = AXObjectCache().GetOrCreate(select);
  if (!select_ax_object)
    return nullptr;

  // This happens if the <select> is not rendered. Return it and move on.
  if (!select_ax_object->IsMenuList())
    return select_ax_object;

  AXMenuList* menu_list = ToAXMenuList(select_ax_object);
  if (menu_list->HasChildren()) {
    const auto& child_objects = menu_list->Children();
    if (child_objects.IsEmpty())
      return nullptr;
    DCHECK_EQ(child_objects.size(), 1UL);
    DCHECK(child_objects[0]->IsMenuListPopup());
    ToAXMenuListPopup(child_objects[0].Get())->UpdateChildrenIfNecessary();
  } else {
    menu_list->UpdateChildrenIfNecessary();
  }
  return parent_.Get();
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

int AXMenuListOption::PosInSet() const {
  // Value should be 1-based. 0 means not supported.
  return SetSize() ? element_->index() + 1 : 0;
}

int AXMenuListOption::SetSize() const {
  // Return 0 if not supported.
  if (!element_)
    return 0;
  HTMLSelectElement* select = element_->OwnerSelectElement();
  if (!select)
    return 0;
  return select->length();
}

AccessibilitySelectedState AXMenuListOption::IsSelected() const {
  if (!GetNode() || !CanSetSelectedAttribute())
    return kSelectedStateUndefined;

  AXMenuListPopup* parent = static_cast<AXMenuListPopup*>(ParentObject());
  if (parent && !parent->IsOffScreen()) {
    return ((parent->ActiveDescendant() == this) ? kSelectedStateTrue
                                                 : kSelectedStateFalse);
  }
  return ((element_ && element_->Selected()) ? kSelectedStateTrue
                                             : kSelectedStateFalse);
}

bool AXMenuListOption::OnNativeSetSelectedAction(bool b) {
  if (!element_ || !CanSetSelectedAttribute())
    return false;

  element_->SetSelected(b);
  return true;
}

bool AXMenuListOption::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

void AXMenuListOption::GetRelativeBounds(AXObject** out_container,
                                         FloatRect& out_bounds_in_container,
                                         SkMatrix44& out_container_transform,
                                         bool* clips_children) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  AXObject* parent = ParentObject();
  if (!parent)
    return;
  DCHECK(parent->IsMenuListPopup());

  AXObject* grandparent = parent->ParentObject();
  if (!grandparent)
    return;
  DCHECK(grandparent->IsMenuList());
  grandparent->GetRelativeBounds(out_container, out_bounds_in_container,
                                 out_container_transform, clips_children);
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
  text_alternative = element_->DisplayLabel();
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

void AXMenuListOption::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  AXMockObject::Trace(visitor);
}

}  // namespace blink
