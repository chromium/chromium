// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "services/viz/public/mojom/compositing/frame_timing_details.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider_params.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/mojom/presentation_feedback.mojom-blink.h"

namespace blink {

BeginFrameProvider::BeginFrameProvider(
    const BeginFrameProviderParams& begin_frame_provider_params,
    BeginFrameProviderClient* client,
    ContextLifecycleNotifier* context)
    : needs_begin_frame_(false),
      requested_needs_begin_frame_(false),
      cfs_receiver_(this, context),
      efs_receiver_(this, context),
      frame_sink_id_(begin_frame_provider_params.frame_sink_id),
      parent_frame_sink_id_(begin_frame_provider_params.parent_frame_sink_id),
      compositor_frame_sink_(context),
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

  // Once we are using RAF, this thread is driving user interactive display
  // updates. Update priority accordingly.
  base::PlatformThread::SetCurrentThreadType(
      base::ThreadType::kDisplayCritical);

  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> provider;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      provider.BindNewPipeAndPassReceiver());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      begin_frame_client_->GetCompositorTaskRunner();

  provider->CreateSimpleCompositorFrameSink(
      parent_frame_sink_id_, frame_sink_id_,
      efs_receiver_.BindNewPipeAndPassRemote(task_runner),
      cfs_receiver_.BindNewPipeAndPassRemote(task_runner),
      compositor_frame_sink_.BindNewPipeAndPassReceiver(task_runner));

  compositor_frame_sink_.set_disconnect_with_reason_handler(WTF::BindOnce(
      &BeginFrameProvider::OnMojoConnectionError, WrapWeakPersistent(this)));
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
    const WTF::HashMap<uint32_t, viz::FrameTimingDetails>&,
    bool frame_ack,
    WTF::Vector<viz::ReturnedResource> resources) {
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
  // It appears that we can lose our existing Mojo Connection, and previously
  // posted tasks can attempt to use the unbounded `compositor_frame_sink_`.
  // If that occurs return so that we don't crash.
  if (!compositor_frame_sink_.is_bound()) {
    return;
  }
  compositor_frame_sink_->DidNotProduceFrame(viz::BeginFrameAck(args, false));
}

void BeginFrameProvider::Trace(Visitor* visitor) const {
  visitor->Trace(cfs_receiver_);
  visitor->Trace(efs_receiver_);
  visitor->Trace(compositor_frame_sink_);
  visitor->Trace(begin_frame_client_);
}

}  // namespace blink
