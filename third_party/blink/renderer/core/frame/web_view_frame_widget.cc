// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "base/debug/crash_logging.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(
    base::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    WebViewImpl& web_view,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool hidden,
    bool never_composited)
    : WebFrameWidgetBase(client,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget),
                         task_runner,
                         frame_sink_id,
                         hidden,
                         never_composited,
                         /*is_for_child_local_root=*/false,
                         is_for_nested_main_frame) {
  web_view.SetMainFrameViewWidget(this);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

}  // namespace blink
