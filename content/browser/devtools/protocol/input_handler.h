// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include <memory>
#include <set>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/input.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {
class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;

namespace protocol {

class InputHandler : public DevToolsDomainHandler, public Input::Backend {
 public:
  InputHandler();
  ~InputHandler() override;

  static std::vector<InputHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  void OnPageScaleFactorChanged(float page_scale_factor);
  Response Disable() override;

  void DispatchKeyEvent(
      const std::string& type,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> text,
      Maybe<std::string> unmodified_text,
      Maybe<std::string> key_identifier,
      Maybe<std::string> code,
      Maybe<std::string> key,
      Maybe<int> windows_virtual_key_code,
      Maybe<int> native_virtual_key_code,
      Maybe<bool> auto_repeat,
      Maybe<bool> is_keypad,
      Maybe<bool> is_system_key,
      Maybe<int> location,
      std::unique_ptr<DispatchKeyEventCallback> callback) override;

  void InsertText(const std::string& text,
                  std::unique_ptr<InsertTextCallback> callback) override;

  void DispatchMouseEvent(
      const std::string& type,
      double x,
      double y,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> button,
      Maybe<int> buttons,
      Maybe<int> click_count,
      Maybe<double> delta_x,
      Maybe<double> delta_y,
      Maybe<std::string> pointer_type,
      std::unique_ptr<DispatchMouseEventCallback> callback) override;

  void DispatchTouchEvent(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback) override;

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      const std::string& button,
                                      Maybe<double> timestamp,
                                      Maybe<double> delta_x,
                                      Maybe<double> delta_y,
                                      Maybe<int> modifiers,
                                      Maybe<int> click_count) override;

  Response SetIgnoreInputEvents(bool ignore) override;

  void SynthesizePinchGesture(
      double x,
      double y,
      double scale_factor,
      Maybe<int> relative_speed,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizePinchGestureCallback> callback) override;

  void SynthesizeScrollGesture(
      double x,
      double y,
      Maybe<double> x_distance,
      Maybe<double> y_distance,
      Maybe<double> x_overscroll,
      Maybe<double> y_overscroll,
      Maybe<bool> prevent_fling,
      Maybe<int> speed,
      Maybe<std::string> gesture_source_type,
      Maybe<int> repeat_count,
      Maybe<int> repeat_delay_ms,
      Maybe<std::string> interaction_marker_name,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback) override;

  void SynthesizeTapGesture(
      double x,
      double y,
      Maybe<int> duration,
      Maybe<int> tap_count,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizeTapGestureCallback> callback) override;

 private:
  class InputInjector;

  void DispatchWebTouchEvent(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback);

  void DispatchSyntheticPointerActionTouch(
      const std::string& type,
      std::unique_ptr<Array<Input::TouchPoint>> touch_points,
      protocol::Maybe<int> modifiers,
      protocol::Maybe<double> timestamp,
      std::unique_ptr<DispatchTouchEventCallback> callback);

  SyntheticPointerActionParams PrepareSyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType pointer_action_type,
      int id,
      const std::string& button_name,
      double x,
      double y,
      int key_modifiers,
      float radius_x = 1.f,
      float radius_y = 1.f,
      float rotation_angle = 0.f,
      float force = 1.f);

  void SynthesizeRepeatingScroll(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback);

  void OnScrollFinished(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback,
      SyntheticGesture::Result result);

  void ClearInputState();
  bool PointIsWithinContents(gfx::PointF point) const;
  InputInjector* EnsureInjector(RenderWidgetHostImpl* widget_host);
  RenderWidgetHostImpl* FindTargetWidgetHost(const gfx::PointF& point,
                                             gfx::PointF* transformed);

  RenderWidgetHostViewBase* GetRootView();

  RenderFrameHostImpl* host_;
  base::flat_set<std::unique_ptr<InputInjector>, base::UniquePtrComparator>
      injectors_;
  float page_scale_factor_;
  int last_id_;
  bool ignore_input_events_ = false;
  std::set<int> pointer_ids_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  base::flat_map<int, blink::WebTouchPoint> touch_points_;
  base::WeakPtrFactory<InputHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
