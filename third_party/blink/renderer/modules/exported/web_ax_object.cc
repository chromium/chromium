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

#include "SkMatrix44.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/keyboard_event_manager.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class WebAXSparseAttributeClientAdapter : public AXSparseAttributeClient {
 public:
  WebAXSparseAttributeClientAdapter(WebAXSparseAttributeClient& attribute_map)
      : attribute_map_(attribute_map) {}
  virtual ~WebAXSparseAttributeClientAdapter() = default;

 private:
  WebAXSparseAttributeClient& attribute_map_;

  void AddBoolAttribute(AXBoolAttribute attribute, bool value) override {
    attribute_map_.AddBoolAttribute(static_cast<WebAXBoolAttribute>(attribute),
                                    value);
  }

  void AddStringAttribute(AXStringAttribute attribute,
                          const String& value) override {
    attribute_map_.AddStringAttribute(
        static_cast<WebAXStringAttribute>(attribute), value);
  }

  void AddObjectAttribute(AXObjectAttribute attribute,
                          AXObject& value) override {
    attribute_map_.AddObjectAttribute(
        static_cast<WebAXObjectAttribute>(attribute), WebAXObject(&value));
  }

  void AddObjectVectorAttribute(AXObjectVectorAttribute attribute,
                                HeapVector<Member<AXObject>>& value) override {
    WebVector<WebAXObject> result(value.size());
    std::copy(value.begin(), value.end(), result.begin());
    attribute_map_.AddObjectVectorAttribute(
        static_cast<WebAXObjectVectorAttribute>(attribute), result);
  }
};

#if DCHECK_IS_ON()
// It's not safe to call some WebAXObject APIs if a layout is pending.
// Clients should call updateLayoutAndCheckValidity first.
static bool IsLayoutClean(Document* document) {
  if (!document || !document->View())
    return false;
  return document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean ||
         ((document->Lifecycle().GetState() == DocumentLifecycle::kStyleClean ||
           document->Lifecycle().GetState() ==
               DocumentLifecycle::kLayoutSubtreeChangeClean) &&
          !document->View()->NeedsLayout());
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

int WebAXObject::GenerateAXID() const {
  if (IsDetached())
    return -1;

  return private_->AXObjectCache().GenerateAXID();
}

bool WebAXObject::UpdateLayoutAndCheckValidity() {
  if (!IsDetached()) {
    Document* document = private_->GetDocument();
    if (!document || !document->View() ||
        !document->View()->UpdateLifecycleToCompositingCleanPlusScrolling())
      return false;
  }

  // Doing a layout can cause this object to be invalid, so check again.
  return !IsDetached();
}

ax::mojom::DefaultActionVerb WebAXObject::Action() const {
  if (IsDetached())
    return ax::mojom::DefaultActionVerb::kNone;

  return private_->Action();
}

bool WebAXObject::CanPress() const {
  if (IsDetached())
    return false;

  return private_->ActionElement() || private_->IsButton() ||
         private_->IsMenuRelated();
}

bool WebAXObject::CanSetFocusAttribute() const {
  if (IsDetached())
    return false;

  return private_->CanSetFocusAttribute();
}

bool WebAXObject::CanSetValueAttribute() const {
  if (IsDetached())
    return false;

  return private_->CanSetValueAttribute();
}

unsigned WebAXObject::ChildCount() const {
  if (IsDetached())
    return 0;

  return private_->Children().size();
}

WebAXObject WebAXObject::ChildAt(unsigned index) const {
  if (IsDetached())
    return WebAXObject();

  if (private_->Children().size() <= index)
    return WebAXObject();

  return WebAXObject(private_->Children()[index]);
}

WebAXObject WebAXObject::ParentObject() const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(private_->ParentObject());
}

void WebAXObject::GetSparseAXAttributes(
    WebAXSparseAttributeClient& client) const {
  if (IsDetached())
    return;

  WebAXSparseAttributeClientAdapter adapter(client);
  private_->GetSparseAXAttributes(adapter);
}

bool WebAXObject::IsAnchor() const {
  if (IsDetached())
    return false;

  return private_->IsAnchor();
}

bool WebAXObject::IsAutofillAvailable() const {
  if (IsDetached())
    return false;

  return private_->IsAutofillAvailable();
}

WebString WebAXObject::AriaAutoComplete() const {
  if (IsDetached())
    return WebString();

  return private_->AriaAutoComplete();
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

  return private_->IsClickable();
}

bool WebAXObject::IsControl() const {
  if (IsDetached())
    return false;

  return private_->IsControl();
}

WebAXRestriction WebAXObject::Restriction() const {
  if (IsDetached())
    return kWebAXRestrictionNone;

  return static_cast<WebAXRestriction>(private_->Restriction());
}

WebAXExpanded WebAXObject::IsExpanded() const {
  if (IsDetached())
    return kWebAXExpandedUndefined;

  return static_cast<WebAXExpanded>(private_->IsExpanded());
}

bool WebAXObject::IsFocused() const {
  if (IsDetached())
    return false;

  return private_->IsFocused();
}

bool WebAXObject::IsHovered() const {
  if (IsDetached())
    return false;

  return private_->IsHovered();
}

bool WebAXObject::IsLinked() const {
  if (IsDetached())
    return false;

  return private_->IsLinked();
}

bool WebAXObject::IsLoaded() const {
  if (IsDetached())
    return false;

  return private_->IsLoaded();
}

bool WebAXObject::IsModal() const {
  if (IsDetached())
    return false;

  return private_->IsModal();
}

bool WebAXObject::IsMultiSelectable() const {
  if (IsDetached())
    return false;

  return private_->IsMultiSelectable();
}

bool WebAXObject::IsOffScreen() const {
  if (IsDetached())
    return false;

  return private_->IsOffScreen();
}

bool WebAXObject::IsPasswordField() const {
  if (IsDetached())
    return false;

  return private_->IsPasswordField();
}

bool WebAXObject::IsRequired() const {
  if (IsDetached())
    return false;

  return private_->IsRequired();
}

WebAXSelectedState WebAXObject::IsSelected() const {
  if (IsDetached())
    return kWebAXSelectedStateUndefined;

  return static_cast<WebAXSelectedState>(private_->IsSelected());
}

bool WebAXObject::IsSelectedOptionActive() const {
  if (IsDetached())
    return false;

  return private_->IsSelectedOptionActive();
}

bool WebAXObject::IsVisible() const {
  if (IsDetached())
    return false;

  return private_->IsVisible();
}

bool WebAXObject::IsVisited() const {
  if (IsDetached())
    return false;

  return private_->IsVisited();
}

WebString WebAXObject::AccessKey() const {
  if (IsDetached())
    return WebString();

  return WebString(private_->AccessKey());
}

unsigned WebAXObject::BackgroundColor() const {
  if (IsDetached())
    return 0;

  // RGBA32 is an alias for unsigned int.
  return private_->BackgroundColor();
}

unsigned WebAXObject::GetColor() const {
  if (IsDetached())
    return 0;

  // RGBA32 is an alias for unsigned int.
  return private_->GetColor();
}

// Deprecated.
void WebAXObject::ColorValue(int& r, int& g, int& b) const {
  if (IsDetached())
    return;

  unsigned color = private_->ColorValue();
  r = (color >> 16) & 0xFF;
  g = (color >> 8) & 0xFF;
  b = color & 0xFF;
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

ax::mojom::HasPopup WebAXObject::HasPopup() const {
  if (IsDetached())
    return ax::mojom::HasPopup::kFalse;

  return private_->HasPopup();
}

bool WebAXObject::IsEditableRoot() const {
  if (IsDetached())
    return false;

  return private_->IsEditableRoot();
}

bool WebAXObject::IsEditable() const {
  if (IsDetached())
    return false;

  return private_->IsEditable();
}

bool WebAXObject::IsMultiline() const {
  if (IsDetached())
    return false;

  return private_->IsMultiline();
}

bool WebAXObject::IsRichlyEditable() const {
  if (IsDetached())
    return false;

  return private_->IsRichlyEditable();
}

int WebAXObject::PosInSet() const {
  if (IsDetached())
    return 0;

  return private_->PosInSet();
}

int WebAXObject::SetSize() const {
  if (IsDetached())
    return 0;

  return private_->SetSize();
}

bool WebAXObject::IsInLiveRegion() const {
  if (IsDetached())
    return false;

  return !!private_->LiveRegionRoot();
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

WebAXObject WebAXObject::LiveRegionRoot() const {
  if (IsDetached())
    return WebAXObject();

  AXObject* live_region_root = private_->LiveRegionRoot();
  if (live_region_root)
    return WebAXObject(live_region_root);
  return WebAXObject();
}

bool WebAXObject::ContainerLiveRegionAtomic() const {
  if (IsDetached())
    return false;

  return private_->ContainerLiveRegionAtomic();
}

bool WebAXObject::ContainerLiveRegionBusy() const {
  if (IsDetached())
    return false;

  return private_->ContainerLiveRegionBusy();
}

WebString WebAXObject::ContainerLiveRegionRelevant() const {
  if (IsDetached())
    return WebString();

  return private_->ContainerLiveRegionRelevant();
}

WebString WebAXObject::ContainerLiveRegionStatus() const {
  if (IsDetached())
    return WebString();

  return private_->ContainerLiveRegionStatus();
}

bool WebAXObject::AriaOwns(WebVector<WebAXObject>& owns_elements) const {
  // aria-owns rearranges the accessibility tree rather than just
  // exposing an attribute.

  // FIXME(dmazzoni): remove this function after we stop calling it
  // from Chromium.  http://crbug.com/489590

  return false;
}

WebString WebAXObject::FontFamily() const {
  if (IsDetached())
    return WebString();

  return private_->FontFamily();
}

float WebAXObject::FontSize() const {
  if (IsDetached())
    return 0.0f;

  return private_->FontSize();
}

bool WebAXObject::CanvasHasFallbackContent() const {
  if (IsDetached())
    return false;

  return private_->CanvasHasFallbackContent();
}

WebString WebAXObject::ImageDataUrl(const WebSize& max_size) const {
  if (IsDetached())
    return WebString();

  return private_->ImageDataUrl(max_size);
}

ax::mojom::InvalidState WebAXObject::InvalidState() const {
  if (IsDetached())
    return ax::mojom::InvalidState::kNone;

  return private_->GetInvalidState();
}

// Only used when invalidState() returns WebAXInvalidStateOther.
WebString WebAXObject::AriaInvalidValue() const {
  if (IsDetached())
    return WebString();

  return private_->AriaInvalidValue();
}

double WebAXObject::EstimatedLoadingProgress() const {
  if (IsDetached())
    return 0.0;

  return private_->EstimatedLoadingProgress();
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
WebAXObject WebAXObject::HitTest(const WebPoint& point) const {
  if (IsDetached())
    return WebAXObject();

  IntPoint contents_point =
      private_->DocumentFrameView()->SoonToBeRemovedUnscaledViewportToContents(
          point);
  AXObject* hit = private_->AccessibilityHitTest(contents_point);

  if (hit)
    return WebAXObject(hit);

  if (private_->GetBoundsInFrameCoordinates().Contains(contents_point))
    return *this;

  return WebAXObject();
}

WebString WebAXObject::KeyboardShortcut() const {
  if (IsDetached())
    return WebString();

  String access_key = private_->AccessKey();
  if (access_key.IsNull())
    return WebString();

  DEFINE_STATIC_LOCAL(String, modifier_string, ());
  if (modifier_string.IsNull()) {
    unsigned modifiers = KeyboardEventManager::kAccessKeyModifiers;
    // Follow the same order as Mozilla MSAA implementation:
    // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
    // should not be localized and defines the separator as "+".
    StringBuilder modifier_string_builder;
    if (modifiers & WebInputEvent::kControlKey)
      modifier_string_builder.Append("Ctrl+");
    if (modifiers & WebInputEvent::kAltKey)
      modifier_string_builder.Append("Alt+");
    if (modifiers & WebInputEvent::kShiftKey)
      modifier_string_builder.Append("Shift+");
    if (modifiers & WebInputEvent::kMetaKey)
      modifier_string_builder.Append("Win+");
    modifier_string = modifier_string_builder.ToString();
  }

  return String(modifier_string + access_key);
}

WebString WebAXObject::Language() const {
  if (IsDetached())
    return WebString();

  return private_->Language();
}

bool WebAXObject::ClearAccessibilityFocus() const {
  if (IsDetached())
    return false;

  return private_->InternalClearAccessibilityFocusAction();
}

bool WebAXObject::Click() const {
  if (IsDetached())
    return false;

  return private_->RequestClickAction();
}

bool WebAXObject::Increment() const {
  if (IsDetached())
    return false;

  return private_->RequestIncrementAction();
}

bool WebAXObject::Decrement() const {
  if (IsDetached())
    return false;

  return private_->RequestDecrementAction();
}

WebAXObject WebAXObject::InPageLinkTarget() const {
  if (IsDetached())
    return WebAXObject();
  AXObject* target = private_->InPageLinkTarget();
  if (!target)
    return WebAXObject();
  return WebAXObject(target);
}

WebAXOrientation WebAXObject::Orientation() const {
  if (IsDetached())
    return kWebAXOrientationUndefined;

  return static_cast<WebAXOrientation>(private_->Orientation());
}

WebVector<WebAXObject> WebAXObject::RadioButtonsInGroup() const {
  if (IsDetached())
    return WebVector<WebAXObject>();

  AXObject::AXObjectVector radio_buttons = private_->RadioButtonsInGroup();
  WebVector<WebAXObject> web_radio_buttons(radio_buttons.size());
  std::copy(radio_buttons.begin(), radio_buttons.end(),
            web_radio_buttons.begin());
  return web_radio_buttons;
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
      NOTREACHED();
      return ax::mojom::TextAffinity::kDownstream;
  }
}

void WebAXObject::Selection(WebAXObject& anchor_object,
                            int& anchor_offset,
                            ax::mojom::TextAffinity& anchor_affinity,
                            WebAXObject& focus_object,
                            int& focus_offset,
                            ax::mojom::TextAffinity& focus_affinity) const {
  if (IsDetached()) {
    anchor_object = WebAXObject();
    anchor_offset = -1;
    anchor_affinity = ax::mojom::TextAffinity::kDownstream;
    focus_object = WebAXObject();
    focus_offset = -1;
    focus_affinity = ax::mojom::TextAffinity::kDownstream;
    return;
  }

  AXObject::AXSelection ax_selection = private_->Selection();
  anchor_object = WebAXObject(ax_selection.anchor_object);
  anchor_offset = ax_selection.anchor_offset;
  anchor_affinity = ToAXAffinity(ax_selection.anchor_affinity);
  focus_object = WebAXObject(ax_selection.focus_object);
  focus_offset = ax_selection.focus_offset;
  focus_affinity = ToAXAffinity(ax_selection.focus_affinity);
  return;
}

bool WebAXObject::SetAccessibilityFocus() const {
  if (IsDetached())
    return false;

  return private_->InternalSetAccessibilityFocusAction();
}

bool WebAXObject::SetSelected(bool selected) const {
  if (IsDetached())
    return false;

  return private_->RequestSetSelectedAction(selected);
}

bool WebAXObject::SetSelection(const WebAXObject& anchor_object,
                               int anchor_offset,
                               const WebAXObject& focus_object,
                               int focus_offset) const {
  if (IsDetached())
    return false;

  AXObject::AXSelection ax_selection(anchor_object, anchor_offset,
                                     TextAffinity::kUpstream, focus_object,
                                     focus_offset, TextAffinity::kDownstream);
  return private_->RequestSetSelectionAction(ax_selection);
}

unsigned WebAXObject::SelectionEnd() const {
  if (IsDetached())
    return 0;

  AXObject::AXSelection ax_selection = private_->SelectionUnderObject();
  if (ax_selection.focus_offset < 0)
    return 0;

  return ax_selection.focus_offset;
}

unsigned WebAXObject::SelectionStart() const {
  if (IsDetached())
    return 0;

  AXObject::AXSelection ax_selection = private_->SelectionUnderObject();
  if (ax_selection.anchor_offset < 0)
    return 0;

  return ax_selection.anchor_offset;
}

unsigned WebAXObject::SelectionEndLineNumber() const {
  if (IsDetached())
    return 0;

  VisiblePosition position = private_->VisiblePositionForIndex(SelectionEnd());
  int line_number = private_->LineForPosition(position);
  if (line_number < 0)
    return 0;

  return line_number;
}

unsigned WebAXObject::SelectionStartLineNumber() const {
  if (IsDetached())
    return 0;

  VisiblePosition position =
      private_->VisiblePositionForIndex(SelectionStart());
  int line_number = private_->LineForPosition(position);
  if (line_number < 0)
    return 0;

  return line_number;
}

bool WebAXObject::Focus() const {
  if (IsDetached())
    return false;

  return private_->RequestFocusAction();
}

bool WebAXObject::SetSequentialFocusNavigationStartingPoint() const {
  if (IsDetached())
    return false;

  return private_->RequestSetSequentialFocusNavigationStartingPointAction();
}

bool WebAXObject::SetValue(WebString value) const {
  if (IsDetached())
    return false;

  return private_->RequestSetValueAction(value);
}

bool WebAXObject::ShowContextMenu() const {
  if (IsDetached())
    return false;

  return private_->RequestShowContextMenuAction();
}

WebString WebAXObject::StringValue() const {
  if (IsDetached())
    return WebString();

  return private_->StringValue();
}

ax::mojom::TextDirection WebAXObject::GetTextDirection() const {
  if (IsDetached())
    return ax::mojom::TextDirection::kLtr;

  return private_->GetTextDirection();
}

ax::mojom::TextPosition WebAXObject::GetTextPosition() const {
  if (IsDetached())
    return ax::mojom::TextPosition::kNone;

  return private_->GetTextPosition();
}

WebAXTextStyle WebAXObject::TextStyle() const {
  if (IsDetached())
    return kWebAXTextStyleNone;

  return static_cast<WebAXTextStyle>(private_->GetTextStyle());
}

WebURL WebAXObject::Url() const {
  if (IsDetached())
    return WebURL();

  return private_->Url();
}

WebString WebAXObject::GetName(ax::mojom::NameFrom& out_name_from,
                               WebVector<WebAXObject>& out_name_objects) const {
  if (IsDetached())
    return WebString();

  ax::mojom::NameFrom name_from = ax::mojom::NameFrom::kUninitialized;
  HeapVector<Member<AXObject>> name_objects;
  WebString result = private_->GetName(name_from, &name_objects);
  out_name_from = name_from;

  out_name_objects.reserve(name_objects.size());
  out_name_objects.resize(name_objects.size());
  std::copy(name_objects.begin(), name_objects.end(), out_name_objects.begin());

  return result;
}

WebString WebAXObject::GetName() const {
  if (IsDetached())
    return WebString();

  ax::mojom::NameFrom name_from;
  HeapVector<Member<AXObject>> name_objects;
  return private_->GetName(name_from, &name_objects);
}

WebString WebAXObject::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& out_description_from,
    WebVector<WebAXObject>& out_description_objects) const {
  if (IsDetached())
    return WebString();

  ax::mojom::DescriptionFrom description_from =
      ax::mojom::DescriptionFrom::kUninitialized;
  HeapVector<Member<AXObject>> description_objects;
  String result =
      private_->Description(name_from, description_from, &description_objects);
  out_description_from = description_from;

  out_description_objects.reserve(description_objects.size());
  out_description_objects.resize(description_objects.size());
  std::copy(description_objects.begin(), description_objects.end(),
            out_description_objects.begin());

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

  return private_->SupportsRangeValue();
}

WebString WebAXObject::ValueDescription() const {
  if (IsDetached())
    return WebString();

  return private_->ValueDescription();
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

bool WebAXObject::HasComputedStyle() const {
  if (IsDetached())
    return false;

  Document* document = private_->GetDocument();
  if (document)
    document->UpdateStyleAndLayoutTree();

  Node* node = private_->GetNode();
  if (!node)
    return false;

  return node->EnsureComputedStyle();
}

WebString WebAXObject::ComputedStyleDisplay() const {
  if (IsDetached())
    return WebString();

  Document* document = private_->GetDocument();
  if (document)
    document->UpdateStyleAndLayoutTree();

  Node* node = private_->GetNode();
  if (!node)
    return WebString();

  const ComputedStyle* computed_style = node->EnsureComputedStyle();
  if (!computed_style)
    return WebString();

  return WebString(CSSProperty::Get(CSSPropertyDisplay)
                       .CSSValueFromComputedStyle(
                           *computed_style, /* layout_object */ nullptr, node,
                           /* allow_visited_style */ false)
                       ->CssText());
}

bool WebAXObject::AccessibilityIsIgnored() const {
  if (IsDetached())
    return false;

  return private_->AccessibilityIsIgnored();
}

bool WebAXObject::LineBreaks(WebVector<int>& result) const {
  if (IsDetached())
    return false;

  Vector<int> line_breaks_vector;
  private_->LineBreaks(line_breaks_vector);
  result = line_breaks_vector;
  return true;
}

int WebAXObject::AriaColumnCount() const {
  if (IsDetached())
    return 0;

  return private_->IsTableLikeRole() ? private_->AriaColumnCount() : 0;
}

unsigned WebAXObject::AriaColumnIndex() const {
  if (IsDetached())
    return 0;

  return private_->IsTableCellLikeRole() ? private_->AriaColumnIndex() : 0;
}

int WebAXObject::AriaRowCount() const {
  if (IsDetached())
    return 0;

  return private_->IsTableLikeRole() ? private_->AriaRowCount() : 0;
}

unsigned WebAXObject::AriaRowIndex() const {
  if (IsDetached())
    return 0;

  return private_->AriaRowIndex();
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

unsigned WebAXObject::RowIndex() const {
  if (IsDetached())
    return 0;

  return private_->IsTableRowLikeRole() ? private_->RowIndex() : 0;
}

WebAXObject WebAXObject::RowHeader() const {
  if (IsDetached())
    return WebAXObject();

  if (!private_->IsTableRowLikeRole())
    return WebAXObject();

  return WebAXObject(private_->HeaderObject());
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
  std::copy(headers.begin(), headers.end(), row_header_elements.begin());
}

unsigned WebAXObject::ColumnIndex() const {
  if (IsDetached())
    return 0;

  if (private_->RoleValue() != ax::mojom::Role::kColumn)
    return 0;

  return private_->ColumnIndex();
}

WebAXObject WebAXObject::ColumnHeader() const {
  if (IsDetached())
    return WebAXObject();

  if (private_->RoleValue() != ax::mojom::Role::kColumn)
    return WebAXObject();

  return WebAXObject(private_->HeaderObject());
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
  std::copy(headers.begin(), headers.end(), column_header_elements.begin());
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

void WebAXObject::LoadInlineTextBoxes() const {
  if (IsDetached())
    return;

  private_->LoadInlineTextBoxes();
}

WebAXObject WebAXObject::NextOnLine() const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(private_.Get()->NextOnLine());
}

WebAXObject WebAXObject::PreviousOnLine() const {
  if (IsDetached())
    return WebAXObject();

  return WebAXObject(private_.Get()->PreviousOnLine());
}

static ax::mojom::MarkerType ToAXMarkerType(
    DocumentMarker::MarkerType marker_type) {
  switch (marker_type) {
    case DocumentMarker::kSpelling:
      return ax::mojom::MarkerType::kSpelling;
    case DocumentMarker::kGrammar:
      return ax::mojom::MarkerType::kGrammar;
    case DocumentMarker::kTextMatch:
      return ax::mojom::MarkerType::kTextMatch;
    case DocumentMarker::kActiveSuggestion:
      return ax::mojom::MarkerType::kActiveSuggestion;
    case DocumentMarker::kSuggestion:
      return ax::mojom::MarkerType::kSuggestion;
    default:
      return ax::mojom::MarkerType::kNone;
  }
}

void WebAXObject::Markers(WebVector<ax::mojom::MarkerType>& types,
                          WebVector<int>& starts,
                          WebVector<int>& ends) const {
  if (IsDetached())
    return;

  Vector<DocumentMarker::MarkerType> marker_types;
  Vector<AXRange> marker_ranges;
  private_->Markers(marker_types, marker_ranges);
  DCHECK_EQ(marker_types.size(), marker_ranges.size());

  WebVector<ax::mojom::MarkerType> web_marker_types(marker_types.size());
  WebVector<int> start_offsets(marker_ranges.size());
  WebVector<int> end_offsets(marker_ranges.size());
  for (wtf_size_t i = 0; i < marker_types.size(); ++i) {
    web_marker_types[i] = ToAXMarkerType(marker_types[i]);
    DCHECK(marker_ranges[i].IsValid());
    DCHECK_EQ(marker_ranges[i].Start().ContainerObject(),
              marker_ranges[i].End().ContainerObject());
    start_offsets[i] = marker_ranges[i].Start().TextOffset();
    end_offsets[i] = marker_ranges[i].End().TextOffset();
  }

  types.Swap(web_marker_types);
  starts.Swap(start_offsets);
  ends.Swap(end_offsets);
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

  Vector<AXRange> word_boundaries;
  private_->GetWordBoundaries(word_boundaries);

  WebVector<int> word_start_offsets(word_boundaries.size());
  WebVector<int> word_end_offsets(word_boundaries.size());
  for (wtf_size_t i = 0; i < word_boundaries.size(); ++i) {
    DCHECK(word_boundaries[i].IsValid());
    DCHECK_EQ(word_boundaries[i].Start().ContainerObject(),
              word_boundaries[i].End().ContainerObject());
    word_start_offsets[i] = word_boundaries[i].Start().TextOffset();
    word_end_offsets[i] = word_boundaries[i].End().TextOffset();
  }

  starts.Swap(word_start_offsets);
  ends.Swap(word_end_offsets);
}

bool WebAXObject::IsScrollableContainer() const {
  if (IsDetached())
    return false;

  return private_->IsScrollableContainer();
}

WebPoint WebAXObject::GetScrollOffset() const {
  if (IsDetached())
    return WebPoint();

  return private_->GetScrollOffset();
}

WebPoint WebAXObject::MinimumScrollOffset() const {
  if (IsDetached())
    return WebPoint();

  return private_->MinimumScrollOffset();
}

WebPoint WebAXObject::MaximumScrollOffset() const {
  if (IsDetached())
    return WebPoint();

  return private_->MaximumScrollOffset();
}

void WebAXObject::SetScrollOffset(const WebPoint& offset) const {
  if (IsDetached())
    return;

  private_->SetScrollOffset(offset);
}

void WebAXObject::GetRelativeBounds(WebAXObject& offset_container,
                                    WebFloatRect& bounds_in_container,
                                    SkMatrix44& container_transform,
                                    bool* clips_children) const {
  if (IsDetached())
    return;

#if DCHECK_IS_ON()
  DCHECK(IsLayoutClean(private_->GetDocument()));
#endif

  AXObject* container = nullptr;
  FloatRect bounds;
  private_->GetRelativeBounds(&container, bounds, container_transform,
                              clips_children);
  offset_container = WebAXObject(container);
  bounds_in_container = WebFloatRect(bounds);
}

bool WebAXObject::ScrollToMakeVisible() const {
  if (IsDetached())
    return false;

  return private_->RequestScrollToMakeVisibleAction();
}

bool WebAXObject::ScrollToMakeVisibleWithSubFocus(
    const WebRect& subfocus) const {
  if (IsDetached())
    return false;

  return private_->RequestScrollToMakeVisibleWithSubFocusAction(subfocus);
}

bool WebAXObject::ScrollToGlobalPoint(const WebPoint& point) const {
  if (IsDetached())
    return false;

  return private_->RequestScrollToGlobalPointAction(point);
}

WebAXObject::WebAXObject(AXObject* object) : private_(object) {}

WebAXObject& WebAXObject::operator=(AXObject* object) {
  private_ = object;
  return *this;
}

WebAXObject::operator AXObject*() const {
  return private_.Get();
}

// static
WebAXObject WebAXObject::FromWebNode(const WebNode& web_node) {
  WebDocument web_document = web_node.GetDocument();
  const Document* doc = web_document.ConstUnwrap<Document>();
  AXObjectCacheImpl* cache = ToAXObjectCacheImpl(doc->ExistingAXObjectCache());
  const Node* node = web_node.ConstUnwrap<Node>();
  return cache ? WebAXObject(cache->Get(node)) : WebAXObject();
}

// static
WebAXObject WebAXObject::FromWebDocument(const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();
  AXObjectCacheImpl* cache =
      ToAXObjectCacheImpl(document->ExistingAXObjectCache());
  return cache ? WebAXObject(cache->GetOrCreate(document->GetLayoutView()))
               : WebAXObject();
}

// static
WebAXObject WebAXObject::FromWebDocumentByID(const WebDocument& web_document,
                                             int ax_id) {
  const Document* document = web_document.ConstUnwrap<Document>();
  AXObjectCacheImpl* cache =
      ToAXObjectCacheImpl(document->ExistingAXObjectCache());
  return cache ? WebAXObject(cache->ObjectFromAXID(ax_id)) : WebAXObject();
}

// static
WebAXObject WebAXObject::FromWebDocumentFocused(
    const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();
  AXObjectCacheImpl* cache =
      ToAXObjectCacheImpl(document->ExistingAXObjectCache());
  return cache ? WebAXObject(cache->FocusedObject()) : WebAXObject();
}

}  // namespace blink
