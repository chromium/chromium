// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_target_client_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"

namespace content {

InputTargetClientImpl::InputTargetClientImpl(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

InputTargetClientImpl::~InputTargetClientImpl() {}

void InputTargetClientImpl::BindToReceiver(
    mojo::PendingReceiver<viz::mojom::InputTargetClient> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver), render_frame_->GetTaskRunner(
                                          blink::TaskType::kInternalDefault));
}

void InputTargetClientImpl::FrameSinkIdAt(const gfx::PointF& point,
                                          const uint64_t trace_id,
                                          FrameSinkIdAtCallback callback) {
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Event.Pipeline",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "FrameSinkIdAt");

  gfx::PointF local_point;
  viz::FrameSinkId id =
      render_frame_->GetLocalRootRenderWidget()->GetFrameSinkIdAtPoint(
          point, &local_point);
  std::move(callback).Run(id, local_point);
}

}  // namespace content
