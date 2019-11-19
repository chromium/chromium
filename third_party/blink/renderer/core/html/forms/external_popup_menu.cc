/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/external_popup_menu.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_external_popup_menu.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_menu_item_info.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#if defined(OS_MACOSX)
#include "third_party/blink/renderer/core/page/chrome_client.h"
#endif

namespace blink {

ExternalPopupMenu::ExternalPopupMenu(LocalFrame& frame,
                                     HTMLSelectElement& owner_element)
    : owner_element_(owner_element),
      local_frame_(frame),
      dispatch_event_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                            this,
                            &ExternalPopupMenu::DispatchEvent),
      web_external_popup_menu_(nullptr) {}

ExternalPopupMenu::~ExternalPopupMenu() = default;

void ExternalPopupMenu::Trace(Visitor* visitor) {
  visitor->Trace(owner_element_);
  visitor->Trace(local_frame_);
  PopupMenu::Trace(visitor);
}

bool ExternalPopupMenu::ShowInternal() {
  // Blink core reuses the PopupMenu of an element.  For simplicity, we do
  // recreate the actual external popup everytime.
  if (web_external_popup_menu_) {
    web_external_popup_menu_->Close();
    web_external_popup_menu_ = nullptr;
  }

  WebPopupMenuInfo info;
  GetPopupMenuInfo(info, *owner_element_);
  if (info.items.empty())
    return false;
  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(local_frame_.Get());
  web_external_popup_menu_ =
      webframe->Client()->CreateExternalPopupMenu(info, this);
  if (web_external_popup_menu_) {
    LayoutObject* layout_object = owner_element_->GetLayoutObject();
    if (!layout_object || !layout_object->IsBox())
      return false;
    IntRect rect = EnclosingIntRect(
        ToLayoutBox(layout_object)
            ->LocalToAbsoluteRect(
                ToLayoutBox(layout_object)->PhysicalBorderBoxRect()));
    IntRect rect_in_viewport = local_frame_->View()->FrameToViewport(rect);
    web_external_popup_menu_->Show(rect_in_viewport);
    return true;
  }

  // The client might refuse to create a popup (when there is already one
  // pending to be shown for example).
  DidCancel();
  return false;
}

void ExternalPopupMenu::Show() {
  if (!ShowInternal())
    return;
#if defined(OS_MACOSX)
  const WebInputEvent* current_event = CurrentInputEvent::Get();
  if (current_event && current_event->GetType() == WebInputEvent::kMouseDown) {
    synthetic_event_ = std::make_unique<WebMouseEvent>();
    *synthetic_event_ = *static_cast<const WebMouseEvent*>(current_event);
    synthetic_event_->SetType(WebInputEvent::kMouseUp);
    dispatch_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
    // FIXME: show() is asynchronous. If preparing a popup is slow and a
    // user released the mouse button before showing the popup, mouseup and
    // click events are correctly dispatched. Dispatching the synthetic
    // mouseup event is redundant in this case.
  }
#endif
}

void ExternalPopupMenu::DispatchEvent(TimerBase*) {
  WebLocalFrameImpl::FromFrame(local_frame_->LocalFrameRoot())
      ->FrameWidgetImpl()
      ->HandleInputEvent(blink::WebCoalescedInputEvent(*synthetic_event_));
  synthetic_event_.reset();
}

void ExternalPopupMenu::Hide() {
  if (owner_element_)
    owner_element_->PopupDidHide();
  if (!web_external_popup_menu_)
    return;
  web_external_popup_menu_->Close();
  web_external_popup_menu_ = nullptr;
}

void ExternalPopupMenu::UpdateFromElement(UpdateReason reason) {
  switch (reason) {
    case kBySelectionChange:
    case kByDOMChange:
      if (needs_update_)
        return;
      needs_update_ = true;
      owner_element_->GetDocument()
          .GetTaskRunner(TaskType::kUserInteraction)
          ->PostTask(FROM_HERE, WTF::Bind(&ExternalPopupMenu::Update,
                                          WrapPersistent(this)));
      break;

    case kByStyleChange:
      // TODO(tkent): We should update the popup location/content in some
      // cases.  e.g. Updating ComputedStyle of the SELECT element affects
      // popup position and OPTION style.
      break;
  }
}

void ExternalPopupMenu::Update() {
  if (!web_external_popup_menu_ || !owner_element_)
    return;
  owner_element_->GetDocument().UpdateStyleAndLayoutTree();
  // disconnectClient() might have been called.
  if (!owner_element_)
    return;
  needs_update_ = false;

  if (ShowInternal())
    return;
  // We failed to show a popup.  Notify it to the owner.
  Hide();
}

void ExternalPopupMenu::DisconnectClient() {
  Hide();
  owner_element_ = nullptr;
  dispatch_event_timer_.Stop();
}

void ExternalPopupMenu::DidChangeSelection(int index) {}

void ExternalPopupMenu::DidAcceptIndex(int index) {
  // Calling methods on the HTMLSelectElement might lead to this object being
  // derefed. This ensures it does not get deleted while we are running this
  // method.
  int popup_menu_item_index = ToPopupMenuItemIndex(index, *owner_element_);

  if (owner_element_) {
    owner_element_->PopupDidHide();
    owner_element_->SelectOptionByPopup(popup_menu_item_index);
  }
  web_external_popup_menu_ = nullptr;
}

// Android uses this function even for single SELECT.
void ExternalPopupMenu::DidAcceptIndices(const WebVector<int>& indices) {
  if (!owner_element_) {
    web_external_popup_menu_ = nullptr;
    return;
  }

  HTMLSelectElement* owner_element = owner_element_;
  owner_element->PopupDidHide();

  if (indices.empty()) {
    owner_element->SelectOptionByPopup(-1);
  } else if (!owner_element->IsMultiple()) {
    owner_element->SelectOptionByPopup(
        ToPopupMenuItemIndex(indices[indices.size() - 1], *owner_element));
  } else {
    Vector<int> list_indices;
    wtf_size_t list_count = SafeCast<wtf_size_t>(indices.size());
    list_indices.ReserveCapacity(list_count);
    for (wtf_size_t i = 0; i < list_count; ++i)
      list_indices.push_back(ToPopupMenuItemIndex(indices[i], *owner_element));
    owner_element->SelectMultipleOptionsByPopup(list_indices);
  }

  web_external_popup_menu_ = nullptr;
}

void ExternalPopupMenu::DidCancel() {
  if (owner_element_)
    owner_element_->PopupDidHide();
  web_external_popup_menu_ = nullptr;
}

void ExternalPopupMenu::GetPopupMenuInfo(WebPopupMenuInfo& info,
                                         HTMLSelectElement& owner_element) {
  const HeapVector<Member<HTMLElement>>& list_items =
      owner_element.GetListItems();
  wtf_size_t item_count = list_items.size();
  wtf_size_t count = 0;
  Vector<WebMenuItemInfo> items(item_count);
  for (wtf_size_t i = 0; i < item_count; ++i) {
    if (owner_element.ItemIsDisplayNone(*list_items[i]))
      continue;

    Element& item_element = *list_items[i];
    WebMenuItemInfo& popup_item = items[count++];
    popup_item.label = owner_element.ItemText(item_element);
    popup_item.tool_tip = item_element.title();
    popup_item.checked = false;
    if (IsA<HTMLHRElement>(item_element)) {
      popup_item.type = WebMenuItemInfo::kSeparator;
    } else if (IsA<HTMLOptGroupElement>(item_element)) {
      popup_item.type = WebMenuItemInfo::kGroup;
    } else {
      popup_item.type = WebMenuItemInfo::kOption;
      popup_item.checked = To<HTMLOptionElement>(item_element).Selected();
    }
    popup_item.enabled = !item_element.IsDisabledFormControl();
    const ComputedStyle& style = *owner_element.ItemComputedStyle(item_element);
    popup_item.text_direction = ToWebTextDirection(style.Direction());
    popup_item.has_text_direction_override = IsOverride(style.GetUnicodeBidi());
  }

  const ComputedStyle& menu_style = owner_element.GetComputedStyle()
                                        ? *owner_element.GetComputedStyle()
                                        : *owner_element.EnsureComputedStyle();
  const SimpleFontData* font_data = menu_style.GetFont().PrimaryFont();
  DCHECK(font_data);
  info.item_height = font_data ? font_data->GetFontMetrics().Height() : 0;
  info.item_font_size = static_cast<int>(
      menu_style.GetFont().GetFontDescription().ComputedSize());
  info.selected_index = ToExternalPopupMenuItemIndex(
      owner_element.SelectedListIndex(), owner_element);
  info.right_aligned = menu_style.Direction() == TextDirection::kRtl;
  info.allow_multiple_selection = owner_element.IsMultiple();
  if (count < item_count)
    items.Shrink(count);
  info.items = items;
}

int ExternalPopupMenu::ToPopupMenuItemIndex(int external_popup_menu_item_index,
                                            HTMLSelectElement& owner_element) {
  if (external_popup_menu_item_index < 0)
    return external_popup_menu_item_index;

  int index_tracker = 0;
  const HeapVector<Member<HTMLElement>>& items = owner_element.GetListItems();
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    if (owner_element.ItemIsDisplayNone(*items[i]))
      continue;
    if (index_tracker++ == external_popup_menu_item_index)
      return i;
  }
  return -1;
}

int ExternalPopupMenu::ToExternalPopupMenuItemIndex(
    int popup_menu_item_index,
    HTMLSelectElement& owner_element) {
  if (popup_menu_item_index < 0)
    return popup_menu_item_index;

  int index_tracker = 0;
  const HeapVector<Member<HTMLElement>>& items = owner_element.GetListItems();
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    if (owner_element.ItemIsDisplayNone(*items[i]))
      continue;
    if (popup_menu_item_index == static_cast<int>(i))
      return index_tracker;
    ++index_tracker;
  }
  return -1;
}

}  // namespace blink
