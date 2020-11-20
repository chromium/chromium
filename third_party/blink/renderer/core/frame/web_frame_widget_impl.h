/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class Element;
class PaintLayerCompositor;
class WebFrameWidget;
class WebFrameWidgetImpl;

// Implements WebFrameWidget for a child local root frame (OOPIF). This object
// is created in the child renderer and attached to the OOPIF's WebLocalFrame.
//
// For the main frame's WebFrameWidget implementation, see WebViewFrameWidget.
//
class WebFrameWidgetImpl final : public WebFrameWidgetBase {
 public:
  WebFrameWidgetImpl(
      base::PassKey<WebFrameWidget>,
      WebWidgetClient&,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited);
  ~WebFrameWidgetImpl() override;

  // WebWidget functions:
  void Close(
      scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) override;

  // WebFrameWidget implementation.
  bool ScrollFocusedEditableElementIntoView() override;

  PaintLayerCompositor* Compositor() const;

 private:
  friend class WebFrameWidget;  // For WebFrameWidget::create.

  // PageWidgetEventHandler functions
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;

  // Finds the parameters required for scrolling the focused editable |element|
  // into view. |rect_to_scroll| is used for recursive scrolling of the element
  // into view and contains all or part of element's bounding box and always
  // includes the caret and is with respect to absolute coordinates.
  void GetScrollParamsForFocusedEditableElement(
      const Element& element,
      PhysicalRect& rect_to_scroll,
      mojom::blink::ScrollIntoViewParamsPtr& params);

  SelfKeepAlive<WebFrameWidgetImpl> self_keep_alive_;
};

}  // namespace blink

#endif
