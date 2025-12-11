/*
 * Copyright (C) 2006, 2007, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/page/chrome_client.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

void ChromeClient::Trace(Visitor* visitor) const {
  visitor->Trace(last_mouse_over_node_);
}

void ChromeClient::InstallSupplements(LocalFrame& frame) {
  CoreInitializer::GetInstance().InstallSupplements(frame);
}

bool ChromeClient::CanOpenUIElementIfDuringPageDismissal(
    Frame& main_frame,
    UIElementType ui_element_type,
    const String& message) {
  for (Frame* frame = &main_frame; frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    Document::PageDismissalType dismissal =
        local_frame->GetDocument()->PageDismissalEventBeingDispatched();
    if (dismissal != Document::kNoDismissal) {
      return ShouldOpenUIElementDuringPageDismissal(
          *local_frame, ui_element_type, message, dismissal);
    }
  }
  return true;
}

Page* ChromeClient::CreateWindow(
    LocalFrame* frame,
    const FrameLoadRequest& r,
    const AtomicString& frame_name,
    const WebWindowFeatures& features,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const SessionStorageNamespaceId& session_storage_namespace_id,
    bool& consumed_user_gesture) {
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kPopup, g_empty_string)) {
    return nullptr;
  }

  return CreateWindowDelegate(frame, r, frame_name, features, sandbox_flags,
                              session_storage_namespace_id,
                              consumed_user_gesture);
}

template <typename Delegate>
static bool OpenJavaScriptDialog(LocalFrame* frame,
                                 const String& message,
                                 const Delegate& delegate) {
  DOMWindowPerformance::performance(*frame->DomWindow())->WillShowModalDialog();
  // Suspend pages in case the client method runs a new event loop that would
  // otherwise cause the load to continue while we're in the middle of
  // executing JavaScript.
  ScopedPagePauser pauser;
  probe::WillRunJavaScriptDialog(frame);
  bool result = delegate();
  probe::DidRunJavaScriptDialog(frame);
  return result;
}

bool ChromeClient::OpenBeforeUnloadConfirmPanel(const String& message,
                                                LocalFrame* frame,
                                                bool is_reload) {
  DCHECK(frame);
  return OpenJavaScriptDialog(frame, message, [this, frame, is_reload]() {
    return OpenBeforeUnloadConfirmPanelDelegate(frame, is_reload);
  });
}

bool ChromeClient::OpenJavaScriptAlert(LocalFrame* frame,
                                       const String& message) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kAlertDialog, message)) {
    return false;
  }
  return OpenJavaScriptDialog(frame, message, [this, frame, &message]() {
    return OpenJavaScriptAlertDelegate(frame, message);
  });
}

bool ChromeClient::OpenJavaScriptConfirm(LocalFrame* frame,
                                         const String& message) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kConfirmDialog, message)) {
    return false;
  }
  return OpenJavaScriptDialog(frame, message, [this, frame, &message]() {
    return OpenJavaScriptConfirmDelegate(frame, message);
  });
}

bool ChromeClient::OpenJavaScriptPrompt(LocalFrame* frame,
                                        const String& prompt,
                                        const String& default_value,
                                        String& result) {
  DCHECK(frame);
  if (!CanOpenUIElementIfDuringPageDismissal(
          frame->Tree().Top(), UIElementType::kPromptDialog, prompt)) {
    return false;
  }
  return OpenJavaScriptDialog(
      frame, prompt, [this, frame, &prompt, &default_value, &result]() {
        return OpenJavaScriptPromptDelegate(frame, prompt, default_value,
                                            result);
      });
}

void ChromeClient::MouseDidMoveOverElement(LocalFrame& frame,
                                           const HitTestLocation& location,
                                           const HitTestResult& result) {
  if (!result.GetScrollbar() && result.InnerNode() &&
      result.InnerNode()->GetDocument().IsDNSPrefetchEnabled()) {
    WebPrescientNetworking* web_prescient_networking =
        frame.PrescientNetworking();
    if (web_prescient_networking) {
      web_prescient_networking->PrefetchDNS(result.AbsoluteLinkURL());
    }
  }

  ShowMouseOverURL(result);

  if (result.GetScrollbar())
    ClearToolTip(frame);
  else
    UpdateTooltipUnderCursor(frame, location, result);
}

void ChromeClient::UpdateTooltipUnderCursor(LocalFrame& frame,
                                            const HitTestLocation& location,
                                            const HitTestResult& result) {
  // First priority is a tooltip for element with "title" attribute.
  TextDirection tool_tip_direction;
  String tool_tip = result.Title(tool_tip_direction);

  // Lastly, some elements provide default tooltip strings.  e.g. <input
  // type="file" multiple> shows a tooltip for the selected filenames.
  if (tool_tip.IsNull()) {
    if (auto* element = DynamicTo<Element>(result.InnerNode())) {
      tool_tip = element->DefaultToolTip();

      // FIXME: We should obtain text direction of tooltip from
      // ChromeClient or platform. As of October 2011, all client
      // implementations don't use text direction information for
      // ChromeClient::UpdateTooltipUnderCursor. We'll work on tooltip text
      // direction during bidi cleanup in form inputs.
      tool_tip_direction = TextDirection::kLtr;
    }
  }

  if (last_tool_tip_point_ == location.Point() &&
      last_tool_tip_text_ == tool_tip)
    return;

  // If a tooltip was displayed earlier, and mouse cursor moves over
  // a different node with the same tooltip text, make sure the previous
  // tooltip is unset, so that it does not get stuck positioned relative
  // to the previous node).
  // The ::UpdateTooltipUnderCursor overload, which is be called down the road,
  // ensures a new tooltip to be displayed with the new context.
  if (result.InnerNodeOrImageMapImage() != last_mouse_over_node_ &&
      !last_tool_tip_text_.empty() && tool_tip == last_tool_tip_text_)
    ClearToolTip(frame);

  last_tool_tip_point_ = location.Point();
  last_tool_tip_text_ = tool_tip;
  last_mouse_over_node_ = result.InnerNodeOrImageMapImage();
  current_tool_tip_text_for_test_ = last_tool_tip_text_;
  UpdateTooltipUnderCursor(frame, tool_tip, tool_tip_direction);
}

void ChromeClient::ElementFocusedFromKeypress(LocalFrame& frame,
                                              const Element* element) {
  String tooltip_text = element->title();
  if (tooltip_text.IsNull())
    tooltip_text = element->DefaultToolTip();

  LayoutObject* layout_object = element->GetLayoutObject();
  if (layout_object) {
    TextDirection tooltip_direction = layout_object->StyleRef().Direction();
    UpdateTooltipFromKeyboard(frame, tooltip_text, tooltip_direction,
                              element->BoundsInWidget());
  }
}

void ChromeClient::ClearToolTip(LocalFrame& frame) {
  current_tool_tip_text_for_test_ = String();
  // Do not check last_tool_tip_* and do not update them intentionally.
  // We don't want to show tooltips with same content after clearToolTip().
  UpdateTooltipUnderCursor(frame, String(), TextDirection::kLtr);
}

bool ChromeClient::Print(LocalFrame* frame) {
  if (!CanOpenUIElementIfDuringPageDismissal(*frame->GetPage()->MainFrame(),
                                             UIElementType::kPrintDialog,
                                             g_empty_string)) {
    return false;
  }

  if (frame->DomWindow()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kModals)) {
    UseCounter::Count(frame->DomWindow(),
                      WebFeature::kDialogInSandboxedContext);
    frame->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        frame->IsInFencedFrameTree()
            ? "Ignored call to 'print()'. The document is in a fenced frame "
              "tree."
            : "Ignored call to 'print()'. The document is sandboxed, and the "
              "'allow-modals' keyword is not set."));
    return false;
  }

  // print() returns quietly during prerendering.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-modals
  if (frame->GetDocument()->IsPrerendering()) {
    frame->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError,
        "Ignored call to 'print()' during prerendering."));
    return false;
  }

  DOMWindowPerformance::performance(*frame->DomWindow())->WillShowModalDialog();

  // Suspend pages in case the client method runs a new event loop that would
  // otherwise cause the load to continue while we're in the middle of
  // executing JavaScript.
  // TODO(crbug.com/956832): Remove this when it is safe to do so.
  ScopedPagePauser pauser;

  PrintDelegate(frame);
  return true;
}

}  // namespace blink
