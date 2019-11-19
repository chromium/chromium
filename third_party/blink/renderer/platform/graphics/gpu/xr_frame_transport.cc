// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/renderer/platform/graphics/gpu_memory_buffer_image_copy.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "ui/gfx/gpu_fence.h"

namespace blink {

XRFrameTransport::XRFrameTransport() : submit_frame_client_receiver_(this) {}

XRFrameTransport::~XRFrameTransport() {
  CallPreviousFrameCallback();
}

void XRFrameTransport::PresentChange() {
  frame_copier_ = nullptr;

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
  submit_frame_client_receiver_.Bind(std::move(receiver));
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
      return false;
  }
}

void XRFrameTransport::FramePreImage(gpu::gles2::GLES2Interface* gl) {
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
    DVLOG(3) << "CreateClientGpuFenceCHROMIUM";
    GLuint id = gl->CreateClientGpuFenceCHROMIUM(
        previous_frame_fence_->AsClientGpuFence());
    gl->WaitGpuFenceCHROMIUM(id);
    gl->DestroyGpuFenceCHROMIUM(id);
    previous_frame_fence_.reset();
  }
}

void XRFrameTransport::CallPreviousFrameCallback() {
  if (previous_image_release_callback_) {
    previous_image_release_callback_->Run(gpu::SyncToken(), false);
    previous_image_release_callback_ = nullptr;
  }
}

void XRFrameTransport::FrameSubmitMissing(
    device::mojom::blink::XRPresentationProvider* vr_presentation_provider,
    gpu::gles2::GLES2Interface* gl,
    int16_t vr_frame_id) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(sync_token.GetData());
  vr_presentation_provider->SubmitFrameMissing(vr_frame_id, sync_token);
}

void XRFrameTransport::FrameSubmit(
    device::mojom::blink::XRPresentationProvider* vr_presentation_provider,
    gpu::gles2::GLES2Interface* gl,
    DrawingBuffer::Client* drawing_buffer_client,
    scoped_refptr<Image> image_ref,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    int16_t vr_frame_id) {
  DCHECK(transport_options_);

  if (transport_options_->transport_method ==
      device::mojom::blink::XRPresentationTransportMethod::
          SUBMIT_AS_TEXTURE_HANDLE) {
#if defined(OS_WIN)
    TRACE_EVENT0("gpu", "XRFrameTransport::CopyImage");
    // Update last_transfer_succeeded_ value. This should usually complete
    // without waiting.
    if (transport_options_->wait_for_transfer_notification)
      WaitForPreviousTransfer();
    CallPreviousFrameCallback();
    previous_image_release_callback_ = std::move(release_callback);
    if (!frame_copier_ || !last_transfer_succeeded_) {
      frame_copier_ = std::make_unique<GpuMemoryBufferImageCopy>(gl);
    }
    gfx::GpuMemoryBuffer* gpu_memory_buffer =
        frame_copier_->CopyImage(image_ref.get());
    drawing_buffer_client->DrawingBufferClientRestoreTexture2DBinding();
    drawing_buffer_client->DrawingBufferClientRestoreFramebufferBinding();
    drawing_buffer_client->DrawingBufferClientRestoreRenderbufferBinding();

    // We can fail to obtain a GpuMemoryBuffer if we don't have GPU support, or
    // for some out-of-memory situations.
    // TODO(billorr): Consider whether we should just drop the frame or exit
    // presentation.
    if (!gpu_memory_buffer) {
      FrameSubmitMissing(vr_presentation_provider, gl, vr_frame_id);
      // We didn't actually submit anything, so don't set
      // the waiting_for_previous_frame_transfer_ and related state.
      return;
    }

    // We decompose the cloned handle, and use it to create a
    // mojo::ScopedHandle which will own cleanup of the handle, and will be
    // passed over IPC.
    gfx::GpuMemoryBufferHandle gpu_handle = gpu_memory_buffer->CloneHandle();
    vr_presentation_provider->SubmitFrameWithTextureHandle(
        vr_frame_id,
        mojo::WrapPlatformFile(gpu_handle.dxgi_handle.GetHandle()));
#else
    NOTIMPLEMENTED();
#endif
  } else if (transport_options_->transport_method ==
             device::mojom::blink::XRPresentationTransportMethod::
                 SUBMIT_AS_MAILBOX_HOLDER) {
    // The AcceleratedStaticBitmapImage must be kept alive until the
    // mailbox is used via createAndConsumeTextureCHROMIUM, the mailbox
    // itself does not keep it alive. We must keep a reference to the
    // image until the mailbox was consumed.
    StaticBitmapImage* static_image =
        static_cast<StaticBitmapImage*>(image_ref.get());
    TRACE_EVENT_BEGIN0("gpu", "XRFrameTransport::EnsureMailbox");
    static_image->EnsureMailbox(kVerifiedSyncToken, GL_NEAREST);
    TRACE_EVENT_END0("gpu", "XRFrameTransport::EnsureMailbox");

    // Conditionally wait for the previous render to finish. A late wait here
    // attempts to overlap work in parallel with the previous frame's
    // rendering. This is used if submitting fully rendered frames to GVR, but
    // is susceptible to bad GPU scheduling if the new frame competes with the
    // previous frame's incomplete rendering.
    if (waiting_for_previous_frame_render_)
      frame_wait_time_ += WaitForPreviousRenderToFinish();

    // Save a reference to the image to keep it alive until next frame,
    // but first wait for the transfer to finish before overwriting it.
    // Usually this check is satisfied without waiting.
    if (transport_options_->wait_for_transfer_notification)
      WaitForPreviousTransfer();
    previous_image_ = std::move(image_ref);
    CallPreviousFrameCallback();
    previous_image_release_callback_ = std::move(release_callback);

    // Create mailbox and sync token for transfer.
    TRACE_EVENT_BEGIN0("gpu", "XRFrameTransport::GetMailbox");
    auto mailbox = static_image->GetMailbox();
    TRACE_EVENT_END0("gpu", "XRFrameTransport::GetMailbox");
    auto sync_token = static_image->GetSyncToken();

    TRACE_EVENT_BEGIN0("gpu", "XRFrameTransport::SubmitFrame");
    vr_presentation_provider->SubmitFrame(
        vr_frame_id, gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D),
        frame_wait_time_);
    TRACE_EVENT_END0("gpu", "XRFrameTransport::SubmitFrame");
  } else if (transport_options_->transport_method ==
             device::mojom::blink::XRPresentationTransportMethod::
                 DRAW_INTO_TEXTURE_MAILBOX) {
    TRACE_EVENT0("gpu", "XRFrameTransport::SubmitFrameDrawnIntoTexture");
    gpu::SyncToken sync_token;
    {
      TRACE_EVENT0("gpu", "GenSyncTokenCHROMIUM");
      gl->GenSyncTokenCHROMIUM(sync_token.GetData());
    }
    if (waiting_for_previous_frame_render_)
      frame_wait_time_ += WaitForPreviousRenderToFinish();
    vr_presentation_provider->SubmitFrameDrawnIntoTexture(
        vr_frame_id, sync_token, frame_wait_time_);
  } else {
    NOTREACHED() << "Unimplemented frame transport method";
  }

  // Set the expected notifications the next frame should wait for.
  waiting_for_previous_frame_transfer_ =
      transport_options_->wait_for_transfer_notification;
  waiting_for_previous_frame_render_ =
      transport_options_->wait_for_render_notification;
  waiting_for_previous_frame_fence_ = transport_options_->wait_for_gpu_fence;
}

void XRFrameTransport::OnSubmitFrameTransferred(bool success) {
  DVLOG(3) << __FUNCTION__;
  waiting_for_previous_frame_transfer_ = false;
  last_transfer_succeeded_ = success;
}

void XRFrameTransport::WaitForPreviousTransfer() {
  TRACE_EVENT0("gpu", "waitForPreviousTransferToFinish");
  while (waiting_for_previous_frame_transfer_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __FUNCTION__ << ": Failed to receive response";
      break;
    }
  }
}

void XRFrameTransport::OnSubmitFrameRendered() {
  DVLOG(3) << __FUNCTION__;
  waiting_for_previous_frame_render_ = false;
}

base::TimeDelta XRFrameTransport::WaitForPreviousRenderToFinish() {
  TRACE_EVENT0("gpu", "waitForPreviousRenderToFinish");
  base::TimeTicks start = base::TimeTicks::Now();
  while (waiting_for_previous_frame_render_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __FUNCTION__ << ": Failed to receive response";
      break;
    }
  }
  return base::TimeTicks::Now() - start;
}

void XRFrameTransport::OnSubmitFrameGpuFence(
    const gfx::GpuFenceHandle& handle) {
  // We just received a GpuFence, unblock WaitForGpuFenceReceived.
  waiting_for_previous_frame_fence_ = false;
  previous_frame_fence_ = std::make_unique<gfx::GpuFence>(handle);
}

base::TimeDelta XRFrameTransport::WaitForGpuFenceReceived() {
  TRACE_EVENT0("gpu", "WaitForGpuFenceReceived");
  base::TimeTicks start = base::TimeTicks::Now();
  while (waiting_for_previous_frame_fence_) {
    if (!submit_frame_client_receiver_.WaitForIncomingCall()) {
      DLOG(ERROR) << __FUNCTION__ << ": Failed to receive response";
      break;
    }
  }
  return base::TimeTicks::Now() - start;
}

void XRFrameTransport::Trace(blink::Visitor* visitor) {}

}  // namespace blink
