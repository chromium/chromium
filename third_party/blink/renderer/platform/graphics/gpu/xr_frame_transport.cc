// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/image_to_buffer_copier.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "ui/gfx/gpu_fence.h"

namespace blink {

XRFrameTransport::XRFrameTransport(
    ContextLifecycleNotifier* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : submit_frame_client_receiver_(this, context), task_runner_(task_runner) {}

XRFrameTransport::~XRFrameTransport() = default;

void XRFrameTransport::PresentChange() {
  // Ensure we don't wait for a frame separator fence when rapidly exiting and
  // re-entering presentation, cf. https://crbug.com/855722.
  waiting_for_previous_frame_fence_ = false;
}

void XRFrameTransport::SetTransportOptions(
    device::mojom::blink::XRPresentationTransportOptionsPtr transport_options) {
  transport_options_ = std::move(transport_options);
}

void XRFrameTransport::BindSubmitFrameClient(
    mojo::PendingReceiver<device::mojom::blink::XRPresentationClient>
        receiver) {
  submit_frame_client_receiver_.reset();
  submit_frame_client_receiver_.Bind(std::move(receiver), task_runner_);
}

bool XRFrameTransport::DrawingIntoSharedBuffer() {
  switch (transport_options_->transport_method) {
    case device::mojom::blink::XRPresentationTransportMethod::
        SUBMIT_AS_TEXTURE_HANDLE:
    case device::mojom::blink::XRPresentationTransportMethod::
        SUBMIT_AS_MAILBOX_HOLDER:
      return false;
    case device::mojom::blink::XRPresentationTransportMethod::
        DRAW_INTO_TEXTURE_MAILBOX:
      return true;
    default:
      NOTREACHED();
  }
}

void XRFrameTransport::FramePreImage(XRFrameTransportDelegate* delegate) {
  frame_wait_time_ = base::TimeDelta();

  // If we're expecting a fence for the previous frame and it hasn't arrived
  // yet, wait for it to be received.
  if (waiting_for_previous_frame_fence_) {
    frame_wait_time_ += WaitForGpuFenceReceived();
  }
  // If we have a GpuFence (it may be missing if WaitForIncomingMethodCall
  // failed), send it to the GPU service process and ask it to do an
  // asynchronous server wait.
  if (previous_frame_fence_) {
    delegate->WaitOnFence(previous_frame_fence_.get());
    previous_frame_fence_.reset();
  }
}

void XRFrameTransport::FrameSubmitMissing(
    device::mojom::blink::XRPresentationProvider* vr_presentation_provider,
    XRFrameTransportDelegate* delegate,
    int16_t vr_frame_id) {
  TRACE_EVENT0("gpu", "FrameSubmitMissing");
  CHECK(delegate);
  vr_presentation_provider->SubmitFrameMissing(vr_frame_id,
                                               delegate->GenerateSyncToken());
}

bool XRFrameTransport::FrameSubmit(
    device::mojom::blink::XRPresentationProvider* vr_presentation_provider,
    XRFrameTransportDelegate* delegate,
    Vector<device::LayerId> layer_ids,
    Vector<scoped_refptr<StaticBitmapImage>> image_refs,
    int16_t vr_frame_id) {
  DCHECK(transport_options_);
  CHECK(delegate);

  if (transport_options_->transport_method ==
      device::mojom::blink::XRPresentationTransportMethod::
          SUBMIT_AS_TEXTURE_HANDLE) {
#if BUILDFLAG(IS_WIN)
    TRACE_EVENT0("gpu", "XRFrameTransport::CopyImage");
    // Update last_transfer_succeeded_ value. This should usually complete
    // without waiting.
    if (transport_options_->wait_for_transfer_notification) {
      WaitForPreviousTransfer();
    }
    // TODO(crbug.com/359418629): This only works because we're restricted to a
    // single layer at the moment.
    CHECK_EQ(image_refs.size(), 1UL);
    auto [gpu_memory_buffer_handle, sync_token] =
        delegate->CopyImage(*image_refs.begin(), last_transfer_succeeded_);

    // We can fail to obtain a GMB handle if we don't have GPU support, or
    // for some out-of-memory situations.
    // TODO(billorr): Consider whether we should just drop the frame or exit
    // presentation.
    if (gpu_memory_buffer_handle.is_null()) {
      FrameSubmitMissing(vr_presentation_provider, delegate, vr_frame_id);
      // We didn't actually submit anything, so don't set
      // the waiting_for_previous_frame_transfer_ and related state.
      return false;
    }

    // We decompose the cloned handle, and use it to create a
    // mojo::PlatformHandle which will own cleanup of the handle, and will be
    // passed over IPC.
    vr_presentation_provider->SubmitFrameWithTextureHandle(
        vr_frame_id,
        mojo::PlatformHandle(std::move(gpu_memory_buffer_handle)
                                 .dxgi_handle()
                                 .TakeBufferHandle()),
        sync_token);
#else
    NOTIMPLEMENTED();
#endif
  } else if (transport_options_->transport_method ==
             device::mojom::blink::XRPresentationTransportMethod::
                 SUBMIT_AS_MAILBOX_HOLDER) {
    CHECK_EQ(image_refs.size(), 1UL);

    // The AcceleratedStaticBitmapImage must be kept alive until the
    // mailbox is used via CreateAndTexStorage2DSharedImageCHROMIUM, the mailbox
    // itself does not keep it alive. We must keep a reference to the
    // image until the mailbox was consumed.
    StaticBitmapImage* static_image = image_refs.begin()->get();
    static_image->EnsureSyncTokenVerified();

    // Conditionally wait for the previous render to finish. A late wait here
    // attempts to overlap work in parallel with the previous frame's
    // rendering. This is used if submitting fully rendered frames to GVR, but
    // is susceptible to bad GPU scheduling if the new frame competes with the
    // previous frame's incomplete rendering.
    if (waiting_for_previous_frame_render_) {
      frame_wait_time_ += WaitForPreviousRenderToFinish();
    }

    // Save a reference to the image to keep it alive until next frame,
    // but first wait for the transfer to finish before overwriting it.
    // Usually this check is satisfied without waiting.
    if (transport_options_->wait_for_transfer_notification) {
      WaitForPreviousTransfer();
    }
    previous_images_ = std::move(image_refs);

    // Create mailbox and sync token for transfer.
    TRACE_EVENT_BEGIN0("gpu", "XRFrameTransport::GetMailbox");
    auto mailbox_holder = static_image->GetMailboxHolder();
    TRACE_EVENT_END0("gpu", "XRFrameTransport::GetMailbox");

    TRACE_EVENT_BEGIN0("gpu", "XRFrameTransport::SubmitFrame");
    vr_presentation_provider->SubmitFrame(vr_frame_id, mailbox_holder,
                                          frame_wait_time_);
    TRACE_EVENT_END0("gpu", "XRFrameTransport::SubmitFrame");
  } else if (transport_options_->transport_method ==
             device::mojom::blink::XRPresentationTransportMethod::
                 DRAW_INTO_TEXTURE_MAILBOX) {
    TRACE_EVENT0("gpu", "XRFrameTransport::SubmitFrameDrawnIntoTexture");
    gpu::SyncToken sync_token = delegate->GenerateSyncToken();
    if (!sync_token.HasData()) {
      return false;
    }
    if (waiting_for_previous_frame_render_) {
      frame_wait_time_ += WaitForPreviousRenderToFinish();
    }
    vr_presentation_provider->SubmitFrameDrawnIntoTexture(
        vr_frame_id, std::move(layer_ids), sync_token, frame_wait_time_);
  } else {
    NOTREACHED() << "Unimplemented frame transport method";
  }

  // Set the expected notifications the next frame should wait for.
  waiting_for_previous_frame_transfer_ =
      transport_options_->wait_for_transfer_notification;
  waiting_for_previous_frame_render_ =
      transport_options_->wait_for_render_notification;
  waiting_for_previous_frame_fence_ = transport_options_->wait_for_gpu_fence;
  return true;
}

void XRFrameTransport::OnSubmitFrameTransferred(bool success) {
  DVLOG(3) << __func__;
  waiting_for_previous_frame_transfer_ = false;
  last_transfer_succeeded_ = success;
}

void XRFrameTransport::RegisterFrameRenderedCallback(
    base::RepeatingClosure callback) {
  on_submit_frame_rendered_callback_ = std::move(callback);
}

void XRFrameTransport::WaitForPreviousTransfer() {
  DVLOG(3) << __func__ << " Start";
  TRACE_EVENT0("gpu", "waitForPreviousTransferToFinish");
  while (waiting_for_previous_frame_transfer_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __func__ << ": Failed to receive response";
      break;
    }
  }
  DVLOG(3) << __func__ << " Stop";
}

void XRFrameTransport::OnSubmitFrameRendered() {
  DVLOG(3) << __func__;
  waiting_for_previous_frame_render_ = false;
  if (on_submit_frame_rendered_callback_) {
    on_submit_frame_rendered_callback_.Run();
  }
}

base::TimeDelta XRFrameTransport::WaitForPreviousRenderToFinish() {
  DVLOG(3) << __func__ << " Start";
  TRACE_EVENT0("gpu", "waitForPreviousRenderToFinish");
  base::TimeTicks start = base::TimeTicks::Now();
  while (waiting_for_previous_frame_render_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __func__ << ": Failed to receive response";
      break;
    }
  }
  DVLOG(3) << __func__ << " Stop";
  return base::TimeTicks::Now() - start;
}

void XRFrameTransport::OnSubmitFrameGpuFence(gfx::GpuFenceHandle handle) {
  // We just received a GpuFence, unblock WaitForGpuFenceReceived.
  waiting_for_previous_frame_fence_ = false;
  previous_frame_fence_ = std::make_unique<gfx::GpuFence>(std::move(handle));
  if (on_submit_frame_rendered_callback_) {
    on_submit_frame_rendered_callback_.Run();
  }
}

base::TimeDelta XRFrameTransport::WaitForGpuFenceReceived() {
  DVLOG(3) << __func__ << " Start";
  TRACE_EVENT0("gpu", "WaitForGpuFenceReceived");
  base::TimeTicks start = base::TimeTicks::Now();
  while (waiting_for_previous_frame_fence_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __func__ << ": Failed to receive response";
      break;
    }
  }
  DVLOG(3) << __func__ << " Stop";
  return base::TimeTicks::Now() - start;
}

void XRFrameTransport::Trace(Visitor* visitor) const {
  visitor->Trace(submit_frame_client_receiver_);
}

}  // namespace blink
