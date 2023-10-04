// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_ax_object_proxy.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/strings/stringprintf.h"
#include "gin/handle.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace content {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining web tests.
std::string RoleToString(ax::mojom::Role role) {
  std::string prefix = "k";
  std::ostringstream result;
  result << role;
  // Check that |result| starts with |prefix|.
  DCHECK_EQ(result.str().find(prefix), 0ull);
  return "AXRole: AX" + result.str().substr(prefix.size());
}

std::string GetStringValue(const blink::WebAXObject& object) {
  std::string value;
  if (object.Role() == ax::mojom::Role::kColorWell) {
    unsigned int color = object.ColorValue();
    unsigned int red = (color >> 16) & 0xFF;
    unsigned int green = (color >> 8) & 0xFF;
    unsigned int blue = color & 0xFF;
    value = base::StringPrintf("rgba(%d, %d, %d, 1)", red, green, blue);
  } else {
    value = object.GetValueForControl().Utf8();
  }
  return value.insert(0, "AXValue: ");
}

std::string GetRole(const blink::WebAXObject& object) {
  std::string role_string = RoleToString(object.Role());

  // Special-case canvas with fallback content because Chromium wants to treat
  // this as essentially a separate role that it can map differently depending
  // on the platform.
  if (object.Role() == ax::mojom::Role::kCanvas &&
      object.CanvasHasFallbackContent()) {
    role_string += "WithFallbackContent";
  }

  return role_string;
}

std::string GetLanguage(const blink::WebAXObject& object) {
  std::string language = object.Language().Utf8();
  return language.insert(0, "AXLanguage: ");
}

std::string GetAttributes(const blink::WebAXObject& object) {
  std::string attributes(object.GetName().Utf8());
  attributes.append("\n");
  attributes.append(GetRole(object));
  return attributes;
}

// New bounds calculation algorithm.  Retrieves the frame-relative bounds
// of an object by calling getRelativeBounds and then applying the offsets
// and transforms recursively on each container of this object.
gfx::RectF BoundsForObject(const blink::WebAXObject& object) {
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  object.GetRelativeBounds(container, bounds, transform);
  gfx::RectF computed_bounds(0, 0, bounds.width(), bounds.height());
  while (!container.IsDetached()) {
    computed_bounds.Offset(bounds.x(), bounds.y());
    computed_bounds.Offset(-container.GetScrollOffset().x(),
                           -container.GetScrollOffset().y());
    computed_bounds = transform.MapRect(computed_bounds);
    container.GetRelativeBounds(container, bounds, transform);
  }
  return computed_bounds;
}

gfx::Rect BoundsForCharacter(const blink::WebAXObject& object,
                             int character_index) {
  DCHECK_EQ(object.Role(), ax::mojom::Role::kStaticText);
  int end = 0;
  for (unsigned i = 0; i < object.ChildCount(); i++) {
    blink::WebAXObject inline_text_box = object.ChildAt(i);
    DCHECK_EQ(inline_text_box.Role(), ax::mojom::Role::kInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.GetName();
    end += name.length();
    if (character_index < start || character_index >= end)
      continue;

    gfx::RectF inline_text_box_rect = BoundsForObject(inline_text_box);

    int local_index = character_index - start;
    blink::WebVector<int> character_offsets;
    inline_text_box.CharacterOffsets(character_offsets);
    if (character_offsets.size() != name.length())
      return gfx::Rect();

    switch (inline_text_box.GetTextDirection()) {
      case ax::mojom::WritingDirection::kLtr: {
        if (local_index) {
          int left =
              inline_text_box_rect.x() + character_offsets[local_index - 1];
          int width = character_offsets[local_index] -
                      character_offsets[local_index - 1];
          return gfx::Rect(left, inline_text_box_rect.y(), width,
                           inline_text_box_rect.height());
        }
        return gfx::Rect(inline_text_box_rect.x(), inline_text_box_rect.y(),
                         character_offsets[0], inline_text_box_rect.height());
      }
      case ax::mojom::WritingDirection::kRtl: {
        int right = inline_text_box_rect.x() + inline_text_box_rect.width();

        if (local_index) {
          int left = right - character_offsets[local_index];
          int width = character_offsets[local_index] -
                      character_offsets[local_index - 1];
          return gfx::Rect(left, inline_text_box_rect.y(), width,
                           inline_text_box_rect.height());
        }
        int left = right - character_offsets[0];
        return gfx::Rect(left, inline_text_box_rect.y(), character_offsets[0],
                         inline_text_box_rect.height());
      }
      case ax::mojom::WritingDirection::kTtb: {
        if (local_index) {
          int top =
              inline_text_box_rect.y() + character_offsets[local_index - 1];
          int height = character_offsets[local_index] -
                       character_offsets[local_index - 1];
          return gfx::Rect(inline_text_box_rect.x(), top,
                           inline_text_box_rect.width(), height);
        }
        return gfx::Rect(inline_text_box_rect.x(), inline_text_box_rect.y(),
                         inline_text_box_rect.width(), character_offsets[0]);
      }
      case ax::mojom::WritingDirection::kBtt: {
        int bottom = inline_text_box_rect.y() + inline_text_box_rect.height();

        if (local_index) {
          int top = bottom - character_offsets[local_index];
          int height = character_offsets[local_index] -
                       character_offsets[local_index - 1];
          return gfx::Rect(inline_text_box_rect.x(), top,
                           inline_text_box_rect.width(), height);
        }
        int top = bottom - character_offsets[0];
        return gfx::Rect(inline_text_box_rect.x(), top,
                         inline_text_box_rect.width(), character_offsets[0]);
      }
      default:
        NOTREACHED();
        return gfx::Rect();
    }
  }

  DCHECK(false);
  return gfx::Rect();
}

void GetBoundariesForOneWord(const blink::WebAXObject& object,
                             int character_index,
                             int& word_start,
                             int& word_end) {
  int end = 0;
  for (size_t i = 0; i < object.ChildCount(); i++) {
    blink::WebAXObject inline_text_box = object.ChildAt(i);
    DCHECK_EQ(inline_text_box.Role(), ax::mojom::Role::kInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.GetName();
    end += name.length();
    if (end <= character_index)
      continue;
    int local_index = character_index - start;

    blink::WebVector<int> starts;
    blink::WebVector<int> ends;
    inline_text_box.GetWordBoundaries(starts, ends);
    size_t word_count = starts.size();
    DCHECK_EQ(ends.size(), word_count);

    // If there are no words, use the InlineTextBox boundaries.
    if (!word_count) {
      word_start = start;
      word_end = end;
      return;
    }

    // Look for a character within any word other than the last.
    for (size_t j = 0; j < word_count - 1; j++) {
      if (local_index < ends[j]) {
        word_start = start + starts[j];
        word_end = start + ends[j];
        return;
      }
    }

    // Return the last word by default.
    word_start = start + starts[word_count - 1];
    word_end = start + ends[word_count - 1];
    return;
  }
}

// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
 public:
  AttributesCollector() {}

  AttributesCollector(const AttributesCollector&) = delete;
  AttributesCollector& operator=(const AttributesCollector&) = delete;

  ~AttributesCollector() {}

  void CollectAttributes(const blink::WebAXObject& object) {
    attributes_.append("\n------------\n");
    attributes_.append(GetAttributes(object));
  }

  std::string attributes() const { return attributes_; }

 private:
  std::string attributes_;
};

}  // namespace

gin::WrapperInfo WebAXObjectProxy::kWrapperInfo = {gin::kEmbedderNativeGin};

WebAXObjectProxy::WebAXObjectProxy(const blink::WebAXObject& object,
                                   WebAXObjectProxy::Factory* factory)
    : accessibility_object_(object), factory_(factory) {}

WebAXObjectProxy::~WebAXObjectProxy() = default;

void WebAXObjectProxy::UpdateLayout() {
  DCHECK(factory());
  DCHECK(factory()->GetAXContext());
  factory()->GetAXContext()->UpdateAXForAllDocuments();
}

ui::AXNodeData WebAXObjectProxy::GetAXNodeData() const {
  ui::AXNodeData node_data;
  if (!IsDetached())
    accessibility_object_.Serialize(&node_data, ui::kAXModeComplete);
  return node_data;
}

gin::ObjectTemplateBuilder WebAXObjectProxy::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebAXObjectProxy>::GetObjectTemplateBuilder(isolate)
      .SetProperty("role", &WebAXObjectProxy::Role)
      .SetProperty("stringValue", &WebAXObjectProxy::StringValue)
      .SetProperty("language", &WebAXObjectProxy::Language)
      .SetProperty("x", &WebAXObjectProxy::X)
      .SetProperty("y", &WebAXObjectProxy::Y)
      .SetProperty("width", &WebAXObjectProxy::Width)
      .SetProperty("height", &WebAXObjectProxy::Height)
      .SetProperty("inPageLinkTarget", &WebAXObjectProxy::InPageLinkTarget)
      .SetProperty("intValue", &WebAXObjectProxy::IntValue)
      .SetProperty("minValue", &WebAXObjectProxy::MinValue)
      .SetProperty("maxValue", &WebAXObjectProxy::MaxValue)
      .SetProperty("stepValue", &WebAXObjectProxy::StepValue)
      .SetProperty("valueDescription", &WebAXObjectProxy::ValueDescription)
      .SetProperty("childrenCount", &WebAXObjectProxy::ChildrenCount)
      .SetProperty("selectionIsBackward",
                   &WebAXObjectProxy::SelectionIsBackward)
      .SetProperty("selectionAnchorObject",
                   &WebAXObjectProxy::SelectionAnchorObject)
      .SetProperty("selectionAnchorOffset",
                   &WebAXObjectProxy::SelectionAnchorOffset)
      .SetProperty("selectionAnchorAffinity",
                   &WebAXObjectProxy::SelectionAnchorAffinity)
      .SetProperty("selectionFocusObject",
                   &WebAXObjectProxy::SelectionFocusObject)
      .SetProperty("selectionFocusOffset",
                   &WebAXObjectProxy::SelectionFocusOffset)
      .SetProperty("selectionFocusAffinity",
                   &WebAXObjectProxy::SelectionFocusAffinity)
      .SetProperty("isAtomic", &WebAXObjectProxy::IsAtomic)
      .SetProperty("isAutofillAvailable",
                   &WebAXObjectProxy::IsAutofillAvailable)
      .SetProperty("isBusy", &WebAXObjectProxy::IsBusy)
      .SetProperty("isRequired", &WebAXObjectProxy::IsRequired)
      .SetProperty("isEditable", &WebAXObjectProxy::IsEditable)
      .SetProperty("isEditableRoot", &WebAXObjectProxy::IsEditableRoot)
      .SetProperty("isRichlyEditable", &WebAXObjectProxy::IsRichlyEditable)
      .SetProperty("isFocused", &WebAXObjectProxy::IsFocused)
      .SetProperty("isFocusable", &WebAXObjectProxy::IsFocusable)
      .SetProperty("isModal", &WebAXObjectProxy::IsModal)
      .SetProperty("isSelected", &WebAXObjectProxy::IsSelected)
      .SetProperty("isSelectable", &WebAXObjectProxy::IsSelectable)
      .SetProperty("isMultiLine", &WebAXObjectProxy::IsMultiLine)
      .SetProperty("isMultiSelectable", &WebAXObjectProxy::IsMultiSelectable)
      .SetProperty("isSelectedOptionActive",
                   &WebAXObjectProxy::IsSelectedOptionActive)
      .SetProperty("isExpanded", &WebAXObjectProxy::IsExpanded)
      .SetProperty("checked", &WebAXObjectProxy::Checked)
      .SetProperty("isVisible", &WebAXObjectProxy::IsVisible)
      .SetProperty("isVisited", &WebAXObjectProxy::IsVisited)
      .SetProperty("isOffScreen", &WebAXObjectProxy::IsOffScreen)
      .SetProperty("isCollapsed", &WebAXObjectProxy::IsCollapsed)
      .SetProperty("hasPopup", &WebAXObjectProxy::HasPopup)
      .SetProperty("isValid", &WebAXObjectProxy::IsValid)
      .SetProperty("isReadOnly", &WebAXObjectProxy::IsReadOnly)
      .SetProperty("isIgnored", &WebAXObjectProxy::IsIgnored)
      .SetProperty("restriction", &WebAXObjectProxy::Restriction)
      .SetProperty("activeDescendant", &WebAXObjectProxy::ActiveDescendant)
      .SetProperty("backgroundColor", &WebAXObjectProxy::BackgroundColor)
      .SetProperty("color", &WebAXObjectProxy::Color)
      .SetProperty("colorValue", &WebAXObjectProxy::ColorValue)
      .SetProperty("fontFamily", &WebAXObjectProxy::FontFamily)
      .SetProperty("fontSize", &WebAXObjectProxy::FontSize)
      .SetProperty("autocomplete", &WebAXObjectProxy::Autocomplete)
      .SetProperty("current", &WebAXObjectProxy::Current)
      .SetProperty("invalid", &WebAXObjectProxy::Invalid)
      .SetProperty("keyShortcuts", &WebAXObjectProxy::KeyShortcuts)
      .SetProperty("ariaColumnCount", &WebAXObjectProxy::AriaColumnCount)
      .SetProperty("ariaColumnIndex", &WebAXObjectProxy::AriaColumnIndex)
      .SetProperty("ariaColumnSpan", &WebAXObjectProxy::AriaColumnSpan)
      .SetProperty("ariaRowCount", &WebAXObjectProxy::AriaRowCount)
      .SetProperty("ariaRowIndex", &WebAXObjectProxy::AriaRowIndex)
      .SetProperty("ariaRowSpan", &WebAXObjectProxy::AriaRowSpan)
      .SetProperty("live", &WebAXObjectProxy::Live)
      .SetProperty("orientation", &WebAXObjectProxy::Orientation)
      .SetProperty("relevant", &WebAXObjectProxy::Relevant)
      .SetProperty("roleDescription", &WebAXObjectProxy::RoleDescription)
      .SetProperty("sort", &WebAXObjectProxy::Sort)
      .SetProperty("url", &WebAXObjectProxy::Url)
      .SetProperty("hierarchicalLevel", &WebAXObjectProxy::HierarchicalLevel)
      .SetProperty("posInSet", &WebAXObjectProxy::PosInSet)
      .SetProperty("setSize", &WebAXObjectProxy::SetSize)
      .SetProperty("clickPointX", &WebAXObjectProxy::ClickPointX)
      .SetProperty("clickPointY", &WebAXObjectProxy::ClickPointY)
      .SetProperty("rowCount", &WebAXObjectProxy::RowCount)
      .SetProperty("rowHeadersCount", &WebAXObjectProxy::RowHeadersCount)
      .SetProperty("columnCount", &WebAXObjectProxy::ColumnCount)
      .SetProperty("columnHeadersCount", &WebAXObjectProxy::ColumnHeadersCount)
      .SetProperty("isClickable", &WebAXObjectProxy::IsClickable)
      //
      // NEW bounding rect calculation - high-level interface
      //
      .SetProperty("boundsX", &WebAXObjectProxy::BoundsX)
      .SetProperty("boundsY", &WebAXObjectProxy::BoundsY)
      .SetProperty("boundsWidth", &WebAXObjectProxy::BoundsWidth)
      .SetProperty("boundsHeight", &WebAXObjectProxy::BoundsHeight)
      .SetMethod("allAttributes", &WebAXObjectProxy::AllAttributes)
      .SetMethod("attributesOfChildren",
                 &WebAXObjectProxy::AttributesOfChildren)
      .SetMethod("ariaActiveDescendantElement",
                 &WebAXObjectProxy::AriaActiveDescendantElement)
      .SetMethod("ariaControlsElementAtIndex",
                 &WebAXObjectProxy::AriaControlsElementAtIndex)
      .SetMethod("ariaDetailsElementAtIndex",
                 &WebAXObjectProxy::AriaDetailsElementAtIndex)
      .SetMethod("ariaErrorMessageElementAtIndex",
                 &WebAXObjectProxy::AriaErrorMessageElementAtIndex)
      .SetMethod("ariaFlowToElementAtIndex",
                 &WebAXObjectProxy::AriaFlowToElementAtIndex)
      .SetMethod("ariaOwnsElementAtIndex",
                 &WebAXObjectProxy::AriaOwnsElementAtIndex)
      .SetMethod("boundsForRange", &WebAXObjectProxy::BoundsForRange)
      .SetMethod("childAtIndex", &WebAXObjectProxy::ChildAtIndex)
      .SetMethod("elementAtPoint", &WebAXObjectProxy::ElementAtPoint)
      .SetMethod("rowHeaderAtIndex", &WebAXObjectProxy::RowHeaderAtIndex)
      .SetMethod("columnHeaderAtIndex", &WebAXObjectProxy::ColumnHeaderAtIndex)
      .SetMethod("rowIndexRange", &WebAXObjectProxy::RowIndexRange)
      .SetMethod("columnIndexRange", &WebAXObjectProxy::ColumnIndexRange)
      .SetMethod("cellForColumnAndRow", &WebAXObjectProxy::CellForColumnAndRow)
      .SetMethod("setSelectedTextRange",
                 &WebAXObjectProxy::SetSelectedTextRange)
      .SetMethod("setSelection", &WebAXObjectProxy::SetSelection)
      .SetMethod("isAttributeSettable", &WebAXObjectProxy::IsAttributeSettable)
      .SetMethod("isPressActionSupported",
                 &WebAXObjectProxy::IsPressActionSupported)
      .SetMethod("hasDefaultAction", &WebAXObjectProxy::HasDefaultAction)
      .SetMethod("parentElement", &WebAXObjectProxy::ParentElement)
      .SetMethod("increment", &WebAXObjectProxy::Increment)
      .SetMethod("decrement", &WebAXObjectProxy::Decrement)
      .SetMethod("showMenu", &WebAXObjectProxy::ShowMenu)
      .SetMethod("press", &WebAXObjectProxy::Press)
      .SetMethod("setValue", &WebAXObjectProxy::SetValue)
      .SetMethod("isEqual", &WebAXObjectProxy::IsEqual)
      .SetMethod("setNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("unsetNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      .SetMethod("takeFocus", &WebAXObjectProxy::TakeFocus)
      .SetMethod("scrollToMakeVisible", &WebAXObjectProxy::ScrollToMakeVisible)
      .SetMethod("scrollToMakeVisibleWithSubFocus",
                 &WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus)
      .SetMethod("scrollToGlobalPoint", &WebAXObjectProxy::ScrollToGlobalPoint)
      .SetMethod("scrollUp", &WebAXObjectProxy::ScrollUp)
      .SetMethod("scrollDown", &WebAXObjectProxy::ScrollDown)
      .SetMethod("scrollX", &WebAXObjectProxy::ScrollX)
      .SetMethod("scrollY", &WebAXObjectProxy::ScrollY)
      .SetMethod("toString", &WebAXObjectProxy::ToString)
      .SetMethod("wordStart", &WebAXObjectProxy::WordStart)
      .SetMethod("wordEnd", &WebAXObjectProxy::WordEnd)
      .SetMethod("nextOnLine", &WebAXObjectProxy::NextOnLine)
      .SetMethod("previousOnLine", &WebAXObjectProxy::PreviousOnLine)
      .SetMethod("misspellingAtIndex", &WebAXObjectProxy::MisspellingAtIndex)
      // TODO(hajimehoshi): This is for backward compatibility. Remove them.
      .SetMethod("addNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("removeNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      //
      // NEW accessible name and description accessors
      //
      .SetProperty("name", &WebAXObjectProxy::Name)
      .SetProperty("nameFrom", &WebAXObjectProxy::NameFrom)
      .SetMethod("nameElementCount", &WebAXObjectProxy::NameElementCount)
      .SetMethod("nameElementAtIndex", &WebAXObjectProxy::NameElementAtIndex)
      .SetProperty("description", &WebAXObjectProxy::Description)
      .SetProperty("descriptionFrom", &WebAXObjectProxy::DescriptionFrom)
      .SetProperty("placeholder", &WebAXObjectProxy::Placeholder)
      .SetProperty("misspellingsCount", &WebAXObjectProxy::MisspellingsCount)
      .SetMethod("descriptionElementCount",
                 &WebAXObjectProxy::DescriptionElementCount)
      .SetMethod("descriptionElementAtIndex",
                 &WebAXObjectProxy::DescriptionElementAtIndex)
      //
      // NEW bounding rect calculation - low-level interface
      //
      .SetMethod("offsetContainer", &WebAXObjectProxy::OffsetContainer)
      .SetMethod("boundsInContainerX", &WebAXObjectProxy::BoundsInContainerX)
      .SetMethod("boundsInContainerY", &WebAXObjectProxy::BoundsInContainerY)
      .SetMethod("boundsInContainerWidth",
                 &WebAXObjectProxy::BoundsInContainerWidth)
      .SetMethod("boundsInContainerHeight",
                 &WebAXObjectProxy::BoundsInContainerHeight)
      .SetMethod("hasNonIdentityTransform",
                 &WebAXObjectProxy::HasNonIdentityTransform);
}

v8::Local<v8::Object> WebAXObjectProxy::GetChildAtIndex(unsigned index) {
  if (IsDetached())
    return v8::Local<v8::Object>();
  UpdateLayout();
  return factory_->GetOrCreate(accessibility_object_.ChildAt(index));
}

bool WebAXObjectProxy::IsRoot() const {
  return false;
}

bool WebAXObjectProxy::IsEqualToObject(const blink::WebAXObject& other) {
  return accessibility_object_.Equals(other);
}

void WebAXObjectProxy::NotificationReceived(
    blink::WebLocalFrame* frame,
    const std::string& notification_name,
    const std::vector<ui::AXEventIntent>& event_intents) {
  if (notification_callback_.IsEmpty())
    return;

  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();

  v8::Local<v8::Array> intents_array(
      v8::Array::New(isolate, event_intents.size()));
  for (size_t i = 0; i < event_intents.size(); ++i) {
    intents_array
        ->CreateDataProperty(context, static_cast<uint32_t>(i),
                             v8::String::NewFromUtf8(
                                 isolate, event_intents[i].ToString().c_str())
                                 .ToLocalChecked())
        .Check();
  }

  v8::Local<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, notification_name.c_str())
          .ToLocalChecked(),
      intents_array};
  // TODO(aboxhall): Can we force this to run in a new task, to avoid
  // dirtying layout during post-layout hooks?
  frame->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, notification_callback_),
      context->Global(), std::size(argv), argv);
}

void WebAXObjectProxy::Reset() {
  notification_callback_.Reset();
  factory_ = nullptr;
  accessibility_object_ = blink::WebAXObject();
}

std::string WebAXObjectProxy::Role() {
  UpdateLayout();
  return GetRole(accessibility_object_);
}

std::string WebAXObjectProxy::StringValue() {
  UpdateLayout();
  return GetStringValue(accessibility_object_);
}

std::string WebAXObjectProxy::Language() {
  UpdateLayout();
  return GetLanguage(accessibility_object_);
}

int WebAXObjectProxy::X() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).x();
}

int WebAXObjectProxy::Y() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).y();
}

int WebAXObjectProxy::Width() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).width();
}

int WebAXObjectProxy::Height() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).height();
}

v8::Local<v8::Value> WebAXObjectProxy::InPageLinkTarget(v8::Isolate* isolate) {
  if (IsDetached())
    return v8::Local<v8::Object>();
  UpdateLayout();
  blink::WebAXObject target = accessibility_object_.InPageLinkTarget();
  if (target.IsNull())
    return v8::Null(isolate);
  return factory_->GetOrCreate(target);
}

int WebAXObjectProxy::IntValue() {
  UpdateLayout();

  if (accessibility_object_.SupportsRangeValue()) {
    float value = 0.0f;
    accessibility_object_.ValueForRange(&value);
    return static_cast<int>(value);
  } else if (accessibility_object_.Role() == ax::mojom::Role::kHeading) {
    return accessibility_object_.HeadingLevel();
  } else {
    return atoi(accessibility_object_.GetValueForControl().Utf8().data());
  }
}

int WebAXObjectProxy::MinValue() {
  UpdateLayout();
  float min_value = 0.0f;
  accessibility_object_.MinValueForRange(&min_value);
  return min_value;
}

int WebAXObjectProxy::MaxValue() {
  UpdateLayout();
  float max_value = 0.0f;
  accessibility_object_.MaxValueForRange(&max_value);
  return max_value;
}

int WebAXObjectProxy::StepValue() {
  UpdateLayout();
  float step_value = 0.0f;
  accessibility_object_.StepValueForRange(&step_value);
  return step_value;
}

std::string WebAXObjectProxy::ValueDescription() {
  UpdateLayout();
  std::string value_description =
      GetAXNodeData().GetStringAttribute(ax::mojom::StringAttribute::kValue);
  return value_description.insert(0, "AXValueDescription: ");
}

int WebAXObjectProxy::ChildrenCount() {
  UpdateLayout();
  int count = 1;  // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = accessibility_object_.ChildCount();
  return count;
}

bool WebAXObjectProxy::SelectionIsBackward() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return is_selection_backward;
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionAnchorObject(
    v8::Isolate* isolate) {
  if (IsDetached())
    return v8::Local<v8::Object>();

  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (anchor_object.IsNull())
    return v8::Null(isolate);

  return factory_->GetOrCreate(anchor_object);
}

int WebAXObjectProxy::SelectionAnchorOffset() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (anchor_offset < 0)
    return -1;

  return anchor_offset;
}

std::string WebAXObjectProxy::SelectionAnchorAffinity() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return anchor_affinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                               : "downstream";
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionFocusObject(
    v8::Isolate* isolate) {
  if (IsDetached())
    return v8::Local<v8::Object>();

  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (focus_object.IsNull())
    return v8::Null(isolate);

  return factory_->GetOrCreate(focus_object);
}

int WebAXObjectProxy::SelectionFocusOffset() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (focus_offset < 0)
    return -1;

  return focus_offset;
}

std::string WebAXObjectProxy::SelectionFocusAffinity() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return focus_affinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                              : "downstream";
}

bool WebAXObjectProxy::IsAtomic() {
  UpdateLayout();
  return accessibility_object_.LiveRegionAtomic();
}

bool WebAXObjectProxy::IsAutofillAvailable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kAutofillAvailable);
}

bool WebAXObjectProxy::IsBusy() {
  UpdateLayout();
  return GetAXNodeData().GetBoolAttribute(ax::mojom::BoolAttribute::kBusy);
}

std::string WebAXObjectProxy::Restriction() {
  UpdateLayout();
  blink::WebAXRestriction web_ax_restriction =
      static_cast<blink::WebAXRestriction>(GetAXNodeData().GetRestriction());
  switch (web_ax_restriction) {
    case blink::kWebAXRestrictionReadOnly:
      return "readOnly";
    case blink::kWebAXRestrictionDisabled:
      return "disabled";
    case blink::kWebAXRestrictionNone:
      break;
  }
  return "none";
}

bool WebAXObjectProxy::IsRequired() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kRequired);
}

bool WebAXObjectProxy::IsEditableRoot() {
  UpdateLayout();
  return GetAXNodeData().GetBoolAttribute(
             ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot) &&
         GetAXNodeData().HasState(ax::mojom::State::kEditable);
}

bool WebAXObjectProxy::IsEditable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kEditable);
}

bool WebAXObjectProxy::IsRichlyEditable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kRichlyEditable);
}

bool WebAXObjectProxy::IsFocused() {
  UpdateLayout();
  return accessibility_object_.IsFocused();
}

bool WebAXObjectProxy::IsFocusable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kFocusable);
}

bool WebAXObjectProxy::IsModal() {
  UpdateLayout();
  return accessibility_object_.IsModal();
}

bool WebAXObjectProxy::IsSelected() {
  UpdateLayout();
  return GetAXNodeData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

bool WebAXObjectProxy::IsSelectable() {
  UpdateLayout();
  ui::AXNodeData node_data = GetAXNodeData();
  // It's selectable if it has the attribute, whether it's true or false.
  return node_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
         node_data.GetRestriction() != ax::mojom::Restriction::kDisabled;
}

bool WebAXObjectProxy::IsMultiLine() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kMultiline);
}

bool WebAXObjectProxy::IsMultiSelectable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kMultiselectable);
}

bool WebAXObjectProxy::IsSelectedOptionActive() {
  UpdateLayout();
  return accessibility_object_.IsSelectedOptionActive();
}

bool WebAXObjectProxy::IsExpanded() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kExpanded);
}

std::string WebAXObjectProxy::Checked() {
  UpdateLayout();
  switch (accessibility_object_.CheckedState()) {
    case ax::mojom::CheckedState::kTrue:
      return "true";
    case ax::mojom::CheckedState::kMixed:
      return "mixed";
    case ax::mojom::CheckedState::kFalse:
      return "false";
    default:
      return std::string();
  }
}

bool WebAXObjectProxy::IsCollapsed() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kCollapsed);
}

bool WebAXObjectProxy::IsVisible() {
  UpdateLayout();
  return !GetAXNodeData().IsInvisible();
}

bool WebAXObjectProxy::IsVisited() {
  UpdateLayout();
  return accessibility_object_.IsVisited();
}

bool WebAXObjectProxy::IsOffScreen() {
  UpdateLayout();
  return accessibility_object_.IsOffScreen();
}

bool WebAXObjectProxy::IsValid() {
  UpdateLayout();
  return !accessibility_object_.IsDetached();
}

bool WebAXObjectProxy::IsReadOnly() {
  UpdateLayout();
  return GetAXNodeData().GetRestriction() == ax::mojom::Restriction::kReadOnly;
}

bool WebAXObjectProxy::IsIgnored() {
  UpdateLayout();
  return accessibility_object_.AccessibilityIsIgnored();
}

v8::Local<v8::Object> WebAXObjectProxy::ActiveDescendant() {
  if (IsDetached())
    return v8::Local<v8::Object>();
  UpdateLayout();
  blink::WebAXObject element = accessibility_object_.AriaActiveDescendant();
  return factory_->GetOrCreate(element);
}

unsigned int WebAXObjectProxy::BackgroundColor() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kBackgroundColor);
}

unsigned int WebAXObjectProxy::Color() {
  UpdateLayout();
  unsigned int color =
      GetAXNodeData().GetIntAttribute(ax::mojom::IntAttribute::kColor);
  // Remove the alpha because it's always 1 and thus not informative.
  return color & 0xFFFFFF;
}

// For input elements of type color.
unsigned int WebAXObjectProxy::ColorValue() {
  UpdateLayout();
  return accessibility_object_.ColorValue();
}

std::string WebAXObjectProxy::FontFamily() {
  UpdateLayout();
  std::string font_family = GetAXNodeData().GetStringAttribute(
      ax::mojom::StringAttribute::kFontFamily);
  return font_family.insert(0, "AXFontFamily: ");
}

float WebAXObjectProxy::FontSize() {
  UpdateLayout();
  return GetAXNodeData().GetFloatAttribute(
      ax::mojom::FloatAttribute::kFontSize);
}

std::string WebAXObjectProxy::Autocomplete() {
  UpdateLayout();
  return accessibility_object_.AutoComplete().Utf8();
}

std::string WebAXObjectProxy::Current() {
  UpdateLayout();
  switch (accessibility_object_.AriaCurrentState()) {
    case ax::mojom::AriaCurrentState::kFalse:
      return "false";
    case ax::mojom::AriaCurrentState::kTrue:
      return "true";
    case ax::mojom::AriaCurrentState::kPage:
      return "page";
    case ax::mojom::AriaCurrentState::kStep:
      return "step";
    case ax::mojom::AriaCurrentState::kLocation:
      return "location";
    case ax::mojom::AriaCurrentState::kDate:
      return "date";
    case ax::mojom::AriaCurrentState::kTime:
      return "time";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::HasPopup() {
  UpdateLayout();
  switch (GetAXNodeData().GetHasPopup()) {
    case ax::mojom::HasPopup::kTrue:
      return "true";
    case ax::mojom::HasPopup::kMenu:
      return "menu";
    case ax::mojom::HasPopup::kListbox:
      return "listbox";
    case ax::mojom::HasPopup::kTree:
      return "tree";
    case ax::mojom::HasPopup::kGrid:
      return "grid";
    case ax::mojom::HasPopup::kDialog:
      return "dialog";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::Invalid() {
  UpdateLayout();
  switch (accessibility_object_.InvalidState()) {
    case ax::mojom::InvalidState::kFalse:
      return "false";
    case ax::mojom::InvalidState::kTrue:
      return "true";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::KeyShortcuts() {
  UpdateLayout();
  return GetAXNodeData().GetStringAttribute(
      ax::mojom::StringAttribute::kKeyShortcuts);
}

int32_t WebAXObjectProxy::AriaColumnCount() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaColumnCount);
}

uint32_t WebAXObjectProxy::AriaColumnIndex() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellColumnIndex);
}

uint32_t WebAXObjectProxy::AriaColumnSpan() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellColumnSpan);
}

int32_t WebAXObjectProxy::AriaRowCount() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaRowCount);
}

uint32_t WebAXObjectProxy::AriaRowIndex() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellRowIndex);
}

uint32_t WebAXObjectProxy::AriaRowSpan() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellRowSpan);
}

std::string WebAXObjectProxy::Live() {
  UpdateLayout();
  return accessibility_object_.LiveRegionStatus().Utf8();
}

std::string WebAXObjectProxy::Orientation() {
  UpdateLayout();
  ui::AXNodeData node_data = GetAXNodeData();
  if (node_data.HasState(ax::mojom::State::kVertical))
    return "AXOrientation: AXVerticalOrientation";
  else if (node_data.HasState(ax::mojom::State::kHorizontal))
    return "AXOrientation: AXHorizontalOrientation";
  return std::string();
}

std::string WebAXObjectProxy::Relevant() {
  UpdateLayout();
  return accessibility_object_.LiveRegionRelevant().Utf8();
}

std::string WebAXObjectProxy::RoleDescription() {
  UpdateLayout();
  return GetAXNodeData().GetStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription);
}

std::string WebAXObjectProxy::Sort() {
  UpdateLayout();
  switch (accessibility_object_.SortDirection()) {
    case ax::mojom::SortDirection::kAscending:
      return "ascending";
    case ax::mojom::SortDirection::kDescending:
      return "descending";
    case ax::mojom::SortDirection::kOther:
      return "other";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::Url() {
  UpdateLayout();
  return accessibility_object_.Url().GetString().Utf8();
}

int WebAXObjectProxy::HierarchicalLevel() {
  UpdateLayout();
  return accessibility_object_.HierarchicalLevel();
}

int WebAXObjectProxy::PosInSet() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(ax::mojom::IntAttribute::kPosInSet);
}

int WebAXObjectProxy::SetSize() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(ax::mojom::IntAttribute::kSetSize);
}

int WebAXObjectProxy::ClickPointX() {
  UpdateLayout();
  gfx::RectF bounds = BoundsForObject(accessibility_object_);
  return bounds.x() + bounds.width() / 2;
}

int WebAXObjectProxy::ClickPointY() {
  UpdateLayout();
  gfx::RectF bounds = BoundsForObject(accessibility_object_);
  return bounds.y() + bounds.height() / 2;
}

int32_t WebAXObjectProxy::RowCount() {
  UpdateLayout();
  return static_cast<int32_t>(accessibility_object_.RowCount());
}

int32_t WebAXObjectProxy::RowHeadersCount() {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

int32_t WebAXObjectProxy::ColumnCount() {
  UpdateLayout();
  return static_cast<int32_t>(accessibility_object_.ColumnCount());
}

int32_t WebAXObjectProxy::ColumnHeadersCount() {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

bool WebAXObjectProxy::IsClickable() {
  UpdateLayout();
  return accessibility_object_.IsClickable();
}

v8::Local<v8::Object> WebAXObjectProxy::AriaActiveDescendantElement() {
  UpdateLayout();
  int ax_id = GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId);

  if (!ax_id)
    return v8::Local<v8::Object>();

  blink::WebAXObject web_ax_object = blink::WebAXObject::FromWebDocumentByID(
      accessibility_object_.GetDocument(), ax_id);
  return factory_->GetOrCreate(web_ax_object);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaControlsElementAtIndex(
    unsigned index) {
  UpdateLayout();
  auto ax_ids = GetAXNodeData().GetIntListAttribute(
      ax::mojom::IntListAttribute::kControlsIds);
  size_t element_count = ax_ids.size();

  if (index >= element_count)
    return v8::Local<v8::Object>();

  blink::WebAXObject web_ax_object = blink::WebAXObject::FromWebDocumentByID(
      accessibility_object_.GetDocument(), ax_ids[index]);

  return factory_->GetOrCreate(web_ax_object);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaDetailsElementAtIndex(
    unsigned index) {
  UpdateLayout();
  auto ax_ids = GetAXNodeData().GetIntListAttribute(
      ax::mojom::IntListAttribute::kDetailsIds);
  size_t element_count = ax_ids.size();

  if (index >= element_count)
    return v8::Local<v8::Object>();

  blink::WebAXObject web_ax_object = blink::WebAXObject::FromWebDocumentByID(
      accessibility_object_.GetDocument(), ax_ids[index]);

  return factory_->GetOrCreate(web_ax_object);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaErrorMessageElementAtIndex(
    unsigned index) {
  UpdateLayout();
  auto ax_ids = GetAXNodeData().GetIntListAttribute(
      ax::mojom::IntListAttribute::kErrormessageIds);
  size_t element_count = ax_ids.size();

  if (index >= element_count) {
    return v8::Local<v8::Object>();
  }

  blink::WebAXObject web_ax_object = blink::WebAXObject::FromWebDocumentByID(
      accessibility_object_.GetDocument(), ax_ids[index]);

  return factory_->GetOrCreate(web_ax_object);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaFlowToElementAtIndex(
    unsigned index) {
  UpdateLayout();
  auto ax_ids = GetAXNodeData().GetIntListAttribute(
      ax::mojom::IntListAttribute::kFlowtoIds);
  size_t element_count = ax_ids.size();

  if (index >= element_count)
    return v8::Local<v8::Object>();

  blink::WebAXObject web_ax_object = blink::WebAXObject::FromWebDocumentByID(
      accessibility_object_.GetDocument(), ax_ids[index]);

  return factory_->GetOrCreate(web_ax_object);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaOwnsElementAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> elements;
  accessibility_object_.AriaOwns(elements);
  size_t element_count = elements.size();
  if (index >= element_count)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

std::string WebAXObjectProxy::AllAttributes() {
  UpdateLayout();
  return GetAttributes(accessibility_object_);
}

std::string WebAXObjectProxy::AttributesOfChildren() {
  UpdateLayout();
  AttributesCollector collector;
  unsigned size = accessibility_object_.ChildCount();
  for (unsigned i = 0; i < size; ++i)
    collector.CollectAttributes(accessibility_object_.ChildAt(i));
  return collector.attributes();
}

std::string WebAXObjectProxy::BoundsForRange(int start, int end) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return std::string();

  int len = end - start;

  // Get the bounds for each character and union them into one large rectangle.
  // This is just for testing so it doesn't need to be efficient.
  gfx::Rect bounds = BoundsForCharacter(accessibility_object_, start);
  for (int i = 1; i < len; i++) {
    gfx::Rect next = BoundsForCharacter(accessibility_object_, start + i);
    int right = std::max(bounds.x() + bounds.width(), next.x() + next.width());
    int bottom =
        std::max(bounds.y() + bounds.height(), next.y() + next.height());
    bounds.set_x(std::min(bounds.x(), next.x()));
    bounds.set_y(std::min(bounds.y(), next.y()));
    bounds.set_width(right - bounds.x());
    bounds.set_height(bottom - bounds.y());
  }

  return base::StringPrintf("{x: %d, y: %d, width: %d, height: %d}", bounds.x(),
                            bounds.y(), bounds.width(), bounds.height());
}

v8::Local<v8::Object> WebAXObjectProxy::ChildAtIndex(int index) {
  // Scripts can sometimes provide bad input.
  // Return undefined object in case of range error, rather than passing a bad
  // index into the a11y core, where it would trigger a DCHECK.
  int num_children = ChildrenCount();
  if (index < 0 || index >= num_children)
    return v8::Local<v8::Object>();

  return GetChildAtIndex(index);
}

v8::Local<v8::Object> WebAXObjectProxy::ElementAtPoint(int x, int y) {
  UpdateLayout();
  gfx::Point point(x, y);
  blink::WebAXObject obj = accessibility_object_.HitTest(point);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::RowHeaderAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  size_t header_count = headers.size();
  if (index >= header_count)
    return {};

  return factory_->GetOrCreate(headers[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::ColumnHeaderAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  size_t header_count = headers.size();
  if (index >= header_count)
    return {};

  return factory_->GetOrCreate(headers[index]);
}

std::string WebAXObjectProxy::RowIndexRange() {
  UpdateLayout();
  unsigned row_index = accessibility_object_.CellRowIndex();
  unsigned row_span = accessibility_object_.CellRowSpan();
  return base::StringPrintf("{%d, %d}", row_index, row_span);
}

std::string WebAXObjectProxy::ColumnIndexRange() {
  UpdateLayout();
  unsigned column_index = accessibility_object_.CellColumnIndex();
  unsigned column_span = accessibility_object_.CellColumnSpan();
  return base::StringPrintf("{%d, %d}", column_index, column_span);
}

v8::Local<v8::Object> WebAXObjectProxy::CellForColumnAndRow(int column,
                                                            int row) {
  UpdateLayout();
  blink::WebAXObject obj =
      accessibility_object_.CellForColumnAndRow(column, row);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

void WebAXObjectProxy::SetSelectedTextRange(int selection_start, int length) {
  UpdateLayout();
  accessibility_object_.SetSelection(accessibility_object_, selection_start,
                                     accessibility_object_,
                                     selection_start + length);
}

bool WebAXObjectProxy::SetSelection(v8::Isolate* isolate,
                                    v8::Local<v8::Value> anchor_object,
                                    int anchor_offset,
                                    v8::Local<v8::Value> focus_object,
                                    int focus_offset) {
  if (anchor_object.IsEmpty() || focus_object.IsEmpty() ||
      !anchor_object->IsObject() || !focus_object->IsObject() ||
      anchor_offset < 0 || focus_offset < 0) {
    return false;
  }

  WebAXObjectProxy* web_ax_anchor = nullptr;
  if (!gin::ConvertFromV8(isolate, anchor_object, &web_ax_anchor)) {
    return false;
  }
  DCHECK(web_ax_anchor);

  WebAXObjectProxy* web_ax_focus = nullptr;
  if (!gin::ConvertFromV8(isolate, focus_object, &web_ax_focus)) {
    return false;
  }
  DCHECK(web_ax_focus);

  UpdateLayout();
  return accessibility_object_.SetSelection(
      web_ax_anchor->accessibility_object_, anchor_offset,
      web_ax_focus->accessibility_object_, focus_offset);
}

bool WebAXObjectProxy::IsAttributeSettable(const std::string& attribute) {
  UpdateLayout();
  bool settable = false;
  if (attribute == "AXValue")
    settable = accessibility_object_.CanSetValueAttribute();
  return settable;
}

bool WebAXObjectProxy::IsPressActionSupported() {
  UpdateLayout();
  return accessibility_object_.Action() == ax::mojom::DefaultActionVerb::kPress;
}

bool WebAXObjectProxy::HasDefaultAction() {
  UpdateLayout();
  return accessibility_object_.Action() != ax::mojom::DefaultActionVerb::kNone;
}

v8::Local<v8::Object> WebAXObjectProxy::ParentElement() {
  UpdateLayout();
  blink::WebAXObject parent_object = accessibility_object_.ParentObject();
  return factory_->GetOrCreate(parent_object);
}

void WebAXObjectProxy::Increment() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kIncrement;
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::Decrement() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDecrement;
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::ShowMenu() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::Press() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  accessibility_object_.PerformAction(action_data);
}

bool WebAXObjectProxy::SetValue(const std::string& value) {
  UpdateLayout();
  if (!accessibility_object_.CanSetValueAttribute())
    return false;

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetValue;
  action_data.value = value;
  return accessibility_object_.PerformAction(action_data);
}

bool WebAXObjectProxy::IsEqual(v8::Isolate* isolate,
                               v8::Local<v8::Object> proxy) {
  WebAXObjectProxy* unwrapped_proxy = nullptr;
  if (!gin::ConvertFromV8(isolate, proxy, &unwrapped_proxy)) {
    return false;
  }
  return unwrapped_proxy->IsEqualToObject(accessibility_object_);
}

void WebAXObjectProxy::SetNotificationListener(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback) {
  notification_callback_.Reset(isolate, callback);
}

void WebAXObjectProxy::UnsetNotificationListener() {
  notification_callback_.Reset();
}

void WebAXObjectProxy::TakeFocus() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::ScrollToMakeVisible() {
  UpdateLayout();
  accessibility_object_.ScrollToMakeVisible();
}

void WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus(int x,
                                                       int y,
                                                       int width,
                                                       int height) {
  UpdateLayout();
  accessibility_object_.ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(x, y, width, height));
}

void WebAXObjectProxy::ScrollToGlobalPoint(int x, int y) {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = gfx::Point(x, y);
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::ScrollUp() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollUp;
  accessibility_object_.PerformAction(action_data);
}

void WebAXObjectProxy::ScrollDown() {
  UpdateLayout();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollDown;
  accessibility_object_.PerformAction(action_data);
}

int WebAXObjectProxy::ScrollX() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
}

int WebAXObjectProxy::ScrollY() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
}

std::string WebAXObjectProxy::ToString() {
  UpdateLayout();
  return accessibility_object_.ToString().Utf8();
}

float WebAXObjectProxy::BoundsX() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).x();
}

float WebAXObjectProxy::BoundsY() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).y();
}

float WebAXObjectProxy::BoundsWidth() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).width();
}

float WebAXObjectProxy::BoundsHeight() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).height();
}

int WebAXObjectProxy::WordStart(int character_index) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_start;
}

int WebAXObjectProxy::WordEnd(int character_index) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_end;
}

v8::Local<v8::Object> WebAXObjectProxy::NextOnLine() {
  UpdateLayout();
  blink::WebAXObject obj = accessibility_object_.NextOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::PreviousOnLine() {
  UpdateLayout();
  blink::WebAXObject obj = accessibility_object_.PreviousOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

std::vector<std::string> WebAXObjectProxy::GetMisspellings() const {
  std::vector<std::string> misspellings;
  std::string text(accessibility_object_.GetName().Utf8());
  if (text.empty())
    return {};

  const ui::AXNodeData& node_data = GetAXNodeData();
  std::vector<int32_t> marker_types;
  std::vector<int32_t> marker_starts;
  std::vector<int32_t> marker_ends;
  if (node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                                    &marker_types) &&
      node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                                    &marker_starts) &&
      node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                    &marker_ends)) {
    DCHECK_EQ(marker_types.size(), marker_starts.size());
    DCHECK_EQ(marker_types.size(), marker_ends.size());
    for (size_t i = 0; i < marker_types.size(); ++i) {
      if (marker_types[i] == int32_t(ax::mojom::MarkerType::kSpelling)) {
        DCHECK_LE(marker_starts[i], marker_ends[i]);
        misspellings.push_back(
            text.substr(marker_starts[i], marker_ends[i] - marker_starts[i]));
      }
    }

    return misspellings;
  }

  return {};
}

std::string WebAXObjectProxy::MisspellingAtIndex(int index) {
  UpdateLayout();

  std::vector<std::string> misspellings = GetMisspellings();
  if (index < 0 || index >= static_cast<int>(misspellings.size()))
    return std::string();
  return misspellings[index];
}

std::string WebAXObjectProxy::Name() {
  UpdateLayout();
  return accessibility_object_.GetName().Utf8();
}

std::string WebAXObjectProxy::NameFrom() {
  UpdateLayout();
  ax::mojom::NameFrom name_from = ax::mojom::NameFrom::kNone;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  switch (name_from) {
    case ax::mojom::NameFrom::kNone:
      return "";
    default:
      return ui::ToString(name_from);
  }
}

int WebAXObjectProxy::NameElementCount() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  return static_cast<int>(name_objects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::NameElementAtIndex(unsigned index) {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  if (index >= name_objects.size())
    return {};
  return factory_->GetOrCreate(name_objects[index]);
}

std::string WebAXObjectProxy::Description() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  return accessibility_object_
      .Description(name_from, description_from, description_objects)
      .Utf8();
}

std::string WebAXObjectProxy::DescriptionFrom() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from =
      ax::mojom::DescriptionFrom::kNone;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  switch (description_from) {
    case ax::mojom::DescriptionFrom::kNone:
      return "";
    default:
      return ui::ToString(description_from);
  }
}

std::string WebAXObjectProxy::Placeholder() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  return accessibility_object_.Placeholder(name_from).Utf8();
}

int WebAXObjectProxy::MisspellingsCount() {
  UpdateLayout();
  return GetMisspellings().size();
}

int WebAXObjectProxy::DescriptionElementCount() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  return static_cast<int>(description_objects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::DescriptionElementAtIndex(
    unsigned index) {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  if (index >= description_objects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(description_objects[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::OffsetContainer() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return factory_->GetOrCreate(container);
}

float WebAXObjectProxy::BoundsInContainerX() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return bounds.x();
}

float WebAXObjectProxy::BoundsInContainerY() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return bounds.y();
}

float WebAXObjectProxy::BoundsInContainerWidth() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return bounds.width();
}

float WebAXObjectProxy::BoundsInContainerHeight() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return bounds.height();
}

bool WebAXObjectProxy::HasNonIdentityTransform() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  gfx::Transform transform;
  accessibility_object_.GetRelativeBounds(container, bounds, transform);
  return !transform.IsIdentity();
}

RootWebAXObjectProxy::RootWebAXObjectProxy(const blink::WebAXObject& object,
                                           Factory* factory)
    : WebAXObjectProxy(object, factory) {}

v8::Local<v8::Object> RootWebAXObjectProxy::GetChildAtIndex(unsigned index) {
  if (index)
    return v8::Local<v8::Object>();

  return factory()->GetOrCreate(accessibility_object());
}

bool RootWebAXObjectProxy::IsRoot() const {
  return true;
}

WebAXObjectProxyList::WebAXObjectProxyList(v8::Isolate* isolate,
                                           blink::WebAXContext& ax_context)
    : isolate_(isolate), ax_context_(&ax_context) {}

WebAXObjectProxyList::~WebAXObjectProxyList() {
  Clear();
}

void WebAXObjectProxyList::Clear() {
  v8::HandleScope handle_scope(isolate_);

  for (auto& persistent : ax_objects_) {
    auto local = v8::Local<v8::Object>::New(isolate_, persistent.second);

    WebAXObjectProxy* proxy = nullptr;
    bool ok = gin::ConvertFromV8(isolate_, local, &proxy);
    DCHECK(ok);

    // Because the v8::Persistent in this container uses
    // CopyablePersistentObject traits, it will not leak the Persistent objects
    // on destruction. However, blink may be keeping a reference to the |proxy|.
    // We Reset() it to drop the callback in the proxy object now that its not
    // in the proxy list.
    proxy->Reset();
  }

  ax_objects_.clear();
}

v8::Local<v8::Object> WebAXObjectProxyList::GetOrCreate(
    const blink::WebAXObject& object) {
  if (object.IsNull() || object.IsDetached()) {
    return v8::Local<v8::Object>();
  }

  // Return existing object if there is a match.
  auto persistent = ax_objects_.find(object.AxID());
  if (persistent != ax_objects_.end()) {
    auto local = v8::Local<v8::Object>::New(isolate_, persistent->second);

#if DCHECK_IS_ON()
    WebAXObjectProxy* proxy = nullptr;
    bool ok = gin::ConvertFromV8(isolate_, local, &proxy);
    DCHECK(ok);
    DCHECK(proxy->IsEqualToObject(object));
#endif

    return local;
  }

  // Create a new object.
  v8::Local<v8::Value> value_handle =
      gin::CreateHandle(isolate_, new WebAXObjectProxy(object, this)).ToV8();
  v8::Local<v8::Object> handle;
  if (value_handle.IsEmpty() ||
      !value_handle->ToObject(isolate_->GetCurrentContext()).ToLocal(&handle)) {
    return {};
  }

  ax_objects_.emplace(object.AxID(), v8::Global<v8::Object>(isolate_, handle));
  return handle;
}

blink::WebAXContext* WebAXObjectProxyList::GetAXContext() {
  return ax_context_;
}

}  // namespace content
