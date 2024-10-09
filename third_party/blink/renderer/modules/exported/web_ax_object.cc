/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_ax_object.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/accessibility/ax_action_data.h"

namespace blink {

namespace {
mojom::blink::ScrollAlignment::Behavior ToBlinkScrollAlignmentBehavior(
    ax::mojom::ScrollAlignment alignment) {
  switch (alignment) {
    case ax::mojom::ScrollAlignment::kNone:
      return mojom::blink::ScrollAlignment::Behavior::kNoScroll;
    case ax::mojom::ScrollAlignment::kScrollAlignmentCenter:
      return mojom::blink::ScrollAlignment::Behavior::kCenter;
    case ax::mojom::ScrollAlignment::kScrollAlignmentTop:
      return mojom::blink::ScrollAlignment::Behavior::kTop;
    case ax::mojom::ScrollAlignment::kScrollAlignmentBottom:
      return mojom::blink::ScrollAlignment::Behavior::kBottom;
    case ax::mojom::ScrollAlignment::kScrollAlignmentLeft:
      return mojom::blink::ScrollAlignment::Behavior::kLeft;
    case ax::mojom::ScrollAlignment::kScrollAlignmentRight:
      return mojom::blink::ScrollAlignment::Behavior::kRight;
    case ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge:
      return mojom::blink::ScrollAlignment::Behavior::kClosestEdge;
  }
  NOTREACHED_IN_MIGRATION() << alignment;
}
}  // namespace

// A utility class which uses the lifetime of this object to signify when
// AXObjCache or AXObjectCacheImpl handles programmatic actions.
class ScopedActionAnnotator {
 public:
  ScopedActionAnnotator(AXObject* obj,
                        ax::mojom::blink::Action event_from_action)
      : cache_(&obj->AXObjectCache()) {
    std::pair<ax::mojom::blink::EventFrom, ax::mojom::blink::Action>
        event_from_data = cache_->active_event_from_data();
    DCHECK_EQ(event_from_data.first, ax::mojom::blink::EventFrom::kNone)
        << "Multiple ScopedActionAnnotator instances cannot be nested.";
    DCHECK_EQ(event_from_data.second, ax::mojom::blink::Action::kNone)
        << "event_from_action must not be set before construction.";
    cache_->set_active_event_from_data(ax::mojom::blink::EventFrom::kAction,
                                       event_from_action);
  }

  ~ScopedActionAnnotator() {
    cache_->set_active_event_from_data(ax::mojom::blink::EventFrom::kNone,
                                       ax::mojom::blink::Action::kNone);
  }

 private:
  Persistent<AXObjectCacheImpl> cache_;
};

#if DCHECK_IS_ON()
static void CheckLayoutClean(const Document* document) {
  DCHECK(document);
  LocalFrameView* view = document->View();
  DCHECK(view);
  DCHECK(!document->NeedsLayoutTreeUpdate());
  LayoutView* lview = view->GetLayoutView();

  DCHECK(!view->NeedsLayout())
      << "\n  Layout pending: " << view->LayoutPending()
      << "\n  Needs layout: " << (lview && lview->NeedsLayout());

  DCHECK_GE(document->Lifecycle().GetState(), DocumentLifecycle::kLayoutClean)
      << "Document lifecycle must be at LayoutClean or later, was "
      << document->Lifecycle().GetState();
}
#endif

void WebAXObject::Reset() {
  private_.Reset();
}

void WebAXObject::Assign(const WebAXObject& other) {
  private_ = other.private_;
}

bool WebAXObject::Equals(const WebAXObject& n) const {
  return private_.Get() == n.private_.Get();
}

bool WebAXObject::IsDetached() const {
  if (private_.IsNull())
    return true;

  return private_->IsDetached();
}

int WebAXObject::AxID() const {
  if (IsDetached())
    return -1;

  return private_->AXObjectID();
}

ax::mojom::DefaultActionVerb WebAXObject::Action() const {
  if (IsDetached())
    return ax::mojom::DefaultActionVerb::kNone;

  return private_->Action();
}

bool WebAXObject::CanSetValueAttribute() const {
  if (IsDetached())
    return false;

  return private_->CanSetValueAttribute();
}

unsigned WebAXObject::ChildCount() const {
  if (IsDetached())
    return 0;
  return private_->ChildCountIncludingIgnored();
}

WebAXObject WebAXObject::ChildAt(unsigned index) const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(
      private_->ChildAtIncludingIgnored(static_cast<int>(index)));
}

WebAXObject WebAXObject::ParentObject() const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(private_->ParentObjectIncludedInTree());
}

void WebAXObject::Serialize(ui::AXNodeData* node_data,
                            ui::AXMode accessibility_mode) const {
  if (IsDetached())
    return;

#if DCHECK_IS_ON()
  if (Node* node = private_->GetNode()) {
    Document* document = private_->GetDocument();
    DCHECK(
        !document->NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*node) ||
        DisplayLockUtilities::LockedAncestorPreventingPaint(*node))
        << "Node needs layout update and is not display locked";
  }
#endif

  ScopedFreezeAXCache freeze(private_->AXObjectCache());
  private_->Serialize(node_data, accessibility_mode);
}

void WebAXObject::AddDirtyObjectToSerializationQueue(
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    std::vector<ui::AXEventIntent> event_intents) const {
  if (IsDetached())
    return;
  private_->AXObjectCache().AddDirtyObjectToSerializationQueue(
      private_.Get(), event_from, event_from_action, event_intents);
}

void WebAXObject::OnLoadInlineTextBoxes() const {
  if (IsDetached())
    return;

  private_->LoadInlineTextBoxes();
}

BLINK_EXPORT void WebAXObject::SetImageAsDataNodeId(
    const gfx::Size& max_size) const {
  if (IsDetached())
    return;
  private_->AXObjectCache().SetImageAsDataNodeId(private_->AXObjectID(),
                                                 max_size);
}

BLINK_EXPORT int WebAXObject::ImageDataNodeId() const {
  if (IsDetached())
    return -1;
  return private_->AXObjectCache().image_data_node_id();
}

WebString WebAXObject::AutoComplete() const {
  if (IsDetached())
    return WebString();

  return private_->AutoComplete();
}

ax::mojom::AriaCurrentState WebAXObject::AriaCurrentState() const {
  if (IsDetached())
    return ax::mojom::AriaCurrentState::kNone;

  return private_->GetAriaCurrentState();
}

ax::mojom::CheckedState WebAXObject::CheckedState() const {
  if (IsDetached())
    return ax::mojom::CheckedState::kNone;

  return private_->CheckedState();
}

bool WebAXObject::IsClickable() const {
  if (IsDetached())
    return false;

  // Filter out any action = kClickAncestor.
  // Explanation: although elements are technically clickable if an ancestor is
  // clickable, we do not expose them as such unless they have a widget role,
  // otherwise there would often be an overwhelming number of clickable nodes.
  ax::mojom::blink::DefaultActionVerb action = Action();
  return action != ax::mojom::blink::DefaultActionVerb::kNone &&
         action != ax::mojom::blink::DefaultActionVerb::kClickAncestor;
}

bool WebAXObject::IsFocused() const {
  if (IsDetached())
    return false;

  return private_->IsFocused();
}

bool WebAXObject::IsModal() const {
  if (IsDetached())
    return false;

  return private_->IsModal();
}

bool WebAXObject::IsVisited() const {
  if (IsDetached())
    return false;

  return private_->IsVisited();
}

unsigned WebAXObject::ColorValue() const {
  if (IsDetached())
    return 0;

  // RGBA32 is an alias for unsigned int.
  return private_->ColorValue();
}

WebAXObject WebAXObject::AriaActiveDescendant() const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(private_->ActiveDescendant());
}

bool WebAXObject::IsEditable() const {
  if (IsDetached())
    return false;

  return private_->IsEditable();
}

bool WebAXObject::LiveRegionAtomic() const {
  if (IsDetached())
    return false;

  return private_->LiveRegionAtomic();
}

WebString WebAXObject::LiveRegionRelevant() const {
  if (IsDetached())
    return WebString();

  return private_->LiveRegionRelevant();
}

WebString WebAXObject::LiveRegionStatus() const {
  if (IsDetached())
    return WebString();

  return private_->LiveRegionStatus();
}

bool WebAXObject::AriaOwns(WebVector<WebAXObject>& owns_elements) const {
  // aria-owns rearranges the accessibility tree rather than just
  // exposing an attribute.

  // FIXME(dmazzoni): remove this function after we stop calling it
  // from Chromium.  http://crbug.com/489590

  return false;
}

bool WebAXObject::CanvasHasFallbackContent() const {
  if (IsDetached())
    return false;

  return private_->CanvasHasFallbackContent();
}

ax::mojom::InvalidState WebAXObject::InvalidState() const {
  if (IsDetached())
    return ax::mojom::InvalidState::kNone;

  return private_->GetInvalidState();
}

int WebAXObject::HeadingLevel() const {
  if (IsDetached())
    return 0;

  return private_->HeadingLevel();
}

int WebAXObject::HierarchicalLevel() const {
  if (IsDetached())
    return 0;

  return private_->HierarchicalLevel();
}

// FIXME: This method passes in a point that has page scale applied but assumes
// that (0, 0) is the top left of the visual viewport. In other words, the
// point has the VisualViewport scale applied, but not the VisualViewport
// offset. crbug.com/459591.
WebAXObject WebAXObject::HitTest(const gfx::Point& point) const {
  if (IsDetached()) {
    return WebAXObject();
  }

  ScopedFreezeAXCache freeze(private_->AXObjectCache());

  // If there's a popup document, hit test on that first.
  // TODO(kschmi) - move this logic to `AXObject` once crbug.com/459591
  // is fixed.
  Document* popup_document =
      private_->AXObjectCache().GetPopupDocumentIfShowing();
  if (popup_document && popup_document != private_->GetDocument()) {
    auto popup_root_obj = WebAXObject::FromWebDocument(popup_document);
    gfx::RectF popup_bounds;
    WebAXObject popup_container;
    gfx::Transform transform;
    popup_root_obj.GetRelativeBounds(popup_container, popup_bounds, transform);

    // The |popup_container| will never be set for a popup element. See
    // `AXObject::GetRelativeBounds`.
    DCHECK(popup_container.IsNull());

    WebAXObject hit_object = popup_root_obj.HitTest(
        point - ToRoundedVector2d(popup_bounds.OffsetFromOrigin()));

    // If the popup hit test succeeded, return that result.
    if (!hit_object.IsDetached()) {
      return hit_object;
    }
  }

  private_->GetDocument()->View()->CheckDoesNotNeedLayout();

  ScopedActionAnnotator annotater(private_.Get(),
                                  ax::mojom::blink::Action::kHitTest);
  gfx::Point contents_point =
      private_->DocumentFrameView()->SoonToBeRemovedUnscaledViewportToContents(
          point);

  if (AXObject* hit = private_->AccessibilityHitTest(contents_point)) {
    return WebAXObject(hit);
  }

  if (private_->GetBoundsInFrameCoordinates().Contains(
          PhysicalOffset(contents_point))) {
    return *this;
  }

  return WebAXObject();
}

gfx::Rect WebAXObject::GetBoundsInFrameCoordinates() const {
  PhysicalRect rect = private_->GetBoundsInFrameCoordinates();
  return ToEnclosingRect(rect);
}

WebString WebAXObject::Language() const {
  if (IsDetached())
    return WebString();

  return private_->Language();
}

bool WebAXObject::PerformAction(const ui::AXActionData& action_data) const {
  if (IsDetached())
    return false;

  Document* document = private_->GetDocument();
  if (!document)
    return false;

  document->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);

  if (IsDetached())
    return false;  // Updating lifecycle could detach object.

  ScopedActionAnnotator annotater(private_.Get(), action_data.action);
  return private_->PerformAction(action_data);
}

WebAXObject WebAXObject::InPageLinkTarget() const {
  if (IsDetached())
    return WebAXObject();
  AXObject* target = private_->InPageLinkTarget();
  if (!target)
    return WebAXObject();
  return WebAXObject(target);
}

ax::mojom::Role WebAXObject::Role() const {
  if (IsDetached())
    return ax::mojom::Role::kUnknown;

  return private_->RoleValue();
}

static ax::mojom::TextAffinity ToAXAffinity(TextAffinity affinity) {
  switch (affinity) {
    case TextAffinity::kUpstream:
      return ax::mojom::TextAffinity::kUpstream;
    case TextAffinity::kDownstream:
      return ax::mojom::TextAffinity::kDownstream;
    default:
      NOTREACHED_IN_MIGRATION();
      return ax::mojom::TextAffinity::kDownstream;
  }
}

bool WebAXObject::IsLoaded() const {
  if (IsDetached())
    return false;

  return private_->IsLoaded();
}

void WebAXObject::Selection(bool& is_selection_backward,
                            WebAXObject& anchor_object,
                            int& anchor_offset,
                            ax::mojom::TextAffinity& anchor_affinity,
                            WebAXObject& focus_object,
                            int& focus_offset,
                            ax::mojom::TextAffinity& focus_affinity) const {
  is_selection_backward = false;
  anchor_object = WebAXObject();
  anchor_offset = -1;
  anchor_affinity = ax::mojom::TextAffinity::kDownstream;
  focus_object = WebAXObject();
  focus_offset = -1;
  focus_affinity = ax::mojom::TextAffinity::kDownstream;

  if (IsDetached() || GetDocument().IsNull())
    return;

  WebAXObject focus = FromWebDocumentFocused(GetDocument());
  if (focus.IsDetached())
    return;

  const auto ax_selection =
      focus.private_->IsAtomicTextField()
          ? AXSelection::FromCurrentSelection(
                ToTextControl(*focus.private_->GetNode()))
          : AXSelection::FromCurrentSelection(*focus.private_->GetDocument());
  if (!ax_selection)
    return;

  const AXPosition ax_anchor = ax_selection.Anchor();
  anchor_object =
      WebAXObject(const_cast<AXObject*>(ax_anchor.ContainerObject()));
  const AXPosition ax_focus = ax_selection.Focus();
  focus_object = WebAXObject(const_cast<AXObject*>(ax_focus.ContainerObject()));

  is_selection_backward = ax_anchor > ax_focus;
  if (ax_anchor.IsTextPosition()) {
    anchor_offset = ax_anchor.TextOffset();
    anchor_affinity = ToAXAffinity(ax_anchor.Affinity());
  } else {
    anchor_offset = ax_anchor.ChildIndex();
  }

  if (ax_focus.IsTextPosition()) {
    focus_offset = ax_focus.TextOffset();
    focus_affinity = ToAXAffinity(ax_focus.Affinity());
  } else {
    focus_offset = ax_focus.ChildIndex();
  }
}

bool WebAXObject::SetSelection(const WebAXObject& anchor_object,
                               int anchor_offset,
                               const WebAXObject& focus_object,
                               int focus_offset) const {
  if (IsDetached() || anchor_object.IsDetached() || focus_object.IsDetached()) {
    return false;
  }

  ScopedActionAnnotator annotater(private_.Get(),
                                  ax::mojom::blink::Action::kSetSelection);
  AXPosition ax_anchor, ax_focus;
  if (static_cast<const AXObject*>(anchor_object)->IsTextObject() ||
      static_cast<const AXObject*>(anchor_object)->IsAtomicTextField()) {
    ax_anchor =
        AXPosition::CreatePositionInTextObject(*anchor_object, anchor_offset);
  } else if (anchor_offset <= 0) {
    ax_anchor = AXPosition::CreateFirstPositionInObject(*anchor_object);
  } else if (anchor_offset >= static_cast<int>(anchor_object.ChildCount())) {
    ax_anchor = AXPosition::CreateLastPositionInObject(*anchor_object);
  } else {
    DCHECK_GE(anchor_offset, 0);
    ax_anchor = AXPosition::CreatePositionBeforeObject(
        *anchor_object.ChildAt(static_cast<unsigned int>(anchor_offset)));
  }

  if (static_cast<const AXObject*>(focus_object)->IsTextObject() ||
      static_cast<const AXObject*>(focus_object)->IsAtomicTextField()) {
    ax_focus =
        AXPosition::CreatePositionInTextObject(*focus_object, focus_offset);
  } else if (focus_offset <= 0) {
    ax_focus = AXPosition::CreateFirstPositionInObject(*focus_object);
  } else if (focus_offset >= static_cast<int>(focus_object.ChildCount())) {
    ax_focus = AXPosition::CreateLastPositionInObject(*focus_object);
  } else {
    DCHECK_GE(focus_offset, 0);
    ax_focus = AXPosition::CreatePositionBeforeObject(
        *focus_object.ChildAt(static_cast<unsigned int>(focus_offset)));
  }

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(ax_anchor).SetFocus(ax_focus).Build();
  return ax_selection.Select();
}

WebString WebAXObject::GetValueForControl() const {
  if (IsDetached())
    return WebString();

  // TODO(nektar): Switch to `GetValueForControl()` once browser changes have
  // landed.
  return private_->SlowGetValueForControlIncludingContentEditable();
}

ax::mojom::blink::WritingDirection WebAXObject::GetTextDirection() const {
  if (IsDetached())
    return ax::mojom::blink::WritingDirection::kLtr;

  return private_->GetTextDirection();
}

WebURL WebAXObject::Url() const {
  if (IsDetached())
    return WebURL();

  return private_->Url();
}

WebString WebAXObject::GetName(ax::mojom::NameFrom& out_name_from,
                               WebVector<WebAXObject>& out_name_objects) const {
  out_name_from = ax::mojom::blink::NameFrom::kNone;

  if (IsDetached())
    return WebString();

  ScopedFreezeAXCache freeze(private_->AXObjectCache());

  HeapVector<Member<AXObject>> name_objects;
  WebString result = private_->GetName(out_name_from, &name_objects);

  out_name_objects.reserve(name_objects.size());
  out_name_objects.resize(name_objects.size());
  base::ranges::copy(name_objects, out_name_objects.begin());

  return result;
}

WebString WebAXObject::GetName() const {
  if (IsDetached())
    return WebString();

  ScopedFreezeAXCache freeze(private_->AXObjectCache());

  ax::mojom::NameFrom name_from;
  HeapVector<Member<AXObject>> name_objects;
  return private_->GetName(name_from, &name_objects);
}

WebString WebAXObject::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& out_description_from,
    WebVector<WebAXObject>& out_description_objects) const {
  out_description_from = ax::mojom::blink::DescriptionFrom::kNone;

  if (IsDetached())
    return WebString();

  HeapVector<Member<AXObject>> description_objects;
  String result = private_->Description(name_from, out_description_from,
                                        &description_objects);

  out_description_objects.reserve(description_objects.size());
  out_description_objects.resize(description_objects.size());
  base::ranges::copy(description_objects, out_description_objects.begin());

  return result;
}

WebString WebAXObject::Placeholder(ax::mojom::NameFrom name_from) const {
  if (IsDetached())
    return WebString();

  return private_->Placeholder(name_from);
}

bool WebAXObject::SupportsRangeValue() const {
  if (IsDetached())
    return false;

  return private_->IsRangeValueSupported();
}

bool WebAXObject::ValueForRange(float* out_value) const {
  if (IsDetached())
    return false;

  return private_->ValueForRange(out_value);
}

bool WebAXObject::MaxValueForRange(float* out_value) const {
  if (IsDetached())
    return false;

  return private_->MaxValueForRange(out_value);
}

bool WebAXObject::MinValueForRange(float* out_value) const {
  if (IsDetached())
    return false;

  return private_->MinValueForRange(out_value);
}

bool WebAXObject::StepValueForRange(float* out_value) const {
  if (IsDetached())
    return false;

  return private_->StepValueForRange(out_value);
}

WebNode WebAXObject::GetNode() const {
  if (IsDetached())
    return WebNode();

  Node* node = private_->GetNode();
  if (!node)
    return WebNode();

  return WebNode(node);
}

WebDocument WebAXObject::GetDocument() const {
  if (IsDetached())
    return WebDocument();

  Document* document = private_->GetDocument();
  if (!document)
    return WebDocument();

  return WebDocument(document);
}

bool WebAXObject::IsIgnored() const {
  if (IsDetached())
    return false;

  return private_->IsIgnored();
}

bool WebAXObject::IsIncludedInTree() const {
  if (IsDetached())
    return false;

  DCHECK(private_->GetDocument());
  DCHECK_GE(private_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean)
      << "Document lifecycle must be at LayoutClean or later, was "
      << private_->GetDocument()->Lifecycle().GetState();

  return private_->IsIncludedInTree();
}

unsigned WebAXObject::ColumnCount() const {
  if (IsDetached())
    return false;

  return private_->IsTableLikeRole() ? private_->ColumnCount() : 0;
}

unsigned WebAXObject::RowCount() const {
  if (IsDetached())
    return 0;

  if (!private_->IsTableLikeRole())
    return 0;

  return private_->RowCount();
}

WebAXObject WebAXObject::CellForColumnAndRow(unsigned column,
                                             unsigned row) const {
  if (IsDetached())
    return WebAXObject();

  if (!private_->IsTableLikeRole())
    return WebAXObject();

  return WebAXObject(private_->CellForColumnAndRow(column, row));
}

void WebAXObject::RowHeaders(
    WebVector<WebAXObject>& row_header_elements) const {
  if (IsDetached())
    return;

  if (!private_->IsTableLikeRole())
    return;

  AXObject::AXObjectVector headers;
  private_->RowHeaders(headers);
  row_header_elements.reserve(headers.size());
  row_header_elements.resize(headers.size());
  base::ranges::copy(headers, row_header_elements.begin());
}

void WebAXObject::ColumnHeaders(
    WebVector<WebAXObject>& column_header_elements) const {
  if (IsDetached())
    return;

  if (!private_->IsTableLikeRole())
    return;

  AXObject::AXObjectVector headers;
  private_->ColumnHeaders(headers);
  column_header_elements.reserve(headers.size());
  column_header_elements.resize(headers.size());
  base::ranges::copy(headers, column_header_elements.begin());
}

unsigned WebAXObject::CellColumnIndex() const {
  if (IsDetached())
    return 0;

  return private_->IsTableCellLikeRole() ? private_->ColumnIndex() : 0;
}

unsigned WebAXObject::CellColumnSpan() const {
  if (IsDetached())
    return 0;

  return private_->IsTableCellLikeRole() ? private_->ColumnSpan() : 0;
}

unsigned WebAXObject::CellRowIndex() const {
  if (IsDetached())
    return 0;

  return private_->IsTableCellLikeRole() ? private_->RowIndex() : 0;
}

unsigned WebAXObject::CellRowSpan() const {
  if (IsDetached())
    return 0;

  return private_->IsTableCellLikeRole() ? private_->RowSpan() : 0;
}

ax::mojom::SortDirection WebAXObject::SortDirection() const {
  if (IsDetached())
    return ax::mojom::SortDirection::kNone;

  return private_->GetSortDirection();
}

WebAXObject WebAXObject::NextOnLine() const {
  if (IsDetached())
    return WebAXObject();

  ScopedFreezeAXCache freeze(private_->AXObjectCache());
  // Force computation of next/previous on line data, since this API may call
  // serializations outside of the regular flow. AXObjectCacheImpl may not had
  // the chance to compute next|previous on line data. Clear the cache and force
  // the computation.
  private_->AXObjectCache().ClearCachedNodesOnLine();
  private_->AXObjectCache().ComputeNodesOnLine(private_->GetLayoutObject());
  return WebAXObject(private_.Get()->NextOnLine());
}

WebAXObject WebAXObject::PreviousOnLine() const {
  if (IsDetached())
    return WebAXObject();

  ScopedFreezeAXCache freeze(private_->AXObjectCache());
  // Force computation of next/previous on line data, since this API may call
  // serializations outside of the regular flow. AXObjectCacheImpl may not had
  // the chance to compute next|previous on line data. Clear the cache and force
  // the computation.
  private_->AXObjectCache().ClearCachedNodesOnLine();
  private_->AXObjectCache().ComputeNodesOnLine(private_->GetLayoutObject());
  return WebAXObject(private_.Get()->PreviousOnLine());
}

void WebAXObject::CharacterOffsets(WebVector<int>& offsets) const {
  if (IsDetached())
    return;

  Vector<int> offsets_vector;
  private_->TextCharacterOffsets(offsets_vector);
  offsets = offsets_vector;
}

void WebAXObject::GetWordBoundaries(WebVector<int>& starts,
                                    WebVector<int>& ends) const {
  if (IsDetached())
    return;

  Vector<int> src_starts;
  Vector<int> src_ends;
  private_->GetWordBoundaries(src_starts, src_ends);
  DCHECK_EQ(src_starts.size(), src_ends.size());

  WebVector<int> word_start_offsets(src_starts.size());
  WebVector<int> word_end_offsets(src_ends.size());
  for (wtf_size_t i = 0; i < src_starts.size(); ++i) {
    word_start_offsets[i] = src_starts[i];
    word_end_offsets[i] = src_ends[i];
  }

  starts.swap(word_start_offsets);
  ends.swap(word_end_offsets);
}

gfx::Point WebAXObject::GetScrollOffset() const {
  if (IsDetached())
    return gfx::Point();

  return private_->GetScrollOffset();
}

gfx::Point WebAXObject::MinimumScrollOffset() const {
  if (IsDetached())
    return gfx::Point();

  return private_->MinimumScrollOffset();
}

gfx::Point WebAXObject::MaximumScrollOffset() const {
  if (IsDetached())
    return gfx::Point();

  return private_->MaximumScrollOffset();
}

void WebAXObject::SetScrollOffset(const gfx::Point& offset) const {
  if (IsDetached())
    return;

  private_->SetScrollOffset(offset);
}

void WebAXObject::GetRelativeBounds(WebAXObject& offset_container,
                                    gfx::RectF& bounds_in_container,
                                    gfx::Transform& container_transform,
                                    bool* clips_children) const {
  if (IsDetached())
    return;

#if DCHECK_IS_ON()
  CheckLayoutClean(private_->GetDocument());
#endif

  AXObject* container = nullptr;
  gfx::RectF bounds;
  private_->GetRelativeBounds(&container, bounds, container_transform,
                              clips_children);
  offset_container = WebAXObject(container);
  bounds_in_container = bounds;
}

bool WebAXObject::ScrollToMakeVisible() const {
  if (IsDetached())
    return false;

  ScopedActionAnnotator annotater(
      private_.Get(), ax::mojom::blink::Action::kScrollToMakeVisible);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::blink::Action::kScrollToMakeVisible;
  return private_->PerformAction(action_data);
}

bool WebAXObject::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& subfocus,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) const {
  if (IsDetached())
    return false;

  ScopedActionAnnotator annotater(
      private_.Get(), ax::mojom::blink::Action::kScrollToMakeVisible);
  auto horizontal_behavior =
      ToBlinkScrollAlignmentBehavior(horizontal_scroll_alignment);
  auto vertical_behavior =
      ToBlinkScrollAlignmentBehavior(vertical_scroll_alignment);

  mojom::blink::ScrollAlignment::Behavior visible_horizontal_behavior =
      scroll_behavior == ax::mojom::ScrollBehavior::kScrollIfVisible
          ? horizontal_behavior
          : mojom::blink::ScrollAlignment::Behavior::kNoScroll;
  mojom::blink::ScrollAlignment::Behavior visible_vertical_behavior =
      scroll_behavior == ax::mojom::ScrollBehavior::kScrollIfVisible
          ? vertical_behavior
          : mojom::blink::ScrollAlignment::Behavior::kNoScroll;

  blink::mojom::blink::ScrollAlignment blink_horizontal_scroll_alignment = {
      visible_horizontal_behavior, horizontal_behavior, horizontal_behavior};
  blink::mojom::blink::ScrollAlignment blink_vertical_scroll_alignment = {
      visible_vertical_behavior, vertical_behavior, vertical_behavior};

  return private_->RequestScrollToMakeVisibleWithSubFocusAction(
      subfocus, blink_horizontal_scroll_alignment,
      blink_vertical_scroll_alignment);
}

void WebAXObject::HandleAutofillSuggestionAvailabilityChanged(
    blink::WebAXAutofillSuggestionAvailability suggestion_availability) const {
  if (IsDetached() || !private_->GetLayoutObject()) {
    return;
  }

  private_->HandleAutofillSuggestionAvailabilityChanged(
      suggestion_availability);
}

int WebAXObject::GenerateAXID() {
  DCHECK(private_->GetDocument() && private_->GetDocument()->IsActive());
  return private_->AXObjectCache().GenerateAXID();
}

void WebAXObject::SetPluginTreeSource(
    ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>*
        source) {
  private_->AXObjectCache().SetPluginTreeSource(source);
}

void WebAXObject::MarkPluginDescendantDirty(ui::AXNodeID node_id) {
  private_->AXObjectCache().MarkPluginDescendantDirty(node_id);
}

WebString WebAXObject::ToString(bool verbose) const {
  if (private_.IsNull())
    return WebString("[Null]");

  return private_->ToString(verbose);
}

WebAXObject::WebAXObject(AXObject* object) : private_(object) {}

WebAXObject& WebAXObject::operator=(AXObject* object) {
  private_ = object;
  return *this;
}

bool WebAXObject::operator==(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ == *other.private_;
}

bool WebAXObject::operator!=(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ != *other.private_;
}

bool WebAXObject::operator<(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ < *other.private_;
}

bool WebAXObject::operator<=(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ <= *other.private_;
}

bool WebAXObject::operator>(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ > *other.private_;
}

bool WebAXObject::operator>=(const WebAXObject& other) const {
  if (IsDetached() || other.IsDetached())
    return false;
  return *private_ >= *other.private_;
}

WebAXObject::operator AXObject*() const {
  return private_.Get();
}

// static
WebAXObject WebAXObject::FromWebNode(const WebNode& web_node) {
  WebDocument web_document = web_node.GetDocument();
  const Document* document = web_document.ConstUnwrap<Document>();
  auto* cache = To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  const Node* node = web_node.ConstUnwrap<Node>();

  if (!cache) {
    return WebAXObject();
  }

  // TODO: if this shouldn't be done by default, add a parameter passed by the
  // caller.

  // Since calls into this lookup might happen prior to the cache building
  // everything from its backing objects like DOM, layout trees, force it here.
  cache->UpdateAXForAllDocuments();
  return WebAXObject(cache->Get(node));
}

// static
WebAXObject WebAXObject::FromWebDocument(const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();
  auto* cache = To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  DCHECK(cache);
  if (!cache->Root())
    return WebAXObject();  // Accessibility not yet active in this cache.
  return WebAXObject(cache->Get(document));
}

// static
WebAXObject WebAXObject::FromWebDocumentByID(const WebDocument& web_document,
                                             int ax_id) {
  const Document* document = web_document.ConstUnwrap<Document>();
  auto* cache = To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  return cache ? WebAXObject(cache->ObjectFromAXID(ax_id)) : WebAXObject();
}

// static
WebAXObject WebAXObject::FromWebDocumentFirstWithRole(
    const WebDocument& web_document,
    ax::mojom::blink::Role role) {
  const Document* document = web_document.ConstUnwrap<Document>();
  auto* cache = To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  return cache ? WebAXObject(cache->FirstObjectWithRole(role)) : WebAXObject();
}

// static
WebAXObject WebAXObject::FromWebDocumentFocused(
    const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();
#if DCHECK_IS_ON()
  CheckLayoutClean(document);
#endif
  auto* cache = To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  cache->UpdateAXForAllDocuments();
  return cache ? WebAXObject(cache->FocusedObject()) : WebAXObject();
}

// static
bool WebAXObject::IsDirty(const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();
  if (!document || !document->View())
    return false;
  if (!document->ExistingAXObjectCache())
    return false;

  return document->ExistingAXObjectCache()->IsDirty();
}

}  // namespace blink
