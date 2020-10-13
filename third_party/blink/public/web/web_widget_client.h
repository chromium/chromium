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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-forward.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_navigation_policy.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
struct ImeTextSpan;
}

namespace blink {
class WebDragData;
class WebMouseEvent;
class WebWidget;
class WebLocalFrame;
class WebString;

class WebWidgetClient {
 public:
  virtual ~WebWidgetClient() = default;

  // Called to request a BeginMainFrame from the compositor. For tests with
  // single thread and no scheduler, the impl should schedule a task to run
  // a synchronous composite.
  virtual void ScheduleAnimation() {}

  // Called to request a BeginMainFrame from the compositor, meant to be used
  // for web tests only, where commits must be explicitly scheduled. Contrary to
  // ScheduleAnimation() this will be a no-op on multi-threaded environments and
  // will unconditionally ensure that the compositor is actually run.
  virtual void ScheduleAnimationForWebTests() {}

  // Called when some JS code has instructed the window associated to the main
  // frame to close, which will result in a request to the browser to close the
  // RenderWidget associated to it
  virtual void CloseWidgetSoon() {}

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const ui::Cursor&) {}

  // Called to show the widget according to the given policy.
  virtual void Show(WebNavigationPolicy) {}

  // Called to set the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  virtual void SetWindowRect(const gfx::Rect&) {}

  // Allocates a LayerTreeFrameSink to submit CompositorFrames to. Only
  // override this method if you wish to provide your own implementation
  // of LayerTreeFrameSinks (usually for tests). If this method returns null
  // a frame sink will be requested from the browser process (ie. default flow).
  virtual std::unique_ptr<cc::LayerTreeFrameSink>
  AllocateNewLayerTreeFrameSink() {
    return nullptr;
  }

  // Requests to lock the mouse cursor for the |requester_frame| in the
  // widget. If true is returned, the success result will be asynchronously
  // returned via a single call to WebWidget::didAcquirePointerLock() or
  // WebWidget::didNotAcquirePointerLock() and a single call to the callback.
  // If false, the request has been denied synchronously.
  using PointerLockCallback =
      base::OnceCallback<void(mojom::PointerLockResult)>;
  virtual bool RequestPointerLock(WebLocalFrame* requester_frame,
                                  PointerLockCallback callback,
                                  bool request_unadjusted_movement) {
    return false;
  }

  virtual bool RequestPointerLockChange(WebLocalFrame* requester_frame,
                                        PointerLockCallback callback,
                                        bool request_unadjusted_movement) {
    return false;
  }

  // Cause the pointer lock to be released. This may be called at any time,
  // including when a lock is pending but not yet acquired.
  // WebWidget::didLosePointerLock() is called when unlock is complete.
  virtual void RequestPointerUnlock() {}

  // Returns true iff the pointer is locked to this widget.
  virtual bool IsPointerLocked() { return false; }

  // Called when a drag-and-drop operation should begin. Returns whether the
  // call has been handled.
  virtual bool InterceptStartDragging(const WebDragData&,
                                      DragOperationsMask,
                                      const SkBitmap& drag_image,
                                      const gfx::Point& drag_image_offset) {
    return false;
  }

  virtual viz::FrameSinkId GetFrameSinkId() {
    NOTREACHED();
    return viz::FrameSinkId();
  }

  // For more information on the sequence of when these callbacks are made
  // consult cc/trees/layer_tree_host_client.h.

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work.
  virtual void WillBeginMainFrame() {}

  // Notification that the output of a BeginMainFrame was committed to the
  // compositor (thread), though would not be submitted to the display
  // compositor yet (see DidCommitAndDrawCompositorFrame()).
  virtual void DidCommitCompositorFrame(base::TimeTicks commit_start_time) {}

  // Notifies that the layer tree host has completed a call to
  // RequestMainFrameUpdate in response to a BeginMainFrame.
  virtual void DidBeginMainFrame() {}

  // Record the time it took for the first paint after the widget transitioned
  // from background inactive to active.
  virtual void RecordTimeToFirstActivePaint(base::TimeDelta duration) {}

  // Called before mouse events are processed and allows the
  // client to handle the event itself. Return true if event was handled
  // and further processing should stop.
  virtual bool WillHandleMouseEvent(const WebMouseEvent& event) {
    return false;
  }

  // Determines whether composition can happen inline.
  virtual bool CanComposeInline() { return false; }

  // Determines if IME events should be sent to Pepper instead of processed to
  // the currently focused frame.
  virtual bool ShouldDispatchImeEventsToPepper() { return false; }

  // Returns the current pepper text input type.
  virtual WebTextInputType GetPepperTextInputType() {
    return WebTextInputType::kWebTextInputTypeNone;
  }

  // Returns the current pepper caret bounds in blink/viewport coordinates.
  virtual gfx::Rect GetPepperCaretBounds() { return gfx::Rect(); }

  // Set the composition in pepper.
  virtual void ImeSetCompositionForPepper(
      const WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end) {}

  // Commit the text to pepper.
  virtual void ImeCommitTextForPepper(
      const WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int relative_cursor_pos) {}

  // Indicate composition is complete to pepper.
  virtual void ImeFinishComposingTextForPepper(bool keep_selection) {}

  // Called to indicate a syntehtic event was queued.
  virtual void WillQueueSyntheticEvent(const WebCoalescedInputEvent& event) {}

  // Whether compositing to LCD text should be auto determined. This can be
  // overridden by tests to disable this.
  virtual bool ShouldAutoDetermineCompositingToLCDTextSetting() { return true; }
};

}  // namespace blink

#endif
