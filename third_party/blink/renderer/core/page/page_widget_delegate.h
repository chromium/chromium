/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_WIDGET_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_WIDGET_DELEGATE_H_

#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class LocalFrame;
class Page;
class WebGestureEvent;
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebPointerEvent;

class CORE_EXPORT PageWidgetEventHandler {
 public:
  virtual void HandleMouseMove(LocalFrame& main_frame,
                               const WebMouseEvent&,
                               const WebVector<const WebInputEvent*>&,
                               const WebVector<const WebInputEvent*>&);
  virtual void HandleMouseLeave(LocalFrame& main_frame, const WebMouseEvent&);
  virtual void HandleMouseDown(LocalFrame& main_frame, const WebMouseEvent&);
  virtual WebInputEventResult HandleMouseUp(LocalFrame& main_frame,
                                            const WebMouseEvent&);
  virtual WebInputEventResult HandleMouseWheel(LocalFrame& main_frame,
                                               const WebMouseWheelEvent&);
  virtual WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) = 0;
  virtual WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) = 0;
  virtual WebInputEventResult HandleGestureEvent(const WebGestureEvent&) = 0;
  virtual WebInputEventResult HandlePointerEvent(
      LocalFrame& main_frame,
      const WebPointerEvent&,
      const WebVector<const WebInputEvent*>&,
      const WebVector<const WebInputEvent*>&);
  virtual ~PageWidgetEventHandler() {}
};

// Common implementation of WebViewImpl and WebPagePopupImpl.
class CORE_EXPORT PageWidgetDelegate {
  STATIC_ONLY(PageWidgetDelegate);

 public:
  static void Animate(Page&, base::TimeTicks monotonic_frame_begin_time);
  static void PostAnimate(Page&);

  // For the following methods, the |root| argument indicates a root LocalFrame
  // from which to start performing the specified operation.

  // See comment of WebWidget::UpdateLifecycle.
  static void UpdateLifecycle(Page&,
                              LocalFrame& root,
                              WebWidget::LifecycleUpdate requested_update,
                              WebWidget::LifecycleUpdateReason reason);

  // See comment of WebWidget::DidBeginFrame.
  static void DidBeginFrame(LocalFrame& root);

  // See FIXME in the function body about nullptr |root|.
  static WebInputEventResult HandleInputEvent(
      PageWidgetEventHandler&,
      const WebCoalescedInputEvent& coalesced_event,
      LocalFrame* root);
};

}  // namespace blink
#endif
