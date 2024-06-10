// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
#define CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_

#include "cc/trees/render_frame_metadata.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/gfx/delegated_ink_point.h"

namespace content {

class RenderWidgetHostViewInput;
class RenderInputRouterIterator;
class RenderWidgetHostInputEventRouter;

class CONTENT_EXPORT RenderInputRouterDelegate {
 public:
  virtual ~RenderInputRouterDelegate() = default;

  virtual RenderWidgetHostViewInput* GetPointerLockView() = 0;
  // TODO(b/331419617): Use a new FrameMetadataBase class instead of
  // RenderFrameMetadata.
  virtual const cc::RenderFrameMetadata& GetLastRenderFrameMetadata() = 0;

  virtual std::unique_ptr<RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters() = 0;

  virtual RenderWidgetHostInputEventRouter* GetInputEventRouter() = 0;

  // Forwards |delegated_ink_point| to viz over IPC to be drawn as part of
  // delegated ink trail, resetting the |ended_delegated_ink_trail| flag.
  virtual void ForwardDelegatedInkPoint(
      gfx::DelegatedInkPoint& delegated_ink_point,
      bool& ended_delegated_ink_trail) = 0;
  // Instructs viz to reset prediction for delegated ink trails, indicating that
  // the trail has ended. Updates the |ended_delegated_ink_trail| flag to
  // reflect this change.
  virtual void ResetDelegatedInkPointPrediction(
      bool& ended_delegated_ink_trail) = 0;

  virtual ukm::SourceId GetCurrentPageUkmSourceId() = 0;

  virtual void NotifyObserversOfInputEvent(
      const blink::WebInputEvent& event) = 0;
  virtual void NotifyObserversOfInputEventAcks(
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      const blink::WebInputEvent& event) = 0;

  // Called upon event ack receipt from the renderer.
  virtual void OnGestureEventAck(
      const input::GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void OnWheelEventAck(
      const input::MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_RENDER_INPUT_ROUTER_DELEGATE_H_
