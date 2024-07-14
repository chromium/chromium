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

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

float GetDprForSizeAdjustment(const Element& owner_element) {
  float dpr = 1.0f;
  // Android doesn't need these adjustments and it makes tests fail.
#ifndef OS_ANDROID
  LocalFrame* frame = owner_element.GetDocument().GetFrame();
  const Page* page = frame ? frame->GetPage() : nullptr;
  dpr = page->GetChromeClient().GetScreenInfo(*frame).device_scale_factor;
#endif
  return dpr;
}

}  // namespace

ExternalPopupMenu::ExternalPopupMenu(LocalFrame& frame,
                                     HTMLSelectElement& owner_element)
    : owner_element_(owner_element),
      local_frame_(frame),
      dispatch_event_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                            this,
                            &ExternalPopupMenu::DispatchEvent),
      receiver_(this, owner_element.GetExecutionContext()) {}

ExternalPopupMenu::~ExternalPopupMenu() = default;

void ExternalPopupMenu::Trace(Visitor* visitor) const {
  visitor->Trace(owner_element_);
  visitor->Trace(local_frame_);
  visitor->Trace(dispatch_event_timer_);
  visitor->Trace(receiver_);
  PopupMenu::Trace(visitor);
}

void ExternalPopupMenu::Reset() {
  receiver_.reset();
}

bool ExternalPopupMenu::ShowInternal() {
  // Blink core reuses the PopupMenu of an element.  For simplicity, we do
  // recreate the actual external popup every time.
  Reset();

  int32_t item_height;
  double font_size;
  int32_t selected_item;
  Vector<mojom::blink::MenuItemPtr> menu_items;
  bool right_aligned;
  bool allow_multiple_selection;
  GetPopupMenuInfo(*owner_element_, &item_height, &font_size, &selected_item,
                   &menu_items, &right_aligned, &allow_multiple_selection);
  if (menu_items.empty())
    return false;

  auto* execution_context = owner_element_->GetExecutionContext();
  if (!receiver_.is_bound()) {
    LayoutObject* layout_object = owner_element_->GetLayoutObject();
    if (!layout_object || !layout_object->IsBox())
      return false;
    auto* box = To<LayoutBox>(layout_object);
    gfx::Rect rect =
        ToEnclosingRect(box->LocalToAbsoluteRect(box->PhysicalBorderBoxRect()));
    gfx::Rect rect_in_viewport = local_frame_->View()->FrameToViewport(rect);
    float scale_for_emulation = WebLocalFrameImpl::FromFrame(local_frame_)
                                    ->LocalRootFrameWidget()
                                    ->GetEmulatorScale();

    // rect_in_viewport needs to be in CSS pixels.
    float dpr = GetDprForSizeAdjustment(*owner_element_);
    if (dpr != 1.0) {
      rect_in_viewport = gfx::ScaleToRoundedRect(rect_in_viewport, 1 / dpr);
    }

    gfx::Rect bounds =
        gfx::Rect(rect_in_viewport.x() * scale_for_emulation,
                  rect_in_viewport.y() * scale_for_emulation,
                  rect_in_viewport.width(), rect_in_viewport.height());
    local_frame_->GetLocalFrameHostRemote().ShowPopupMenu(
        receiver_.BindNewPipeAndPassRemote(execution_context->GetTaskRunner(
            TaskType::kInternalUserInteraction)),
        bounds, item_height, font_size, selected_item, std::move(menu_items),
        right_aligned, allow_multiple_selection);
    return true;
  }

  // The client might refuse to create a popup (when there is already one
  // pending to be shown for example).
  DidCancel();
  return false;
}

void ExternalPopupMenu::Show(PopupMenu::ShowEventType) {
  if (!ShowInternal())
    return;
#if BUILDFLAG(IS_MAC)
  const WebInputEvent* current_event = CurrentInputEvent::Get();
  if (current_event &&
      current_event->GetType() == WebInputEvent::Type::kMouseDown) {
    synthetic_event_ = std::make_unique<WebMouseEvent>();
    *synthetic_event_ = *static_cast<const WebMouseEvent*>(current_event);
    synthetic_event_->SetType(WebInputEvent::Type::kMouseUp);
    dispatch_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
    // FIXME: show() is asynchronous. If preparing a popup is slow and a
    // user released the mouse button before showing the popup, mouseup and
    // click events are correctly dispatched. Dispatching the synthetic
    // mouseup event is redundant in this case.
  }
#endif
}

void ExternalPopupMenu::DispatchEvent(TimerBase*) {
  static_cast<WebWidget*>(
      WebLocalFrameImpl::FromFrame(local_frame_->LocalFrameRoot())
          ->FrameWidgetImpl())
      ->HandleInputEvent(
          blink::WebCoalescedInputEvent(*synthetic_event_, ui::LatencyInfo()));
  synthetic_event_.reset();
}

void ExternalPopupMenu::Hide() {
  if (owner_element_)
    owner_element_->PopupDidHide();
  Reset();
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
          ->PostTask(FROM_HERE, WTF::BindOnce(&ExternalPopupMenu::Update,
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
  if (!receiver_.is_bound() || !owner_element_)
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

void ExternalPopupMenu::DidAcceptIndices(const Vector<int32_t>& indices) {
  local_frame_->NotifyUserActivation(
      mojom::blink::UserActivationNotificationType::kInteraction);

  // Calling methods on the HTMLSelectElement might lead to this object being
  // derefed. This ensures it does not get deleted while we are running this
  // method.
  if (!owner_element_) {
    Reset();
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
    wtf_size_t list_count = base::checked_cast<wtf_size_t>(indices.size());
    list_indices.reserve(list_count);
    for (wtf_size_t i = 0; i < list_count; ++i)
      list_indices.push_back(ToPopupMenuItemIndex(indices[i], *owner_element));
    owner_element->SelectMultipleOptionsByPopup(list_indices);
  }
  Reset();
}

void ExternalPopupMenu::DidCancel() {
  if (owner_element_)
    owner_element_->PopupDidHide();
  Reset();
}

void ExternalPopupMenu::GetPopupMenuInfo(
    HTMLSelectElement& owner_element,
    int32_t* item_height,
    double* font_size,
    int32_t* selected_item,
    Vector<mojom::blink::MenuItemPtr>* menu_items,
    bool* right_aligned,
    bool* allow_multiple_selection) {
  const HeapVector<Member<HTMLElement>>& list_items =
      owner_element.GetListItems();
  wtf_size_t item_count = list_items.size();
  for (wtf_size_t i = 0; i < item_count; ++i) {
    if (owner_element.ItemIsDisplayNone(*list_items[i]))
      continue;

    Element& item_element = *list_items[i];
#if BUILDFLAG(IS_ANDROID)
    // Separators get rendered as selectable options on android
    if (IsA<HTMLHRElement>(item_element)) {
      continue;
    }
#endif
    auto popup_item = mojom::blink::MenuItem::New();
    popup_item->label = owner_element.ItemText(item_element);
    popup_item->tool_tip = item_element.title();
    popup_item->checked = false;
    if (IsA<HTMLHRElement>(item_element)) {
      popup_item->type = mojom::blink::MenuItem::Type::kSeparator;
    } else if (IsA<HTMLOptGroupElement>(item_element)) {
      popup_item->type = mojom::blink::MenuItem::Type::kGroup;
    } else {
      popup_item->type = mojom::blink::MenuItem::Type::kOption;
      popup_item->checked = To<HTMLOptionElement>(item_element).Selected();
    }
    popup_item->enabled = !item_element.IsDisabledFormControl();
    const ComputedStyle& style = *owner_element.ItemComputedStyle(item_element);
    popup_item->text_direction = ToBaseTextDirection(style.Direction());
    popup_item->has_text_direction_override =
        IsOverride(style.GetUnicodeBidi());
    menu_items->push_back(std::move(popup_item));
  }

  const ComputedStyle& menu_style = owner_element.GetComputedStyle()
                                        ? *owner_element.GetComputedStyle()
                                        : *owner_element.EnsureComputedStyle();
  const SimpleFontData* font_data = menu_style.GetFont().PrimaryFont();
  DCHECK(font_data);
  // These coordinates need to be in CSS pixels.
  float dpr = GetDprForSizeAdjustment(owner_element);
  *item_height = font_data ? font_data->GetFontMetrics().Height() / dpr : 0;
  *font_size = static_cast<int>(
      menu_style.GetFont().GetFontDescription().ComputedSize() / dpr);
  *selected_item = ToExternalPopupMenuItemIndex(
      owner_element.SelectedListIndex(), owner_element);

  *right_aligned = menu_style.Direction() == TextDirection::kRtl;

  *allow_multiple_selection = owner_element.IsMultiple();
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
#if BUILDFLAG(IS_ANDROID)
    // <hr> elements are not sent to the browser on android
    if (IsA<HTMLHRElement>(*items[i])) {
      continue;
    }
#endif
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
#if BUILDFLAG(IS_ANDROID)
    // <hr> elements are not sent to the browser on android
    if (IsA<HTMLHRElement>(*items[i])) {
      continue;
    }
#endif
    if (popup_menu_item_index == static_cast<int>(i))
      return index_tracker;
    ++index_tracker;
  }
  return -1;
}

}  // namespace blink
