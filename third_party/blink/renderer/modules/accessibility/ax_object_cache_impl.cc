/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_progress_indicator.h"
#include "third_party/blink/renderer/modules/accessibility/ax_radio_input.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"
#include "third_party/blink/renderer/modules/accessibility/ax_svg_root.h"
#include "third_party/blink/renderer/modules/accessibility/ax_validation_message.h"
#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_role_properties.h"

// Prevent code that runs during the lifetime of the stack from altering the
// document lifecycle. Usually doc is the same as document_, but it can be
// different when it is a popup document. Because it's harmless to test both
// documents, even if they are the same, the scoped check is initialized for
// both documents.
// clang-format off
#if DCHECK_IS_ON()
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)                        \
  DocumentLifecycle::DisallowTransitionScope scoped1((document).Lifecycle()); \
  DocumentLifecycle::DisallowTransitionScope scoped2(document_->Lifecycle())
#else
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)
#endif  // DCHECK_IS_ON()
// clang-format on

namespace blink {

namespace {

// Return a node for the current layout object or ancestor layout object.
Node* GetClosestNodeForLayoutObject(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  Node* node = layout_object->GetNode();
  return node ? node : GetClosestNodeForLayoutObject(layout_object->Parent());
}

bool IsActive(Document& document) {
  return document.IsActive() && !document.IsDetached();
}

}  // namespace

// static
AXObjectCache* AXObjectCacheImpl::Create(Document& document) {
  return MakeGarbageCollected<AXObjectCacheImpl>(document);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document)
    : document_(document),
      modification_count_(0),
      validation_message_axid_(0),
      active_aria_modal_dialog_(nullptr),
      relation_cache_(std::make_unique<AXRelationCache>(this)),
      accessibility_event_permission_(mojom::blink::PermissionStatus::ASK),
      permission_service_(document.GetExecutionContext()),
      permission_observer_receiver_(this, document.GetExecutionContext()) {
  if (document_->LoadEventFinished())
    AddPermissionStatusListener();
  documents_.insert(&document);
  use_ax_menu_list_ = GetSettings()->GetUseAXMenuList();

  // Perform last, to ensure AXObjectCacheImpl() is fully set up, as
  // AXRelationCache() sometimes calls back into AXObjectCacheImpl.
  relation_cache_->Init();
}

AXObjectCacheImpl::~AXObjectCacheImpl() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void AXObjectCacheImpl::Dispose() {
  for (auto& entry : objects_) {
    AXObject* obj = entry.value;
    obj->Detach();
    RemoveAXID(obj);
  }

  permission_observer_receiver_.reset();

#if DCHECK_IS_ON()
  has_been_disposed_ = true;
#endif
}

AXObject* AXObjectCacheImpl::Root() {
  return GetOrCreate(document_);
}

void AXObjectCacheImpl::InitializePopup(Document* document) {
  if (!document || documents_.Contains(document) || !document->View())
    return;

  documents_.insert(document);
}

void AXObjectCacheImpl::DisposePopup(Document* document) {
  if (!documents_.Contains(document) || !document->View())
    return;

  documents_.erase(document);
}

Node* AXObjectCacheImpl::FocusedElement() {
  Node* focused_node = document_->FocusedElement();
  if (!focused_node)
    focused_node = document_;

  // If it's an image map, get the focused link within the image map.
  if (IsA<HTMLAreaElement>(focused_node))
    return focused_node;

  // See if there's a page popup, for example a calendar picker.
  Element* adjusted_focused_element = document_->AdjustedFocusedElement();
  if (auto* input = DynamicTo<HTMLInputElement>(adjusted_focused_element)) {
    if (AXObject* ax_popup = input->PopupRootAXObject()) {
      if (Element* focused_element_in_popup =
              ax_popup->GetDocument()->FocusedElement())
        focused_node = focused_element_in_popup;
    }
  }

  return focused_node;
}

AXObject* AXObjectCacheImpl::GetOrCreateFocusedObjectFromNode(Node* node) {
  // If it's an image map, get the focused link within the image map.
  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return FocusedImageMapUIElement(area);

  if (node->GetDocument() != GetDocument() &&
      node->GetDocument().Lifecycle().GetState() <
          DocumentLifecycle::kLayoutClean) {
    // Node is in a different, unclean document. This can occur in an open
    // popup. Ensure the popup document has a clean layout before trying to
    // create an AXObject from a node in it.
    if (node->GetDocument().View()) {
      node->GetDocument()
          .View()
          ->UpdateLifecycleToCompositingCleanPlusScrolling(
              DocumentUpdateReason::kAccessibility);
    }
  }

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return nullptr;

  // the HTML element, for example, is focusable but has an AX object that is
  // ignored
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectIncludedInTree();

  return obj;
}

AXObject* AXObjectCacheImpl::FocusedImageMapUIElement(
    HTMLAreaElement* area_element) {
  // Find the corresponding accessibility object for the HTMLAreaElement. This
  // should be in the list of children for its corresponding image.
  if (!area_element)
    return nullptr;

  HTMLImageElement* image_element = area_element->ImageElement();
  if (!image_element)
    return nullptr;

  AXObject* ax_layout_image = GetOrCreate(image_element);
  if (!ax_layout_image)
    return nullptr;

  const AXObject::AXObjectVector& image_children =
      ax_layout_image->ChildrenIncludingIgnored();
  unsigned count = image_children.size();
  for (unsigned k = 0; k < count; ++k) {
    AXObject* child = image_children[k];
    auto* ax_object = DynamicTo<AXImageMapLink>(child);
    if (!ax_object)
      continue;

    if (ax_object->AreaElement() == area_element)
      return child;
  }

  return nullptr;
}

AXObject* AXObjectCacheImpl::FocusedObject() {
  return GetOrCreateFocusedObjectFromNode(this->FocusedElement());
}

AXObject* AXObjectCacheImpl::Get(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  AXID ax_id = layout_object_mapping_.at(layout_object);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));

  Node* node = layout_object->GetNode();
  if (node && DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    // It's in a locked subtree so we need to search by node instead of by
    // layout object.
    if (ax_id) {
      // We previously saved the node in the cache with its layout object,
      // but now it's in a locked subtree so we should remove the entry with its
      // layout object and replace it with an AXNodeObject created from the node
      // instead.
      Remove(ax_id);
      return GetOrCreate(node);
    }
    return Get(node);
  }

  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

// Returns true if |node| is an <option> element and its parent <select>
// is a menu list (not a list box).
static bool IsMenuListOption(const Node* node) {
  auto* option_element = DynamicTo<HTMLOptionElement>(node);
  if (!option_element)
    return false;
  const HTMLSelectElement* select = option_element->OwnerSelectElement();
  if (!select || !select->UsesMenuList())
    return false;
  return select->GetLayoutObject();
}

// TODO(aleventhal) Remove side effects or rename, e.g. GetUpdated().
AXObject* AXObjectCacheImpl::Get(const Node* node) {
  if (!node)
    return nullptr;

  // Menu list option and HTML area elements are indexed by DOM node, never by
  // layout object.
  LayoutObject* layout_object = node->GetLayoutObject();
  if (IsMenuListOption(node) || IsA<HTMLAreaElement>(node))
    layout_object = nullptr;

  AXID layout_id = layout_object ? layout_object_mapping_.at(layout_object) : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));

  AXID node_id = node_object_mapping_.at(node);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));

  if (layout_object &&
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    // The node is in a display locked subtree, but we've previously put it in
    // the cache with its layout object.
    if (layout_id) {
      Remove(layout_id);
      layout_id = 0;
    }
    layout_object = nullptr;
  }

  if (layout_object && node_id && !layout_id && !IsMenuListOption(node) &&
      !IsA<HTMLAreaElement>(node)) {
    // Has a layout object but no layout_id, meaning that when the AXObject was
    // originally created only for Node*, the LayoutObject* didn't exist yet.
    // This can happen if an AXNodeObject is created for a node that's not laid
    // out, but later something changes and it gets a layoutObject (like if it's
    // reparented). It's also possible the layout object changed.
    // In any case, reuse the ax_id since the node didn't change.
    Remove(node_id);

    // Note that this codepath can be reached when |layout_object| is about to
    // be destroyed.

    // This potentially misses root LayoutObject re-creation, but we have no way
    // of knowing whether the |layout_object| in those cases is still valid.
    if (!layout_object->Parent())
      return nullptr;

    layout_object_mapping_.Set(layout_object, node_id);
    AXObject* new_obj = CreateFromRenderer(layout_object);
    ids_in_use_.insert(node_id);
    new_obj->SetAXObjectID(node_id);
    objects_.Set(node_id, new_obj);
    new_obj->Init();
    new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
    new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
        new_obj->AccessibilityIsIgnoredButIncludedInTree());
    return new_obj;
  }

  if (layout_id)
    return objects_.at(layout_id);

  if (!node_id)
    return nullptr;

  return objects_.at(node_id);
}

AXObject* AXObjectCacheImpl::Get(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  AXID ax_id = inline_text_box_object_mapping_.at(inline_text_box);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

AXID AXObjectCacheImpl::GetAXID(Node* node) {
  AXObject* ax_object = GetOrCreate(node);
  if (!ax_object)
    return 0;
  return ax_object->AXObjectID();
}

Element* AXObjectCacheImpl::GetElementFromAXID(AXID axid) {
  AXObject* ax_object = ObjectFromAXID(axid);
  if (!ax_object || !ax_object->GetElement())
    return nullptr;
  return ax_object->GetElement();
}

AXObject* AXObjectCacheImpl::Get(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return nullptr;

  AXID ax_id = accessible_node_mapping_.at(accessible_node);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes g_null_atom.
static bool NodeHasRole(Node* node, const String& role) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  // TODO(accessibility) support role strings with multiple roles.
  return EqualIgnoringASCIICase(
      element->FastGetAttribute(html_names::kRoleAttr), role);
}

AXObject* AXObjectCacheImpl::CreateFromRenderer(LayoutObject* layout_object) {
  // FIXME: How could layoutObject->node() ever not be an Element?
  Node* node = layout_object->GetNode();

  // If the node is aria role="list" or the aria role is empty and its a
  // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
  if (NodeHasRole(node, "list") || NodeHasRole(node, "directory") ||
      (NodeHasRole(node, g_null_atom) &&
       (IsA<HTMLUListElement>(node) || IsA<HTMLOListElement>(node) ||
        IsA<HTMLDListElement>(node))))
    return MakeGarbageCollected<AXList>(layout_object, *this);

  // media element
  if (node && node->IsMediaElement())
    return AccessibilityMediaElement::Create(layout_object, *this);

  if (IsA<HTMLOptionElement>(node))
    return MakeGarbageCollected<AXListBoxOption>(layout_object, *this);

  if (auto* html_input_element = DynamicTo<HTMLInputElement>(node)) {
    const AtomicString& type = html_input_element->type();
    if (type == input_type_names::kRadio)
      return MakeGarbageCollected<AXRadioInput>(layout_object, *this);
    if (type == input_type_names::kRange)
      return MakeGarbageCollected<AXSlider>(layout_object, *this);
  }

  if (layout_object->IsSVGRoot())
    return MakeGarbageCollected<AXSVGRoot>(layout_object, *this);

  if (layout_object->IsBoxModelObject()) {
    auto* css_box = To<LayoutBoxModelObject>(layout_object);
    if (auto* select_element = DynamicTo<HTMLSelectElement>(node)) {
      if (select_element->UsesMenuList()) {
        if (use_ax_menu_list_)
          return MakeGarbageCollected<AXMenuList>(css_box, *this);
      } else {
        return MakeGarbageCollected<AXListBox>(css_box, *this);
      }
    }

    // progress bar
    if (css_box->IsProgress()) {
      return MakeGarbageCollected<AXProgressIndicator>(
          To<LayoutProgress>(css_box), *this);
    }
  }

  return MakeGarbageCollected<AXLayoutObject>(layout_object, *this);
}

AXObject* AXObjectCacheImpl::CreateFromNode(Node* node) {
  if (IsMenuListOption(node) && use_ax_menu_list_) {
    return MakeGarbageCollected<AXMenuListOption>(To<HTMLOptionElement>(node),
                                                  *this);
  }

  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return MakeGarbageCollected<AXImageMapLink>(area, *this);

  return MakeGarbageCollected<AXNodeObject>(node, *this);
}

AXObject* AXObjectCacheImpl::CreateFromInlineTextBox(
    AbstractInlineTextBox* inline_text_box) {
  return MakeGarbageCollected<AXInlineTextBox>(inline_text_box, *this);
}

AXObject* AXObjectCacheImpl::GetOrCreate(AccessibleNode* accessible_node) {
  if (AXObject* obj = Get(accessible_node))
    return obj;

  AXObject* new_obj =
      MakeGarbageCollected<AXVirtualObject>(*this, accessible_node);
  const AXID ax_id = GetOrCreateAXID(new_obj);
  accessible_node_mapping_.Set(accessible_node, ax_id);

  new_obj->Init();
  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(const Node* node) {
  return GetOrCreate(const_cast<Node*>(node));
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node) {
  if (!node || !node->isConnected())
    return nullptr;

  if (!node->IsElementNode() && !node->IsTextNode() && !node->IsDocumentNode())
    return nullptr;  // Only documents, elements and text nodes get a11y objects

  if (AXObject* obj = Get(node))
    return obj;

#if DCHECK_IS_ON()
  Document* document = &node->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  // If the node has a layout object, prefer using that as the primary key for
  // the AXObject, with the exception of the HTMLAreaElement and nodes within
  // a locked subtree, which are created based on its node.
  if (node->GetLayoutObject() && !IsA<HTMLAreaElement>(node) &&
      !DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    return GetOrCreate(node->GetLayoutObject());
  }

  if (!LayoutTreeBuilderTraversal::Parent(*node))
    return nullptr;

  if (IsA<HTMLHeadElement>(node))
    return nullptr;

  AXObject* new_obj = CreateFromNode(node);

  // Will crash later if we have two objects for the same node.
  DCHECK(!Get(node));

  const AXID ax_id = GetOrCreateAXID(new_obj);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  node_object_mapping_.Set(node, ax_id);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  MaybeNewRelationTarget(node, new_obj);

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  if (AXObject* obj = Get(layout_object))
    return obj;

#if DCHECK_IS_ON()
  Document* document = &layout_object->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  // Area elements are never created based on layout objects (see |Get|), so we
  // really should never get here.
  Node* node = layout_object->GetNode();
  if (node && (IsMenuListOption(node) || IsA<HTMLAreaElement>(node)))
    return nullptr;

  // Prefer creating AXNodeObjects over AXLayoutObjects in locked subtrees
  // (e.g. content-visibility: auto), even if a LayoutObject is available,
  // because the LayoutObject is not guaranteed to be up-to-date (it might come
  // from a previous layout update), or even it is up-to-date, it may not remain
  // up-to-date. Blink doesn't update style/layout for nodes in locked
  // subtrees, so creating a matching AXLayoutObjects could lead to the use of
  // old information. Note that Blink will recreate the AX objects as
  // AXLayoutObjects when a locked element is activated, aka it becomes visible.
  // Visit https://wicg.github.io/display-locking/#accessibility for more info.
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*layout_object)) {
    if (!node) {
      // Nodeless objects such as anonymous blocks do not get accessible objects
      // in a locked subtree. Anonymous blocks are added to help layout when
      // a block and inline are siblings.
      // This prevents an odd mixture of ax objects in a locked subtree, e.g.
      // AXNodeObjects when there is a node, and AXLayoutObjects
      // when there isn't. The locked subtree should not have AXLayoutObjects.
      return nullptr;
    }
    return GetOrCreate(layout_object->GetNode());
  }

  AXObject* new_obj = CreateFromRenderer(layout_object);

  // Will crash later if we have two objects for the same layoutObject.
  DCHECK(!Get(layout_object));

  const AXID axid = GetOrCreateAXID(new_obj);

  layout_object_mapping_.Set(layout_object, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  if (node && node->GetLayoutObject() == layout_object) {
    AXID prev_axid = node_object_mapping_.at(node);
    if (prev_axid != 0 && prev_axid != axid) {
      Remove(prev_axid);
      node_object_mapping_.Set(node, axid);
    }
    MaybeNewRelationTarget(node, new_obj);
  }

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(
    AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  if (AXObject* obj = Get(inline_text_box))
    return obj;

  AXObject* new_obj = CreateFromInlineTextBox(inline_text_box);

  // Will crash later if we have two objects for the same inlineTextBox.
  DCHECK(!Get(inline_text_box));

  const AXID axid = GetOrCreateAXID(new_obj);

  inline_text_box_object_mapping_.Set(inline_text_box, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(ax::mojom::blink::Role role) {
  AXObject* obj = nullptr;

  switch (role) {
    case ax::mojom::Role::kSliderThumb:
      obj = MakeGarbageCollected<AXSliderThumb>(*this);
      break;
    case ax::mojom::Role::kMenuListPopup:
      DCHECK(use_ax_menu_list_);
      obj = MakeGarbageCollected<AXMenuListPopup>(*this);
      break;
    default:
      obj = nullptr;
  }

  if (!obj)
    return nullptr;

  GetOrCreateAXID(obj);

  obj->Init();
  return obj;
}

ContainerNode* FindParentTable(Node* node) {
  ContainerNode* parent = node->parentNode();
  while (parent && !IsA<HTMLTableElement>(*parent))
    parent = parent->parentNode();
  return parent;
}

void AXObjectCacheImpl::ContainingTableRowsOrColsMaybeChanged(Node* node) {
  // Any containing table must recompute its rows and columns on insertion or
  // removal of a <tr> or <td>.
  // Get parent table from DOM, because AXObject/layout tree are incomplete.
  ContainerNode* containing_table = nullptr;
  if (IsA<HTMLTableCellElement>(node) || IsA<HTMLTableRowElement>(node))
    containing_table = FindParentTable(node);

  if (containing_table) {
    AXObject* ax_table = Get(containing_table);
    if (ax_table)
      ax_table->SetNeedsToUpdateChildren();
  }
}

void AXObjectCacheImpl::InvalidateTableSubtree(AXObject* subtree) {
  if (!subtree)
    return;

  LayoutObject* layout_object = subtree->GetLayoutObject();
  if (layout_object) {
    LayoutObject* layout_child = layout_object->SlowFirstChild();
    while (layout_child) {
      InvalidateTableSubtree(Get(layout_child));
      layout_child = layout_child->NextSibling();
    }
  }

  AXID ax_id = subtree->AXObjectID();
  Remove(ax_id);
}

void AXObjectCacheImpl::Remove(AXID ax_id) {
  if (!ax_id)
    return;

  // First, fetch object to operate some cleanup functions on it.
  AXObject* obj = objects_.at(ax_id);
  if (!obj)
    return;

  obj->Detach();
  RemoveAXID(obj);

  // Finally, remove the object.
  if (!objects_.Take(ax_id))
    return;

  DCHECK_GE(objects_.size(), ids_in_use_.size());
}

void AXObjectCacheImpl::Remove(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;

  AXID ax_id = accessible_node_mapping_.at(accessible_node);
  Remove(ax_id);
  accessible_node_mapping_.erase(accessible_node);
}

void AXObjectCacheImpl::Remove(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  AXID ax_id = layout_object_mapping_.at(layout_object);
  Remove(ax_id);
  layout_object_mapping_.erase(layout_object);
}

void AXObjectCacheImpl::Remove(Node* node) {
  if (!node)
    return;

  // This is all safe even if we didn't have a mapping.
  AXID ax_id = node_object_mapping_.at(node);
  Remove(ax_id);
  node_object_mapping_.erase(node);

  if (node->GetLayoutObject())
    Remove(node->GetLayoutObject());
}

void AXObjectCacheImpl::Remove(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return;

  AXID ax_id = inline_text_box_object_mapping_.at(inline_text_box);
  Remove(ax_id);
  inline_text_box_object_mapping_.erase(inline_text_box);
}

AXID AXObjectCacheImpl::GenerateAXID() const {
  static AXID last_used_id = 0;

  // Generate a new ID.
  AXID obj_id = last_used_id;
  do {
    ++obj_id;
  } while (!obj_id || HashTraits<AXID>::IsDeletedValue(obj_id) ||
           ids_in_use_.Contains(obj_id));

  last_used_id = obj_id;

  return obj_id;
}

void AXObjectCacheImpl::AddToFixedOrStickyNodeList(const AXObject* object) {
  DCHECK(object);
  DCHECK(!object->IsDetached());
  fixed_or_sticky_node_ids_.insert(object->AXObjectID());
}

AXID AXObjectCacheImpl::GetOrCreateAXID(AXObject* obj) {
  // check for already-assigned ID
  const AXID existing_axid = obj->AXObjectID();
  if (existing_axid) {
    DCHECK(ids_in_use_.Contains(existing_axid));
    return existing_axid;
  }

  const AXID new_axid = GenerateAXID();

  ids_in_use_.insert(new_axid);
  obj->SetAXObjectID(new_axid);
  objects_.Set(new_axid, obj);

  return new_axid;
}

void AXObjectCacheImpl::RemoveAXID(AXObject* object) {
  if (!object)
    return;

  fixed_or_sticky_node_ids_.clear();

  if (active_aria_modal_dialog_ == object)
    active_aria_modal_dialog_ = nullptr;

  AXID obj_id = object->AXObjectID();
  if (!obj_id)
    return;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(obj_id));
  DCHECK(ids_in_use_.Contains(obj_id));
  object->SetAXObjectID(0);
  ids_in_use_.erase(obj_id);
  autofill_state_map_.erase(obj_id);

  relation_cache_->RemoveAXID(obj_id);
}

AXObject* AXObjectCacheImpl::NearestExistingAncestor(Node* node) {
  // Find the nearest ancestor that already has an accessibility object, since
  // we might be in the middle of a layout.
  while (node) {
    if (AXObject* obj = Get(node))
      return obj;
    node = node->parentNode();
  }
  return nullptr;
}

AXObject::InOrderTraversalIterator AXObjectCacheImpl::InOrderTraversalBegin() {
  AXObject* root = Root();
  if (root)
    return AXObject::InOrderTraversalIterator(*root);
  return InOrderTraversalEnd();
}

AXObject::InOrderTraversalIterator AXObjectCacheImpl::InOrderTraversalEnd() {
  return AXObject::InOrderTraversalIterator();
}

void AXObjectCacheImpl::UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram() {
  UMA_HISTOGRAM_COUNTS_100000(
      "Blink.Accessibility.NumTreeUpdatesQueuedBeforeLayout",
      tree_update_callback_queue_.size());
}

void AXObjectCacheImpl::InvalidateBoundingBoxForFixedOrStickyPosition() {
  for (AXID id : fixed_or_sticky_node_ids_)
    changed_bounds_ids_.insert(id);
}

void AXObjectCacheImpl::DeferTreeUpdateInternal(base::OnceClosure callback,
                                                AXObject* obj) {
  // Called for updates that do not have a DOM node, e.g. a children or text
  // changed event that occurs on an anonymous layout block flow.
  DCHECK(obj);

  if (!IsActive(GetDocument()) || tree_updates_paused_)
    return;

  if (obj->IsDetached())
    return;

  Document* tree_update_document = obj->GetDocument();

  // Ensure the tree update document is in a good state.
  if (!tree_update_document || !IsActive(*tree_update_document))
    return;

  if (tree_update_callback_queue_.size() >= max_pending_updates_) {
    UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

    tree_updates_paused_ = true;
    tree_update_callback_queue_.clear();
    return;
  }

  DCHECK(!tree_update_document->GetPage()->Animator().IsServicingAnimations() ||
         (tree_update_document->Lifecycle().GetState() <
              DocumentLifecycle::kInAccessibility ||
          tree_update_document->Lifecycle().StateAllowsDetach()))
      << "DeferTreeUpdateInternal should only be outside of the lifecycle or "
         "before the accessibility state.";
  tree_update_callback_queue_.push_back(MakeGarbageCollected<TreeUpdateParams>(
      obj->GetNode(), obj->AXObjectID(), ComputeEventFrom(),
      ActiveEventIntents(), std::move(callback)));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a document lifecycle update scheduled.
  ScheduleVisualUpdate();
}

void AXObjectCacheImpl::DeferTreeUpdateInternal(base::OnceClosure callback,
                                                const Node* node) {
  DCHECK(node);

  if (!IsActive(GetDocument()) || tree_updates_paused_)
    return;

  Document& tree_update_document = node->GetDocument();

  // Ensure the tree update document is in a good state.
  if (!IsActive(tree_update_document))
    return;

  if (tree_update_callback_queue_.size() >= max_pending_updates_) {
    UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

    tree_updates_paused_ = true;
    tree_update_callback_queue_.clear();
    return;
  }

  DCHECK(!tree_update_document.GetPage()->Animator().IsServicingAnimations() ||
         (tree_update_document.Lifecycle().GetState() <
              DocumentLifecycle::kInAccessibility ||
          tree_update_document.Lifecycle().StateAllowsDetach()))
      << "DeferTreeUpdateInternal should only be outside of the lifecycle or "
         "before the accessibility state.";
  tree_update_callback_queue_.push_back(MakeGarbageCollected<TreeUpdateParams>(
      node, 0, ComputeEventFrom(), ActiveEventIntents(), std::move(callback)));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a document lifecycle update scheduled.
  ScheduleVisualUpdate();
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(const Node*),
    const Node* node) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node));
  DeferTreeUpdateInternal(std::move(callback), node);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*),
    Node* node) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node));
  DeferTreeUpdateInternal(std::move(callback), node);
}

// TODO(accessibility) PostNotification calls made when layout is unclean should
// use this instead, in order to avoid potentially unsafe calls to Get(), which
// can call CreateFromRenderer(). For an example, see CheckedStateChanged().
void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node* node,
                                      ax::mojom::blink::Event event),
    Node* node,
    ax::mojom::blink::Event event) {
  base::OnceClosure callback = WTF::Bind(method, WrapWeakPersistent(this),
                                         WrapWeakPersistent(node), event);
  DeferTreeUpdateInternal(std::move(callback), node);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(const QualifiedName&, Element* element),
    const QualifiedName& attr_name,
    Element* element) {
  base::OnceClosure callback = WTF::Bind(
      method, WrapWeakPersistent(this), attr_name, WrapWeakPersistent(element));
  DeferTreeUpdateInternal(std::move(callback), element);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*, AXObject*),
    Node* node,
    AXObject* obj) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node),
                WrapWeakPersistent(obj));
  if (obj) {
    DCHECK_EQ(node, obj->GetNode());
    DeferTreeUpdateInternal(std::move(callback), obj);
  } else {
    DeferTreeUpdateInternal(std::move(callback), node);
  }
}

void AXObjectCacheImpl::SelectionChanged(Node* node) {
  if (!node)
    return;

  Settings* settings = GetSettings();
  if (settings && settings->GetAriaModalPrunesAXTree())
    UpdateActiveAriaModalDialog(node);

  DeferTreeUpdate(&AXObjectCacheImpl::SelectionChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::SelectionChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  // Something about the call chain for this method seems to leave distribution
  // in a dirty state - update it before we call GetOrCreate so that we don't
  // crash.
  node->UpdateDistributionForFlatTreeTraversal();
  AXObject* ax_object = GetOrCreate(node);
  if (ax_object)
    ax_object->SelectionChanged();
}

void AXObjectCacheImpl::UpdateReverseRelations(
    const AXObject* relation_source,
    const Vector<String>& target_ids) {
  relation_cache_->UpdateReverseRelations(relation_source, target_ids);
}

void AXObjectCacheImpl::StyleChanged(const LayoutObject* layout_object) {
  DCHECK(layout_object);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node)
    DeferTreeUpdate(&AXObjectCacheImpl::StyleChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::StyleChangedWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // There is a ton of style change notifications coming from newly-opened
  // calendar popups for pickers. Solving that problem is what inspired the
  // approach below, which is likely true for all elements.
  //
  // If we don't know about an object, then its style did not change as far as
  // we (and ATs) are concerned. For this reason, don't call GetOrCreate.
  AXObject* obj = Get(node);
  if (!obj)
    return;

  DCHECK(!obj->IsDetached());

  // If the foreground or background color on an item inside a container which
  // supports selection changes, it can be the result of the selection changing
  // as well as the container losing focus. We handle these notifications via
  // their state changes, so no need to mark them dirty here.
  AXObject* parent = obj->CachedParentObject();
  if (parent && ui::IsContainerWithSelectableChildren(parent->RoleValue()))
    return;

  MarkAXObjectDirty(obj, false);
}

void AXObjectCacheImpl::TextChanged(Node* node) {
  if (!node)
    return;

  // A text changed event is redundant with children changed on the same node.
  if (nodes_with_pending_children_changed_.find(node) !=
      nodes_with_pending_children_changed_.end()) {
    return;
  }

  DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::TextChanged(const LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // The node may be null when the text changes on an anonymous layout object,
  // such as a layout block flow that is inserted to parent an inline object
  // when it has a block sibling.
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    // A text changed event is redundant with children changed on the same node.
    if (nodes_with_pending_children_changed_.find(node) !=
        nodes_with_pending_children_changed_.end()) {
      return;
    }

    DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
    return;
  }

  if (Get(layout_object)) {
    DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, nullptr,
                    Get(layout_object));
  }
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(
    Node* optional_node_for_relation_update,
    AXObject* obj) {
  if (obj ? obj->IsDetached() : !optional_node_for_relation_update)
    return;

#if DCHECK_IS_ON()
  Document* document = obj ? obj->GetDocument()
                           : &optional_node_for_relation_update->GetDocument();
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (obj) {
    if (obj->RoleValue() == ax::mojom::blink::Role::kStaticText) {
      Settings* settings = GetSettings();
      if (settings && settings->GetInlineTextBoxAccessibilityEnabled()) {
        // Update inline text box children.
        ChildrenChangedWithCleanLayout(optional_node_for_relation_update, obj);
        return;
      }
    }

    MarkAXObjectDirty(obj, /*subtree=*/false);
  }

  if (optional_node_for_relation_update)
    relation_cache_->UpdateRelatedTree(optional_node_for_relation_update);
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  TextChangedWithCleanLayout(node, Get(node));
}

void AXObjectCacheImpl::FocusableChangedWithCleanLayout(Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  if (obj->AriaHiddenRoot()) {
    // Elements that are hidden but focusable are not ignored. Therefore, if a
    // hidden element's focusable state changes, it's ignored state must be
    // recomputed.
    ChildrenChangedWithCleanLayout(element->parentNode());
  }

  // Refresh the focusable state and State::kIgnored on the exposed object.
  MarkAXObjectDirty(obj, false);
}

void AXObjectCacheImpl::DocumentTitleChanged() {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());

  AXObject* root = Get(document_);
  if (root)
    PostNotification(root, ax::mojom::blink::Event::kDocumentTitleChanged);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttached(Node* node) {
  DCHECK(node);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  DeferTreeUpdate(
      &AXObjectCacheImpl::UpdateCacheAfterNodeIsAttachedWithCleanLayout, node);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttachedWithCleanLayout(
    Node* node) {
  if (!node || !node->isConnected())
    return;

  Element* element = DynamicTo<Element>(node);
  if (!element)
    return;

  Document* document = &node->GetDocument();
  if (!document)
    return;

#if DCHECK_IS_ON()
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  // Process any relation attributes that can affect ax objects already created.

  // Force computation of aria-owns, so that original parents that already
  // computed their children get the aria-owned children removed.
  if (element->FastHasAttribute(html_names::kAriaOwnsAttr) ||
      element->HasExplicitlySetAttrAssociatedElements(
          html_names::kAriaOwnsAttr)) {
    HandleAttributeChangedWithCleanLayout(html_names::kAriaOwnsAttr, element);
  }

  MaybeNewRelationTarget(node, Get(node));
}

void AXObjectCacheImpl::DidInsertChildrenOfNode(Node* node) {
  // If a node is inserted that is a descendant of a leaf node in the
  // accessibility tree, notify the root of that subtree that its children have
  // changed.
  DCHECK(node);
  DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::DidInsertChildrenOfNodeWithCleanLayout(Node* node) {
  if (!node)
    return;

#if DCHECK_IS_ON()
  Document* document = &node->GetDocument();
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (AXObject* obj = Get(node)) {
    TextChangedWithCleanLayout(node);
  } else {
    DidInsertChildrenOfNodeWithCleanLayout(NodeTraversal::Parent(*node));
  }
}

void AXObjectCacheImpl::ChildrenChanged(Node* node) {
  if (!node)
    return;

  // Don't enqueue a deferred event on the same node more than once.
  if (!nodes_with_pending_children_changed_.insert(node).is_new_entry)
    return;

  DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::ChildrenChanged(const LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // Update using nearest node (walking ancestors if necessary).
  Node* node = GetClosestNodeForLayoutObject(layout_object);

  if (node) {
    // Don't enqueue a deferred event on the same node more than once.
    if (!nodes_with_pending_children_changed_.insert(node).is_new_entry)
      return;

    DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, node);

    if (layout_object->GetNode() == node)
      return;  // Node matched the layout object passed in, no further updates.

    // Node was for an ancestor of an anonymous layout object passed in.
    // layout object was anonymous. Fall through to continue updating
    // descendants of the matching AXObject for the layout object.
  }

  // Update using layout object.
  // Only using the layout object when no node could be found to update.
  AXObject* ax_layout_obj = Get(layout_object);
  if (!ax_layout_obj)
    return;

  if (ax_layout_obj->LastKnownIsIncludedInTreeValue()) {
    // Participates in tree: update children if they haven't already been.
    DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout,
                    ax_layout_obj->GetNode(), ax_layout_obj);
  }

  // Invalidate child ax objects below an anonymous layout object.
  // The passed-in layout object was anonymous, e.g. anonymous block flow
  // inserted by blink as an inline's parent when it had a block sibling.
  // If children change on an anonymous layout object, this can
  // mean that child AXObjects actually had their children change.
  // Therefore, invalidate any of those children as well, using the nearest
  // parent that participates in the tree.
  // In this example, if ChildrenChanged() is called on the anonymous block,
  // then we also process ChildrenChanged() on the <div> and <a>:
  // <div>
  //  |    \
  // <p>  Anonymous block
  //         \
  //         <a>
  //           \
  //           text
  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, child);
  }
}

void AXObjectCacheImpl::ChildrenChanged(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;

  AXObject* object = Get(accessible_node);
  if (!object)
    return;
  DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout,
                  object->GetNode(), object);
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  ChildrenChangedWithCleanLayout(node, Get(node));
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* optional_node,
                                                       AXObject* obj) {
  if (obj ? obj->IsDetached() : !optional_node)
    return;

#if DCHECK_IS_ON()
  Document* document = obj ? obj->GetDocument() : &optional_node->GetDocument();
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (obj && !obj->IsDetached())
    obj->ChildrenChanged();

  if (optional_node) {
    ContainingTableRowsOrColsMaybeChanged(optional_node);
    relation_cache_->UpdateRelatedTree(optional_node);
  }
}

void AXObjectCacheImpl::ProcessDeferredAccessibilityEvents(Document& document) {
  TRACE_EVENT0("accessibility", "ProcessDeferredAccessibilityEvents");

  if (document.Lifecycle().GetState() != DocumentLifecycle::kInAccessibility) {
    DCHECK(false) << "Deferred events should only be processed during the "
                     "accessibility document lifecycle";
    return;
  }

  ProcessUpdates(document);

  // Changes to ids or aria-owns may have resulted in queued up relation
  // cache work; do that now.
  relation_cache_->ProcessUpdatesWithCleanLayout();

  PostNotifications(document);
}

bool AXObjectCacheImpl::IsDirty() const {
  return !tree_updates_paused_ &&
         (tree_update_callback_queue_.size() || notifications_to_post_.size());
}

void AXObjectCacheImpl::EmbeddingTokenChanged(HTMLFrameOwnerElement* element) {
  if (!element)
    return;

  MarkElementDirty(element, false);
}

void AXObjectCacheImpl::ProcessUpdates(Document& document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document);

  if (tree_updates_paused_) {
    ChildrenChangedWithCleanLayout(nullptr, GetOrCreate(&document));
    tree_updates_paused_ = false;
    return;
  }

  UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  TreeUpdateCallbackQueue old_tree_update_callback_queue;
  tree_update_callback_queue_.swap(old_tree_update_callback_queue);
  nodes_with_pending_children_changed_.clear();

  for (auto& tree_update : old_tree_update_callback_queue) {
    const Node* node = tree_update->node;
    AXID axid = tree_update->axid;

    // Need either an DOM node or an AXObject to be a valid update.
    // These may have been destroyed since the original update occurred.
    if (!node) {
      if (!axid || !ObjectFromAXID(axid))
        return;
    }
    base::OnceClosure& callback = tree_update->callback;
    // Insure the update is for the correct document.
    // If no node, this update must be from an AXObject with no DOM node,
    // such as an AccessibleNode. In that case, ensure the update is in the
    // main document.
    Document& tree_update_document = node ? node->GetDocument() : GetDocument();
    if (document != tree_update_document) {
      tree_update_callback_queue_.push_back(
          MakeGarbageCollected<TreeUpdateParams>(
              node, axid, tree_update->event_from, tree_update->event_intents,
              std::move(callback)));
      continue;
    }

    FireTreeUpdatedEventImmediately(document, tree_update->event_from,
                                    tree_update->event_intents,
                                    std::move(callback));
  }
}

void AXObjectCacheImpl::PostNotifications(Document& document) {
  HeapVector<Member<AXEventParams>> old_notifications_to_post;
  notifications_to_post_.swap(old_notifications_to_post);
  for (auto& params : old_notifications_to_post) {
    AXObject* obj = params->target;

    if (!obj || !obj->AXObjectID())
      continue;

    if (obj->IsDetached())
      continue;

    ax::mojom::blink::Event event_type = params->event_type;
    ax::mojom::blink::EventFrom event_from = params->event_from;
    const BlinkAXEventIntentsSet& event_intents = params->event_intents;
    if (obj->GetDocument() != &document) {
      notifications_to_post_.push_back(MakeGarbageCollected<AXEventParams>(
          obj, event_type, event_from, event_intents));
      continue;
    }

    FireAXEventImmediately(obj, event_type, event_from, event_intents);
  }
}

void AXObjectCacheImpl::PostNotification(const LayoutObject* layout_object,
                                         ax::mojom::blink::Event notification) {
  if (!layout_object)
    return;
  PostNotification(Get(layout_object), notification);
}

// TODO(accessibility) Eventually replace direct calls during unclean layout
// with deferred calls.
void AXObjectCacheImpl::PostNotification(Node* node,
                                         ax::mojom::blink::Event notification) {
  if (!node)
    return;
  PostNotification(Get(node), notification);
}

void AXObjectCacheImpl::EnsurePostNotification(
    Node* node,
    ax::mojom::blink::Event notification) {
  if (!node)
    return;
  PostNotification(GetOrCreate(node), notification);
}

void AXObjectCacheImpl::PostNotification(AXObject* object,
                                         ax::mojom::blink::Event event_type) {
  if (!object || !object->AXObjectID() || object->IsDetached())
    return;

  modification_count_++;

  // It's possible for FireAXEventImmediately to post another notification.
  // If we're still in the accessibility document lifecycle, fire these events
  // immediately rather than deferring them.
  if (object->GetDocument()->Lifecycle().GetState() ==
      DocumentLifecycle::kInAccessibility) {
    FireAXEventImmediately(object, event_type, ComputeEventFrom(),
                           ActiveEventIntents());
    return;
  }

  notifications_to_post_.push_back(MakeGarbageCollected<AXEventParams>(
      object, event_type, ComputeEventFrom(), ActiveEventIntents()));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a visual update scheduled.
  ScheduleVisualUpdate();
}

void AXObjectCacheImpl::ScheduleVisualUpdate() {
  // Scheduling visual updates before the document is finished loading can
  // interfere with event ordering.
  if (!GetDocument().IsLoadCompleted())
    return;

  // If there was a document change that doesn't trigger a lifecycle update on
  // its own, (e.g. because it doesn't make layout dirty), make sure we run
  // lifecycle phases to update the computed accessibility tree.
  LocalFrameView* frame_view = GetDocument().View();
  Page* page = GetDocument().GetPage();
  if (!frame_view || !page)
    return;

  if (!frame_view->CanThrottleRendering() &&
      (!GetDocument().GetPage()->Animator().IsServicingAnimations() ||
       GetDocument().Lifecycle().GetState() >=
           DocumentLifecycle::kInAccessibility)) {
    page->Animator().ScheduleVisualUpdate(GetDocument().GetFrame());
  }
}

void AXObjectCacheImpl::FireTreeUpdatedEventImmediately(
    Document& document,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents,
    base::OnceClosure callback) {
  DCHECK_EQ(document.Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

  base::AutoReset<ax::mojom::blink::EventFrom> event_from_resetter(
      &active_event_from_, event_from);
  ScopedBlinkAXEventIntent defered_event_intents(event_intents.AsVector(),
                                                 &document);
  std::move(callback).Run();
}

void AXObjectCacheImpl::FireAXEventImmediately(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents) {
  DCHECK_EQ(obj->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

#if DCHECK_IS_ON()
  // Make sure none of the layout views are in the process of being laid out.
  // Notifications should only be sent after the layoutObject has finished
  auto* ax_layout_object = DynamicTo<AXLayoutObject>(obj);
  if (ax_layout_object) {
    LayoutObject* layout_object = ax_layout_object->GetLayoutObject();
    if (layout_object && layout_object->View())
      DCHECK(!layout_object->View()->GetLayoutState());
  }

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*obj->GetDocument());
#endif  // DCHECK_IS_ON()

  PostPlatformNotification(obj, event_type, event_from, event_intents);

  if (event_type == ax::mojom::blink::Event::kChildrenChanged &&
      obj->CachedParentObject()) {
    const bool was_ignored = obj->LastKnownIsIgnoredValue();
    const bool was_ignored_but_included_in_tree =
        obj->LastKnownIsIgnoredButIncludedInTreeValue();
    bool is_ignored_changed =
        was_ignored != obj->AccessibilityIsIgnored() ||
        was_ignored_but_included_in_tree !=
            obj->AccessibilityIsIgnoredButIncludedInTree();
    if (is_ignored_changed)
      ChildrenChangedWithCleanLayout(nullptr, obj->CachedParentObject());
  }
}

bool AXObjectCacheImpl::IsAriaOwned(const AXObject* object) const {
  return relation_cache_->IsAriaOwned(object);
}

AXObject* AXObjectCacheImpl::GetAriaOwnedParent(const AXObject* object) const {
  return relation_cache_->GetAriaOwnedParent(object);
}

void AXObjectCacheImpl::GetAriaOwnedChildren(
    const AXObject* owner,
    HeapVector<Member<AXObject>>& owned_children) {
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
  relation_cache_->GetAriaOwnedChildren(owner, owned_children);
}

bool AXObjectCacheImpl::MayHaveHTMLLabel(const HTMLElement& elem) {
  // Return false if this type of element will not accept a <label for> label.
  if (!elem.IsLabelable())
    return false;

  // Return true if a <label for> pointed to this element at some point.
  if (relation_cache_->MayHaveHTMLLabelViaForAttribute(elem))
    return true;

  // Return true if any amcestor is a label, as in <label><input></label>.
  return Traversal<HTMLLabelElement>::FirstAncestor(elem);
}

void AXObjectCacheImpl::CheckedStateChanged(Node* node) {
  DeferTreeUpdate(&AXObjectCacheImpl::PostNotification, node,
                  ax::mojom::blink::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxOptionStateChanged(HTMLOptionElement* option) {
  PostNotification(option, ax::mojom::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxSelectedChildrenChanged(
    HTMLSelectElement* select) {
  PostNotification(select, ax::mojom::Event::kSelectedChildrenChanged);
}

void AXObjectCacheImpl::ListboxActiveIndexChanged(HTMLSelectElement* select) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(select->GetDocument());

  auto* ax_object = DynamicTo<AXListBox>(Get(select));
  if (!ax_object)
    return;

  ax_object->ActiveIndexChanged();
}

void AXObjectCacheImpl::LocationChanged(const LayoutObject* layout_object) {
  // No need to send this notification if the object is aria-hidden.
  // Note that if the node is ignored for other reasons, it still might
  // be important to send this notification if any of its children are
  // visible - but in the case of aria-hidden we can safely ignore it.
  AXObject* obj = Get(layout_object);
  if (obj && obj->AriaHiddenRoot())
    return;

  PostNotification(layout_object, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::RadiobuttonRemovedFromGroup(
    HTMLInputElement* group_member) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(group_member->GetDocument());

  auto* ax_object = DynamicTo<AXRadioInput>(Get(group_member));
  if (!ax_object)
    return;

  // The 'posInSet' and 'setSize' attributes should be updated from the first
  // node, as the removed node is already detached from tree.
  auto* first_radio = ax_object->FindFirstRadioButtonInGroup(group_member);
  AXObject* first_obj = Get(first_radio);
  auto* ax_first_obj = DynamicTo<AXRadioInput>(first_obj);
  if (!ax_first_obj)
    return;

  ax_first_obj->UpdatePosAndSetSize(1);
  PostNotification(first_obj, ax::mojom::Event::kAriaAttributeChanged);
  ax_first_obj->RequestUpdateToNextNode(true);
}

void AXObjectCacheImpl::ImageLoaded(const LayoutObject* layout_object) {
  AXObject* obj = Get(layout_object);
  MarkAXObjectDirty(obj, false);
}

void AXObjectCacheImpl::HandleClicked(Node* node) {
  if (AXObject* obj = Get(node))
    PostNotification(obj, ax::mojom::Event::kClicked);
}

void AXObjectCacheImpl::HandleAttributeChanged(
    const QualifiedName& attr_name,
    AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;
  modification_count_++;
  if (AXObject* obj = Get(accessible_node))
    PostNotification(obj, ax::mojom::Event::kAriaAttributeChanged);
}

void AXObjectCacheImpl::HandleAriaExpandedChangeWithCleanLayout(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  if (AXObject* obj = GetOrCreate(node))
    obj->HandleAriaExpandedChanged();
}

void AXObjectCacheImpl::HandleAriaSelectedChangedWithCleanLayout(Node* node) {
  DCHECK(node);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kCheckedStateChanged);

  AXObject* listbox = obj->ParentObjectUnignored();
  if (listbox && listbox->RoleValue() == ax::mojom::Role::kListBox) {
    // Ensure listbox options are in sync as selection status may have changed
    MarkAXObjectDirty(listbox, true);
    PostNotification(listbox, ax::mojom::Event::kSelectedChildrenChanged);
  }
}

void AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  TRACE_EVENT1("accessibility",
               "AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout", "id",
               obj->AXObjectID());
  PostNotification(obj, ax::mojom::Event::kBlur);
}

void AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout(Node* node) {
  node = FocusedElement();  // Needs to get this with clean layout.
  if (!node || !node->GetDocument().View())
    return;
  node->UpdateDistributionForFlatTreeTraversal();

  if (node->GetDocument().NeedsLayoutTreeUpdateForNode(*node)) {
    // This should only occur when focus goes into a popup document. The main
    // document has an updated layout, but the popup does not.
    // TODO(accessibility) design callback system so that popup processing
    // can occur with a clean layout.
    DCHECK_NE(document_, node->GetDocument());
    node->GetDocument().View()->UpdateLifecycleToCompositingCleanPlusScrolling(
        DocumentUpdateReason::kAccessibility);
  }

  AXObject* obj = GetOrCreateFocusedObjectFromNode(node);
  if (!obj)
    return;

  TRACE_EVENT1("accessibility",
               "AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout", "id",
               obj->AXObjectID());
  PostNotification(obj, ax::mojom::Event::kFocus);
}

// This might be the new target of a relation. Handle all possible cases.
void AXObjectCacheImpl::MaybeNewRelationTarget(Node* node, AXObject* obj) {
  // Track reverse relations
  relation_cache_->UpdateRelatedTree(node);

  if (!obj)
    return;

  // Check whether aria-activedescendant on a focused object points to |obj|.
  // If so, fire activedescendantchanged event now.
  // This is only for ARIA active descendants, not in a native control like a
  // listbox, which has its own initial active descendant handling.
  Node* focused_node = document_->FocusedElement();
  if (focused_node) {
    AXObject* focus = Get(focused_node);
    if (focus && focus->ActiveDescendant() == obj &&
        obj->CanBeActiveDescendant())
      focus->HandleActiveDescendantChanged();
  }
}

void AXObjectCacheImpl::HandleActiveDescendantChangedWithCleanLayout(
    Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  // Changing the active descendant should trigger recomputing all
  // cached values even if it doesn't result in a notification, because
  // it can affect what's focusable or not.
  modification_count_++;

  if (AXObject* obj = GetOrCreate(node))
    obj->HandleActiveDescendantChanged();
}

// Be as safe as possible about changes that could alter the accessibility role,
// as this may require a different subclass of AXObject.
// Role changes are disallowed by the spec but we must handle it gracefully, see
// https://www.w3.org/TR/wai-aria-1.1/#h-roles for more information.
void AXObjectCacheImpl::HandleRoleChangeWithCleanLayout(Node* node) {
  if (!node)
    return;  // Virtual AOM node.

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Invalidate the current object and make the parent reconsider its children.
  if (AXObject* obj = GetOrCreate(node)) {
    // Save parent for later use.
    AXObject* parent = obj->ParentObject();

    // If role changes on a table, invalidate the entire table subtree as many
    // objects may suddenly need to change, because presentation is inherited
    // from the table to rows and cells.
    LayoutObject* layout_object = node->GetLayoutObject();
    if (layout_object && layout_object->IsTable())
      InvalidateTableSubtree(obj);
    else
      Remove(node);

    // Parent object changed children, as the previous AXObject for this node
    // was destroyed and a different one was created in its place.
    if (parent)
      ChildrenChangedWithCleanLayout(parent->GetNode(), parent);
    modification_count_++;
  }
}

void AXObjectCacheImpl::HandleRoleChangeIfNotEditableWithCleanLayout(
    Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Do not invalidate object if the role doesn't actually change when it's a
  // text control, otherwise unique id will change on platform side, and confuse
  // some screen readers as user edits.
  // TODO(aleventhal) Ideally the text control check would be removed, and
  // HandleRoleChangeWithCleanLayout() and only ever invalidate when the role
  // actually changes. For example:
  // if (obj->RoleValue() == obj->ComputeAccessibilityRole())
  //   return;
  // However, doing that would require
  // waiting for layout to complete, as ComputeAccessibilityRole() looks at
  // layout objects.
  AXObject* obj = Get(node);
  if (!obj || !obj->IsTextControl())
    HandleRoleChangeWithCleanLayout(node);
}

void AXObjectCacheImpl::HandleAttributeChanged(const QualifiedName& attr_name,
                                               Element* element) {
  DCHECK(element);
  DeferTreeUpdate(&AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout,
                  attr_name, element);
}

void AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout(
    const QualifiedName& attr_name,
    Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  if (attr_name == html_names::kRoleAttr ||
      attr_name == html_names::kTypeAttr) {
    HandleRoleChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kSizeAttr ||
             attr_name == html_names::kAriaHaspopupAttr) {
    // Role won't change on edits.
    HandleRoleChangeIfNotEditableWithCleanLayout(element);
  } else if (attr_name == html_names::kAltAttr ||
             attr_name == html_names::kTitleAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kForAttr &&
             IsA<HTMLLabelElement>(*element)) {
    LabelChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kIdAttr) {
    MaybeNewRelationTarget(element, Get(element));
  } else if (attr_name == html_names::kTabindexAttr) {
    FocusableChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kDisabledAttr ||
             attr_name == html_names::kReadonlyAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kValueAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kMinAttr ||
             attr_name == html_names::kMaxAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kStepAttr) {
    MarkElementDirty(element, false);
  }

  if (!attr_name.LocalName().StartsWith("aria-"))
    return;

  // Perform updates specific to each attribute.
  if (attr_name == html_names::kAriaActivedescendantAttr) {
    HandleActiveDescendantChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaValuenowAttr ||
             attr_name == html_names::kAriaValuetextAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kAriaLabelAttr ||
             attr_name == html_names::kAriaLabeledbyAttr ||
             attr_name == html_names::kAriaLabelledbyAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaDescriptionAttr ||
             attr_name == html_names::kAriaDescribedbyAttr) {
    // TODO do we need a DescriptionChanged() ?
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaCheckedAttr ||
             attr_name == html_names::kAriaPressedAttr) {
    PostNotification(element, ax::mojom::blink::Event::kCheckedStateChanged);
  } else if (attr_name == html_names::kAriaSelectedAttr) {
    HandleAriaSelectedChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaExpandedAttr) {
    HandleAriaExpandedChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaHiddenAttr) {
    ChildrenChangedWithCleanLayout(element->parentNode());
  } else if (attr_name == html_names::kAriaInvalidAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kAriaErrormessageAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kAriaOwnsAttr) {
    if (AXObject* obj = GetOrCreate(element))
      relation_cache_->UpdateAriaOwnsWithCleanLayout(obj);
  } else {
    PostNotification(element, ax::mojom::Event::kAriaAttributeChanged);
  }
}

AXObject* AXObjectCacheImpl::GetOrCreateValidationMessageObject() {
  AXObject* message_ax_object = nullptr;
  // Create only if it does not already exist.
  if (validation_message_axid_) {
    message_ax_object = ObjectFromAXID(validation_message_axid_);
  }
  if (!message_ax_object) {
    message_ax_object = MakeGarbageCollected<AXValidationMessage>(*this);
    DCHECK(message_ax_object);
    // Cache the validation message container for reuse.
    validation_message_axid_ = GetOrCreateAXID(message_ax_object);
    message_ax_object->Init();
    // Validation message alert object is a child of the document, as not all
    // form controls can have a child. Also, there are form controls such as
    // listbox that technically can have children, but they are probably not
    // expected to have alerts within AT client code.
    ChildrenChanged(document_);
  }
  return message_ax_object;
}

AXObject* AXObjectCacheImpl::ValidationMessageObjectIfInvalid() {
  Element* focused_element = document_->FocusedElement();
  if (focused_element) {
    ListedElement* form_control = ListedElement::From(*focused_element);
    if (form_control && !form_control->IsNotCandidateOrValid()) {
      // These must both be true:
      // * Focused control is currently invalid.
      // * Validation message was previously created but hidden
      // from timeout or currently visible.
      bool was_validation_message_already_created = validation_message_axid_;
      if (was_validation_message_already_created ||
          form_control->IsValidationMessageVisible()) {
        AXObject* focused_object = FocusedObject();
        if (focused_object) {
          // Return as long as the focused form control isn't overriding with a
          // different message via aria-errormessage.
          bool override_native_validation_message =
              focused_object->GetAOMPropertyOrARIAAttribute(
                  AOMRelationProperty::kErrorMessage);
          if (!override_native_validation_message) {
            AXObject* message = GetOrCreateValidationMessageObject();
            if (message && !was_validation_message_already_created)
              ChildrenChanged(document_);
            return message;
          }
        }
      }
    }
  }

  // No focused, invalid form control.
  RemoveValidationMessageObject();
  return nullptr;
}

void AXObjectCacheImpl::RemoveValidationMessageObject() {
  if (validation_message_axid_) {
    // Remove when it becomes hidden, so that a new object is created the next
    // time the message becomes visible. It's not possible to reuse the same
    // alert, because the event generator will not generate an alert event if
    // the same object is hidden and made visible quickly, which occurs if the
    // user submits the form when an alert is already visible.
    Remove(validation_message_axid_);
    validation_message_axid_ = 0;
    ChildrenChanged(document_);
  }
}

// Native validation error popup for focused form control in current document.
void AXObjectCacheImpl::HandleValidationMessageVisibilityChanged(
    const Node* form_control) {
  DCHECK(form_control);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(form_control->GetDocument());

  DeferTreeUpdate(&AXObjectCacheImpl::
                      HandleValidationMessageVisibilityChangedWithCleanLayout,
                  form_control);
}

void AXObjectCacheImpl::HandleValidationMessageVisibilityChangedWithCleanLayout(
    const Node* form_control) {
#if DCHECK_IS_ON()
  DCHECK(form_control);
  Document* document = &form_control->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  AXObject* message_ax_object = ValidationMessageObjectIfInvalid();
  if (message_ax_object)
    MarkAXObjectDirty(message_ax_object, false);  // May be invisible now.

  // If the form control is invalid, it will now have an error message relation
  // to the message container.
  MarkElementDirty(form_control, false);
}

void AXObjectCacheImpl::HandleEventListenerAdded(
    const Node& node,
    const AtomicString& event_type) {
  // If this is the first |event_type| listener for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 1)
    HandleEventSubscriptionChanged(node, event_type);
}

void AXObjectCacheImpl::HandleEventListenerRemoved(
    const Node& node,
    const AtomicString& event_type) {
  // If there are no more |event_type| listeners for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 0)
    HandleEventSubscriptionChanged(node, event_type);
}

bool AXObjectCacheImpl::DoesEventListenerImpactIgnoredState(
    const AtomicString& event_type) const {
  return event_util::IsMouseButtonEventType(event_type);
}

void AXObjectCacheImpl::HandleEventSubscriptionChanged(
    const Node& node,
    const AtomicString& event_type) {
  // Adding or Removing an event listener for certain events may affect whether
  // a node or its descendants should be accessibility ignored.
  if (!DoesEventListenerImpactIgnoredState(event_type))
    return;

  // If the |event_type| may affect the ignored state of |node|, invalidate all
  // cached values then mark |node| dirty so it may reconsider its accessibility
  // ignored state.
  modification_count_++;
  MarkElementDirty(&node, /*subtree=*/false);
}

void AXObjectCacheImpl::LabelChangedWithCleanLayout(Element* element) {
  // Will call back to TextChanged() when done updating relation cache.
  relation_cache_->LabelChanged(element);
}

void AXObjectCacheImpl::InlineTextBoxesUpdated(
    LineLayoutItem line_layout_item) {
  if (!InlineTextBoxAccessibilityEnabled())
    return;

  LayoutObject* layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(line_layout_item);

  // Only update if the accessibility object already exists and it's
  // not already marked as dirty.
  if (AXObject* obj = Get(layout_object)) {
    if (!obj->NeedsToUpdateChildren()) {
      obj->SetNeedsToUpdateChildren();
      PostNotification(layout_object, ax::mojom::Event::kChildrenChanged);
    }
  }
}

Settings* AXObjectCacheImpl::GetSettings() {
  return document_->GetSettings();
}

bool AXObjectCacheImpl::InlineTextBoxAccessibilityEnabled() {
  Settings* settings = this->GetSettings();
  if (!settings)
    return false;
  return settings->GetInlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::RootAXEditableElement(const Node* node) {
  const Element* result = RootEditableElement(*node);
  const auto* element = DynamicTo<Element>(node);
  if (!element)
    element = node->parentElement();

  for (; element; element = element->parentElement()) {
    if (NodeIsTextControl(element))
      result = element;
  }

  return result;
}

AXObject* AXObjectCacheImpl::FirstAccessibleObjectFromNode(const Node* node) {
  if (!node)
    return nullptr;

  AXObject* accessible_object = GetOrCreate(node->GetLayoutObject());
  while (accessible_object &&
         !accessible_object->AccessibilityIsIncludedInTree()) {
    node = NodeTraversal::Next(*node);

    while (node && !node->GetLayoutObject())
      node = NodeTraversal::NextSkippingChildren(*node);

    if (!node)
      return nullptr;

    accessible_object = GetOrCreate(node->GetLayoutObject());
  }

  return accessible_object;
}

bool AXObjectCacheImpl::NodeIsTextControl(const Node* node) {
  if (!node)
    return false;

  const AXObject* ax_object = GetOrCreate(const_cast<Node*>(node));
  return ax_object && ax_object->IsTextControl();
}

bool IsNodeAriaVisible(Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  bool is_null = true;
  bool hidden = AccessibleNode::GetPropertyOrARIAAttribute(
      element, AOMBooleanProperty::kHidden, is_null);
  return !is_null && !hidden;
}

void AXObjectCacheImpl::PostPlatformNotification(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents) {
  if (!document_ || !document_->View() ||
      !document_->View()->GetFrame().GetPage()) {
    return;
  }

  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (web_frame && web_frame->Client()) {
    ui::AXEvent event;
    event.id = obj->AXObjectID();
    event.event_type = event_type;
    event.event_from = event_from;
    event.event_intents.resize(event_intents.size());
    // We need to filter out the counts from every intent.
    std::transform(event_intents.begin(), event_intents.end(),
                   event.event_intents.begin(),
                   [](const auto& intent) { return intent.key.intent(); });

    web_frame->Client()->PostAccessibilityEvent(event);
  }
}

void AXObjectCacheImpl::MarkAXObjectDirty(AXObject* obj, bool subtree) {
  if (!obj || !document_ || !document_->View() ||
      !document_->View()->GetFrame().GetPage())
    return;

  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (webframe && webframe->Client())
    webframe->Client()->MarkWebAXObjectDirty(WebAXObject(obj), subtree);
}

void AXObjectCacheImpl::MarkElementDirty(const Node* element, bool subtree) {
  // Warning, if no AXObject exists for element, nothing is marked dirty,
  // including descendant objects when subtree == true.
  MarkAXObjectDirty(Get(element), subtree);
}

void AXObjectCacheImpl::HandleFocusedUIElementChanged(
    Element* old_focused_element,
    Element* new_focused_element) {
  TRACE_EVENT0("accessibility",
               "AXObjectCacheImpl::HandleFocusedUIElementChanged");
#if DCHECK_IS_ON()
  // The focus can be in a different document when a popup is open.
  Document& focused_doc =
      new_focused_element ? new_focused_element->GetDocument() : *document_;
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(focused_doc);
#endif  // DCHECK_IS_ON()

  RemoveValidationMessageObject();

  if (!new_focused_element) {
    // When focus is cleared, implicitly focus the document by sending a blur.
    if (GetDocument().documentElement()) {
      DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                      GetDocument().documentElement());
    }
    return;
  }

  Page* page = new_focused_element->GetDocument().GetPage();
  if (!page)
    return;

  if (old_focused_element) {
    DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                    old_focused_element);
  }

  Settings* settings = GetSettings();
  if (settings && settings->GetAriaModalPrunesAXTree())
    UpdateActiveAriaModalDialog(new_focused_element);

  DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout,
                  this->FocusedElement());
}

// Check if the focused node is inside an active aria-modal dialog. If so, we
// should mark the cache as dirty to recompute the ignored status of each node.
void AXObjectCacheImpl::UpdateActiveAriaModalDialog(Node* node) {
  AXObject* new_active_aria_modal = AncestorAriaModalDialog(node);
  if (active_aria_modal_dialog_ == new_active_aria_modal)
    return;

  active_aria_modal_dialog_ = new_active_aria_modal;
  modification_count_++;
  MarkAXObjectDirty(Root(), true);
}

AXObject* AXObjectCacheImpl::AncestorAriaModalDialog(Node* node) {
  for (Element* ancestor = Traversal<Element>::FirstAncestorOrSelf(*node);
       ancestor; ancestor = Traversal<Element>::FirstAncestor(*ancestor)) {
    if (!ancestor->FastHasAttribute(html_names::kAriaModalAttr))
      continue;

    AtomicString aria_modal =
        ancestor->FastGetAttribute(html_names::kAriaModalAttr);
    if (!EqualIgnoringASCIICase(aria_modal, "true")) {
      continue;
    }

    AXObject* ancestor_ax_object = GetOrCreate(ancestor);
    ax::mojom::blink::Role ancestor_role = ancestor_ax_object->RoleValue();

    if (!ui::IsDialog(ancestor_role))
      continue;

    return ancestor_ax_object;
  }
  return nullptr;
}

AXObject* AXObjectCacheImpl::GetActiveAriaModalDialog() const {
  return active_aria_modal_dialog_;
}

HeapVector<Member<AXObject>>
AXObjectCacheImpl::GetAllObjectsWithChangedBounds() {
  VectorOf<AXObject> changed_bounds_objects;
  changed_bounds_objects.ReserveCapacity(changed_bounds_ids_.size());
  for (AXID changed_bounds_id : changed_bounds_ids_) {
    if (AXObject* obj = ObjectFromAXID(changed_bounds_id))
      changed_bounds_objects.push_back(obj);
  }
  changed_bounds_ids_.clear();
  return changed_bounds_objects;
}

void AXObjectCacheImpl::HandleInitialFocus() {
  PostNotification(document_, ax::mojom::Event::kFocus);
}

void AXObjectCacheImpl::HandleEditableTextContentChanged(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = nullptr;
  // We shouldn't create a new AX object here because we might be in the middle
  // of a layout.
  do {
    obj = Get(node);
  } while (!obj && (node = node->parentNode()));
  if (!obj)
    return;

  while (obj && !obj->IsNativeTextControl() && !obj->IsNonNativeTextControl())
    obj = obj->ParentObject();
  PostNotification(obj, ax::mojom::Event::kValueChanged);
}

void AXObjectCacheImpl::HandleScaleAndLocationChanged(Document* document) {
  if (!document)
    return;
  PostNotification(document, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::HandleTextFormControlChanged(Node* node) {
  HandleEditableTextContentChanged(node);
}

void AXObjectCacheImpl::HandleTextMarkerDataAdded(Node* start, Node* end) {
  if (!start || !end)
    return;

  // Notify the client of new text marker data.
  ChildrenChanged(start);
  if (start != end) {
    ChildrenChanged(end);
  }
}

void AXObjectCacheImpl::HandleValueChanged(Node* node) {
  PostNotification(node, ax::mojom::Event::kValueChanged);

  // If it's a slider, invalidate the thumb's bounding box.
  AXObject* ax_object = Get(node);
  if (ax_object && ax_object->RoleValue() == ax::mojom::blink::Role::kSlider &&
      ax_object->HasChildren() && !ax_object->NeedsToUpdateChildren() &&
      ax_object->ChildCountIncludingIgnored() == 1) {
    changed_bounds_ids_.insert(
        ax_object->ChildAtIncludingIgnored(0)->AXObjectID());
  }
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOption(LayoutObject* menu_list,
                                                     int option_index) {
  if (!use_ax_menu_list_) {
    MarkAXObjectDirty(Get(menu_list), false);
    return;
  }

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (!ax_object)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*ax_object->GetDocument());

  ax_object->DidUpdateActiveOption(option_index);
}

void AXObjectCacheImpl::DidShowMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  DCHECK(menu_list->GetNode());
  DeferTreeUpdate(&AXObjectCacheImpl::DidShowMenuListPopupWithCleanLayout,
                  menu_list->GetNode());
}

void AXObjectCacheImpl::DidShowMenuListPopupWithCleanLayout(Node* menu_list) {
  if (!use_ax_menu_list_) {
    MarkAXObjectDirty(Get(menu_list), false);
    return;
  }

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidShowPopup();
}

void AXObjectCacheImpl::DidHideMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  DCHECK(menu_list->GetNode());
  DeferTreeUpdate(&AXObjectCacheImpl::DidHideMenuListPopupWithCleanLayout,
                  menu_list->GetNode());
}

void AXObjectCacheImpl::DidHideMenuListPopupWithCleanLayout(Node* menu_list) {
  if (!use_ax_menu_list_) {
    MarkAXObjectDirty(Get(menu_list), false);
    return;
  }

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidHidePopup();
}

void AXObjectCacheImpl::HandleLoadComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);

  AddPermissionStatusListener();
  DeferTreeUpdate(&AXObjectCacheImpl::HandleLoadCompleteWithCleanLayout,
                  document);
}

void AXObjectCacheImpl::HandleLoadCompleteWithCleanLayout(Node* document_node) {
  DCHECK(document_node);
  DCHECK(IsA<Document>(document_node));
#if DCHECK_IS_ON()
  Document* document = To<Document>(document_node);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  AddPermissionStatusListener();
  PostNotification(GetOrCreate(document_node),
                   ax::mojom::blink::Event::kLoadComplete);
}

void AXObjectCacheImpl::HandleLayoutComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);
  if (document->Lifecycle().GetState() >=
      DocumentLifecycle::kAfterPerformLayout) {
    PostNotification(GetOrCreate(document),
                     ax::mojom::blink::Event::kLayoutComplete);
  } else {
    DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, document,
                    ax::mojom::blink::Event::kLayoutComplete);
  }
}

void AXObjectCacheImpl::HandleScrolledToAnchor(const Node* anchor_node) {
  if (!anchor_node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(anchor_node->GetDocument());

  AXObject* obj = GetOrCreate(anchor_node->GetLayoutObject());
  if (!obj)
    return;
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectUnignored();
  PostNotification(obj, ax::mojom::Event::kScrolledToAnchor);
}

void AXObjectCacheImpl::HandleFrameRectsChanged(Document& document) {
  MarkAXObjectDirty(Get(&document), false);
}

void AXObjectCacheImpl::InvalidateBoundingBox(
    const LayoutObject* layout_object) {
  if (AXObject* obj = Get(const_cast<LayoutObject*>(layout_object)))
    changed_bounds_ids_.insert(obj->AXObjectID());
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LocalFrameView* frame_view) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*frame_view->GetFrame().GetDocument());

  InvalidateBoundingBoxForFixedOrStickyPosition();
  MarkElementDirty(document_, false);
  DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, document_,
                  ax::mojom::blink::Event::kLayoutComplete);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LayoutObject* layout_object) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());
  InvalidateBoundingBoxForFixedOrStickyPosition();
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    MarkElementDirty(node, false);
    DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, node,
                    ax::mojom::blink::Event::kLayoutComplete);
  }
}

const AtomicString& AXObjectCacheImpl::ComputedRoleForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return AXObject::RoleName(ax::mojom::Role::kUnknown);
  return AXObject::RoleName(obj->RoleValue());
}

String AXObjectCacheImpl::ComputedNameForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return "";

  return obj->ComputedName();
}

void AXObjectCacheImpl::OnTouchAccessibilityHover(const IntPoint& location) {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());
  AXObject* hit = Root()->AccessibilityHitTest(location);
  if (hit) {
    // Ignore events on a frame or plug-in, because the touch events
    // will be re-targeted there and we don't want to fire duplicate
    // accessibility events.
    if (hit->GetLayoutObject() &&
        hit->GetLayoutObject()->IsLayoutEmbeddedContent())
      return;

    PostNotification(hit, ax::mojom::Event::kHover);
  }
}

void AXObjectCacheImpl::SetCanvasObjectBounds(HTMLCanvasElement* canvas,
                                              Element* element,
                                              const LayoutRect& rect) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(element->GetDocument());

  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  AXObject* ax_canvas = GetOrCreate(canvas);
  if (!ax_canvas)
    return;

  obj->SetElementRect(rect, ax_canvas);
}

void AXObjectCacheImpl::AddPermissionStatusListener() {
  if (!document_->GetExecutionContext())
    return;

  // Passing an Origin to Mojo crashes if the host is empty because
  // blink::SecurityOrigin sets unique to false, but url::Origin sets
  // unique to true. This only happens for some obscure corner cases
  // like on Android where the system registers unusual protocol handlers,
  // and we don't need any special permissions in those cases.
  //
  // http://crbug.com/759528 and http://crbug.com/762716
  if (document_->Url().Protocol() != "file" &&
      document_->Url().Host().IsEmpty()) {
    return;
  }

  if (permission_service_.is_bound())
    permission_service_.reset();

  ConnectToPermissionService(
      document_->GetExecutionContext(),
      permission_service_.BindNewPipeAndPassReceiver(
          document_->GetTaskRunner(TaskType::kUserInteraction)));

  if (permission_observer_receiver_.is_bound())
    permission_observer_receiver_.reset();

  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      document_->GetTaskRunner(TaskType::kUserInteraction));
  permission_service_->AddPermissionObserver(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      accessibility_event_permission_, std::move(observer));
}

void AXObjectCacheImpl::OnPermissionStatusChange(
    mojom::PermissionStatus status) {
  accessibility_event_permission_ = status;
}

bool AXObjectCacheImpl::CanCallAOMEventListeners() const {
  return accessibility_event_permission_ == mojom::PermissionStatus::GRANTED;
}

void AXObjectCacheImpl::RequestAOMEventListenerPermission() {
  if (accessibility_event_permission_ != mojom::PermissionStatus::ASK)
    return;

  if (!permission_service_.is_bound())
    return;

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      LocalFrame::HasTransientUserActivation(document_->GetFrame()),
      WTF::Bind(&AXObjectCacheImpl::OnPermissionStatusChange,
                WrapPersistent(this)));
}

void AXObjectCacheImpl::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(accessible_node_mapping_);
  visitor->Trace(node_object_mapping_);
  visitor->Trace(active_aria_modal_dialog_);

  visitor->Trace(objects_);
  visitor->Trace(notifications_to_post_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receiver_);
  visitor->Trace(documents_);
  visitor->Trace(tree_update_callback_queue_);
  visitor->Trace(nodes_with_pending_children_changed_);
  AXObjectCache::Trace(visitor);
}

ax::mojom::blink::EventFrom AXObjectCacheImpl::ComputeEventFrom() {
  if (active_event_from_ != ax::mojom::blink::EventFrom::kNone)
    return active_event_from_;

  if (document_ && document_->View() &&
      LocalFrame::HasTransientUserActivation(
          &(document_->View()->GetFrame()))) {
    return ax::mojom::blink::EventFrom::kUser;
  }

  return ax::mojom::blink::EventFrom::kPage;
}

WebAXAutofillState AXObjectCacheImpl::GetAutofillState(AXID id) const {
  auto iter = autofill_state_map_.find(id);
  if (iter == autofill_state_map_.end())
    return WebAXAutofillState::kNoSuggestions;
  return iter->value;
}

void AXObjectCacheImpl::SetAutofillState(AXID id, WebAXAutofillState state) {
  WebAXAutofillState previous_state = GetAutofillState(id);
  if (state != previous_state) {
    autofill_state_map_.Set(id, state);
    MarkAXObjectDirty(ObjectFromAXID(id), false);
  }
}

}  // namespace blink
