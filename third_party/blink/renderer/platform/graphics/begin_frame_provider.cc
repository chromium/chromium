// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "services/viz/public/mojom/compositing/frame_timing_details.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "ui/gfx/mojom/presentation_feedback.mojom-blink.h"

namespace blink {

BeginFrameProvider::BeginFrameProvider(
    const BeginFrameProviderParams& begin_frame_provider_params,
    BeginFrameProviderClient* client)
    : needs_begin_frame_(false),
      requested_needs_begin_frame_(false),
      frame_sink_id_(begin_frame_provider_params.frame_sink_id),
      parent_frame_sink_id_(begin_frame_provider_params.parent_frame_sink_id),
      begin_frame_client_(client) {}

void BeginFrameProvider::ResetCompositorFrameSink() {
  compositor_frame_sink_.reset();
  efs_receiver_.reset();
  cfs_receiver_.reset();
  if (needs_begin_frame_) {
    needs_begin_frame_ = false;
    RequestBeginFrame();
  }
}

void BeginFrameProvider::OnMojoConnectionError(uint32_t custom_reason,
                                               const std::string& description) {
  if (custom_reason) {
    DLOG(ERROR) << description;
  }
  ResetCompositorFrameSink();
}

bool BeginFrameProvider::IsValidFrameProvider() {
  if (!parent_frame_sink_id_.is_valid() || !frame_sink_id_.is_valid()) {
    return false;
  }

  return true;
}

void BeginFrameProvider::CreateCompositorFrameSinkIfNeeded() {
  if (!parent_frame_sink_id_.is_valid() || !frame_sink_id_.is_valid()) {
    return;
  }

  if (compositor_frame_sink_.is_bound())
    return;

  // Once we are using RAF, this thread is driving Display updates. Update
  // priority accordingly.
  base::PlatformThread::SetCurrentThreadPriority(base::ThreadPriority::DISPLAY);

  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      provider.BindNewPipeAndPassReceiver());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ThreadScheduler::Current()->CompositorTaskRunner();

  provider->CreateSimpleCompositorFrameSink(
      parent_frame_sink_id_, frame_sink_id_,
      efs_receiver_.BindNewPipeAndPassRemote(task_runner),
      cfs_receiver_.BindNewPipeAndPassRemote(task_runner),
      compositor_frame_sink_.BindNewPipeAndPassReceiver());

  compositor_frame_sink_.set_disconnect_with_reason_handler(base::BindOnce(
      &BeginFrameProvider::OnMojoConnectionError, weak_factory_.GetWeakPtr()));
}

void BeginFrameProvider::RequestBeginFrame() {
  requested_needs_begin_frame_ = true;
  if (needs_begin_frame_) {
    return;
  }

  CreateCompositorFrameSinkIfNeeded();

  needs_begin_frame_ = true;
  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void BeginFrameProvider::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    WTF::HashMap<uint32_t, ::viz::mojom::blink::FrameTimingDetailsPtr>) {
  TRACE_EVENT_WITH_FLOW0("blink", "BeginFrameProvider::OnBeginFrame",
                         TRACE_ID_GLOBAL(args.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (args.deadline < base::TimeTicks::Now()) {
    compositor_frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(args, false));
    return;
  }

  // If there was no need for a BeginFrame, just skip it.
  if (needs_begin_frame_ && requested_needs_begin_frame_) {
    requested_needs_begin_frame_ = false;
    begin_frame_client_->BeginFrame(args);
  } else {
    if (!requested_needs_begin_frame_) {
      needs_begin_frame_ = false;
      compositor_frame_sink_->SetNeedsBeginFrame(false);
    }
  }
}

void BeginFrameProvider::FinishBeginFrame(const viz::BeginFrameArgs& args) {
  compositor_frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(args, false));
}

}  // namespace blink
