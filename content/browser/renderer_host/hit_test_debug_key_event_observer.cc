// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/hit_test_debug_key_event_observer.h"

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#endif

typedef blink::WebInputEvent::Type Type;
typedef blink::WebInputEvent::Modifiers Modifiers;

namespace content {

HitTestDebugKeyEventObserver::HitTestDebugKeyEventObserver(
    RenderWidgetHostImpl* host)
    : host_(host), hit_test_query_(nullptr) {
  host_->AddInputEventObserver(this);
}

HitTestDebugKeyEventObserver::~HitTestDebugKeyEventObserver() {
  host_->RemoveInputEventObserver(this);
}

void HitTestDebugKeyEventObserver::OnInputEventAck(
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
    const blink::WebInputEvent& event) {
  if (blink::mojom::InputEventResultState::kConsumed == state ||
      (event.GetType() != Type::kRawKeyDown &&
       event.GetType() != Type::kKeyDown)) {
    return;
  }

  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);

  // All keybindings use Ctrl-Shift modifiers.
  if (key_event.GetModifiers() !=
      (Modifiers::kControlKey | Modifiers::kShiftKey)) {
    return;
  }

  int key = key_event.windows_key_code;

  if (key == ui::VKEY_H) {
    if (!hit_test_query_) {
      hit_test_query_ = GetHitTestQuery(GetHostFrameSinkManager(),
                                        host_->GetView()->GetRootFrameSinkId());
    }
    if (hit_test_query_) {
      std::string printed_hit_test_data = hit_test_query_->PrintHitTestData();
      VLOG(1) << (printed_hit_test_data.empty() ? "No hit-test data."
                                                : printed_hit_test_data);
    }
  } else if (key == ui::VKEY_K) {
#if defined(USE_AURA)
    ui::Compositor* compositor =
        host_->GetView()->GetNativeView()->GetHost()->compositor();
    cc::LayerTreeDebugState debug_state = compositor->GetLayerTreeDebugState();
    debug_state.show_hit_test_borders = !debug_state.show_hit_test_borders;
    compositor->SetLayerTreeDebugState(debug_state);
#endif
    // TODO(zandershah): Implement for mac and android through
    // RenderWidgetHostViewBase interface.
  }
}

}  // namespace content
