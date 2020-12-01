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

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
}

namespace blink {
class WebDragData;

class WebWidgetClient {
 public:
  virtual ~WebWidgetClient() = default;

  // Called to request a BeginMainFrame from the compositor. For tests with
  // single thread and no scheduler, the impl should schedule a task to run
  // a synchronous composite.
  virtual void ScheduleAnimation() {}

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const ui::Cursor&) {}

  // Allocates a LayerTreeFrameSink to submit CompositorFrames to. Only
  // override this method if you wish to provide your own implementation
  // of LayerTreeFrameSinks (usually for tests). If this method returns null
  // a frame sink will be requested from the browser process (ie. default flow).
  virtual std::unique_ptr<cc::LayerTreeFrameSink>
  AllocateNewLayerTreeFrameSink() {
    return nullptr;
  }

  // Called when a drag-and-drop operation should begin. Returns whether the
  // call has been handled.
  virtual bool InterceptStartDragging(const WebDragData&,
                                      DragOperationsMask,
                                      const SkBitmap& drag_image,
                                      const gfx::Point& drag_image_offset) {
    return false;
  }

  // For more information on the sequence of when these callbacks are made
  // consult cc/trees/layer_tree_host_client.h.

  // Notifies that the layer tree host has completed a call to
  // RequestMainFrameUpdate in response to a BeginMainFrame.
  virtual void DidBeginMainFrame() {}

  // Called to indicate a syntehtic event was queued.
  virtual void WillQueueSyntheticEvent(const WebCoalescedInputEvent& event) {}

  // Whether compositing to LCD text should be auto determined. This can be
  // overridden by tests to disable this.
  virtual bool ShouldAutoDetermineCompositingToLCDTextSetting() { return true; }
};

}  // namespace blink

#endif
