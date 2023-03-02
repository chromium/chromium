// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlvideoelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// static
GPUExternalTexture* GPUExternalTexture::CreateImpl(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer,
    absl::optional<media::VideoFrame::ID> media_video_frame_unique_id,
    ExceptionState& exception_state) {
  DCHECK(media_video_frame);

  // TODO(crbug.com/1330250): Support additional color spaces for external
  // textures.
  if (webgpu_desc->colorSpace().AsEnum() !=
      V8PredefinedColorSpace::Enum::kSRGB) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "colorSpace !== 'srgb' isn't supported yet.");
    return nullptr;
  }

  PredefinedColorSpace dst_predefined_color_space;
  if (!ValidateAndConvertColorSpace(webgpu_desc->colorSpace(),
                                    dst_predefined_color_space,
                                    exception_state)) {
    return nullptr;
  }

  gfx::ColorSpace src_color_space = media_video_frame->ColorSpace();
  // It should be very rare that a frame didn't get a valid colorspace through
  // the guessing process:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/video_color_space.cc;l=69;drc=6c9cfff09be8397270b376a4e4407328694e97fa
  // The historical rule for this was to use BT.601 for SD content and BT.709
  // for HD content:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/ffmpeg/ffmpeg_common.cc;l=683;drc=1946212ac0100668f14eb9e2843bdd846e510a1e)
  // We prefer always using BT.709 since SD content in practice is down-scaled
  // HD content, not NTSC broadcast content.
  if (!src_color_space.IsValid()) {
    src_color_space = gfx::ColorSpace::CreateREC709();
  }
  gfx::ColorSpace dst_color_space =
      PredefinedColorSpaceToGfxColorSpace(dst_predefined_color_space);

  ExternalTexture external_texture =
      CreateExternalTexture(device, src_color_space, dst_color_space,
                            media_video_frame, video_renderer);

  if (external_texture.wgpu_external_texture == nullptr ||
      external_texture.mailbox_texture == nullptr) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  GPUExternalTexture* gpu_external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          device, external_texture.wgpu_external_texture,
          external_texture.mailbox_texture, media_video_frame_unique_id);

  return gpu_external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::CreateExpired(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  // Validate GPUExternalTextureDescriptor.
  ExternalTextureSource source;
  switch (webgpu_desc->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = webgpu_desc->source()->GetAsHTMLVideoElement();
      source = GetExternalTextureSourceFromVideoElement(video, exception_state);
      break;
    }
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = webgpu_desc->source()->GetAsVideoFrame();
      source = GetExternalTextureSourceFromVideoFrame(frame, exception_state);
      break;
    }
  }
  if (!source.valid)
    return nullptr;

  // Bypass importing video frame into Dawn.
  GPUExternalTexture* external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          device,
          device->GetProcs().deviceCreateErrorExternalTexture(
              device->GetHandle()),
          nullptr /*mailbox_texture*/,
          absl::nullopt /*media_video_frame_unique_id*/);

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromHTMLVideoElement(
    GPUDevice* device,
    HTMLVideoElement* video,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoElement(video, exception_state);
  if (!source.valid)
    return nullptr;

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      device, webgpu_desc, source.media_video_frame, source.video_renderer,
      source.media_video_frame_unique_id, exception_state);

  // WebGPU Spec requires that If the latest presented frame of video is not
  // the same frame from which texture was imported, set expired to true and
  // releasing ownership of the underlying resource and remove the texture from
  // active list. Listen to HTMLVideoElement and insert the texture into active
  // list for management.
  if (external_texture) {
    external_texture->ListenToHTMLVideoElement(video);
    device->AddActiveExternalTexture(external_texture);
  }

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromVideoFrame(
    GPUDevice* device,
    VideoFrame* frame,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoFrame(frame, exception_state);
  if (!source.valid)
    return nullptr;

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      device, webgpu_desc, source.media_video_frame, source.video_renderer,
      absl::nullopt, exception_state);

  // If the webcodec video frame has been closed or destroyed, set expired to
  // true, releasing ownership of the underlying resource and remove the texture
  // from active list. Listen to the VideoFrame and insert the texture into
  // active list for management.
  if (external_texture) {
    external_texture->ListenToVideoFrame(frame);

    // VideoFrame maybe closed when GPUExternalTexture trying to listen to.
    // In that case GPUExternalTexture should be expired and GPUDevice
    // doesn't need to manage it.
    if (!external_texture->expired())
      device->AddActiveExternalTexture(external_texture);
  }

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::Create(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  switch (webgpu_desc->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = webgpu_desc->source()->GetAsHTMLVideoElement();
      return GPUExternalTexture::FromHTMLVideoElement(
          device, video, webgpu_desc, exception_state);
    }
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = webgpu_desc->source()->GetAsVideoFrame();
      return GPUExternalTexture::FromVideoFrame(device, frame, webgpu_desc,
                                                exception_state);
    }
  }

  NOTREACHED();
}

GPUExternalTexture::GPUExternalTexture(
    GPUDevice* device,
    WGPUExternalTexture external_texture,
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
    absl::optional<media::VideoFrame::ID> media_video_frame_unique_id)
    : DawnObject<WGPUExternalTexture>(device, external_texture),
      mailbox_texture_(mailbox_texture),
      media_video_frame_unique_id_(media_video_frame_unique_id) {
  // Mark GPUExternalTexture without back resources as destroyed because no need
  // to do real resource releasing.
  if (!mailbox_texture_)
    status_ = Status::Destroyed;
}

void GPUExternalTexture::Destroy() {
  DCHECK(!destroyed());
  DCHECK(mailbox_texture_);

  status_ = Status::Destroyed;
  mailbox_texture_.reset();
}

void GPUExternalTexture::ListenToHTMLVideoElement(HTMLVideoElement* video) {
  DCHECK(video);

  video_ = video;
  video->GetDocument()
      .GetScriptedAnimationController()
      .WebGPURegisterVideoFrameStateCallback(WTF::BindRepeating(
          &GPUExternalTexture::ContinueCheckingCurrentVideoFrame,
          WrapPersistent(this)));

  status_ = Status::ListenToHTMLVideoElement;
}

bool GPUExternalTexture::ContinueCheckingCurrentVideoFrame() {
  DCHECK(video_);
  DCHECK(media_video_frame_unique_id_.has_value());

  if (destroyed())
    return false;

  WebMediaPlayer* media_player = video_->GetWebMediaPlayer();

  // HTMLVideoElement transition from having a WMP to not having one.
  if (!media_player) {
    ExpireExternalTextureFromHTMLVideoElement();
    return false;
  }

  // VideoFrame unique id is unique in the same process. Compare the unique id
  // with current video frame from compositor to detect a new presented
  // video frame and expire the GPUExternalTexture.
  if (media_video_frame_unique_id_ != media_player->CurrentFrameId()) {
    ExpireExternalTextureFromHTMLVideoElement();
    return false;
  }

  return true;
}

void GPUExternalTexture::Trace(Visitor* visitor) const {
  visitor->Trace(video_);
  DawnObject<WGPUExternalTexture>::Trace(visitor);
}

void GPUExternalTexture::ExpireExternalTextureFromHTMLVideoElement() {
  DCHECK(status_ != Status::ListenToVideoFrame);
  ExpireExternalTexture();
}

void GPUExternalTexture::ExpireExternalTextureFromVideoFrame() {
  DCHECK(status_ != Status::ListenToHTMLVideoElement);
  ExpireExternalTexture();
}

void GPUExternalTexture::ExpireExternalTexture() {
  device()->RemoveActiveExternalTexture(this);
  Destroy();
}

void GPUExternalTexture::ListenToVideoFrame(VideoFrame* frame) {
  bool success = frame->handle()->WebGPURegisterExternalTextureExpireCallback(
      CrossThreadBindOnce(&GPUExternalTexture::OnVideoFrameClosed,
                          WrapCrossThreadWeakPersistent(this)));
  if (!success) {
    Destroy();
    return;
  }

  task_runner_ =
      device()->GetExecutionContext()->GetTaskRunner(TaskType::kWebGPU);

  status_ = Status::ListenToVideoFrame;
}

void GPUExternalTexture::OnVideoFrameClosed() {
  DCHECK(task_runner_);

  if (destroyed())
    return;

  // Expire the GPUExternalTexture here in the main thread to prevent it from
  // being used again (because WebGPU runs on the main thread). Expiring the
  // texture later in ExpireExternalTextureFromVideoFrame() could occur on a
  // worker thread and cause a race condition.
  status_ = Status::Expired;

  if (task_runner_->BelongsToCurrentThread()) {
    ExpireExternalTextureFromVideoFrame();
    return;
  }

  // If current thread is not the one that creates GPUExternalTexture. Post task
  // to that thread to destroy the GPUExternalTexture.
  task_runner_->PostTask(FROM_HERE,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             &GPUExternalTexture::OnVideoFrameClosed,
                             WrapCrossThreadWeakPersistent(this))));
}

bool GPUExternalTexture::expired() const {
  return status_ == Status::Expired || status_ == Status::Destroyed;
}

bool GPUExternalTexture::destroyed() const {
  return status_ == Status::Destroyed;
}

}  // namespace blink
