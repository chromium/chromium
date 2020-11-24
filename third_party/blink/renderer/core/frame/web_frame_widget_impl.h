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
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
class WebFrameWidget;
class WebFrameWidgetImpl;

// Implements WebFrameWidget for both main frames and child local root frame
// (OOPIF). The WebFrameWidgetBase class will eventually be combined with this
// class.
//
class CORE_EXPORT WebFrameWidgetImpl : public WebFrameWidgetBase {
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
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame);
  ~WebFrameWidgetImpl() override;

 private:
  friend class WebFrameWidget;  // For WebFrameWidget::create.
};

// Convenience type for creation method taken by
// InstallCreateMainFrameWebFrameWidgetHook(). The method signature matches the
// WebFrameWidgetImpl constructor.
using CreateMainFrameWebFrameWidgetFunction =
    WebFrameWidgetImpl* (*)(base::PassKey<WebFrameWidget>,
                            WebWidgetClient&,
                            CrossVariantMojoAssociatedRemote<
                                mojom::blink::FrameWidgetHostInterfaceBase>
                                frame_widget_host,
                            CrossVariantMojoAssociatedReceiver<
                                mojom::blink::FrameWidgetInterfaceBase>
                                frame_widget,
                            CrossVariantMojoAssociatedRemote<
                                mojom::blink::WidgetHostInterfaceBase>
                                widget_host,
                            CrossVariantMojoAssociatedReceiver<
                                mojom::blink::WidgetInterfaceBase> widget,
                            scoped_refptr<base::SingleThreadTaskRunner>
                                task_runner,
                            const viz::FrameSinkId& frame_sink_id,
                            bool hidden,
                            bool never_composited,
                            bool is_for_child_local_root,
                            bool is_for_nested_main_frame);
// Overrides the implementation of WebFrameWidget::CreateForMainFrame() function
// below. Used by tests to override some functionality on WebFrameWidgetImpl.
void CORE_EXPORT InstallCreateMainFrameWebFrameWidgetHook(
    CreateMainFrameWebFrameWidgetFunction create_widget);

}  // namespace blink

#endif
