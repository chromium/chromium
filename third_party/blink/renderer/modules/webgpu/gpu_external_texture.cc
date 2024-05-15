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
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
ExternalTextureCache::ExternalTextureCache(GPUDevice* device)
    : device_(device) {}

GPUExternalTexture* ExternalTextureCache::Import(
    const GPUExternalTextureDescriptor* descriptor,
    ExceptionState& exception_state) {
  // Ensure the GPUExternalTexture created from a destroyed GPUDevice will be
  // expired immediately.
  if (device()->destroyed()) {
    return GPUExternalTexture::CreateExpired(this, descriptor, exception_state);
  }

  GPUExternalTexture* external_texture = nullptr;
  switch (descriptor->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = descriptor->source()->GetAsHTMLVideoElement();
      auto cache = from_html_video_element_.find(video);
      if (cache != from_html_video_element_.end()) {
        external_texture = cache->value;

        // If we got a cache miss, or `ContinueCheckingCurrentVideoFrame`
        // returned false, make a new external texture.
        // `ContinueCheckingCurrentVideoFrame` returns false if the frame has
        // expired and it no longer needs to be checked for expiry.
        if (external_texture->NeedsToUpdate()) {
          external_texture = GPUExternalTexture::FromHTMLVideoElement(
              this, video, descriptor, exception_state);
        }
      } else {
        external_texture = GPUExternalTexture::FromHTMLVideoElement(
            this, video, descriptor, exception_state);
      }

      // GPUExternalTexture imported from HTMLVideoElement should be expired
      // at the end of task.
      if (external_texture) {
        external_texture->Refresh();
        ExpireAtEndOfTask(external_texture);
      }
      break;
    }
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = descriptor->source()->GetAsVideoFrame();

      auto cache = from_video_frame_.find(frame);
      if (cache != from_video_frame_.end()) {
        external_texture = cache->value;
      } else {
        external_texture = GPUExternalTexture::FromVideoFrame(
            this, frame, descriptor, exception_state);
      }
      break;
    }
  }

  return external_texture;
}

void ExternalTextureCache::Destroy() {
  // Skip pending expiry tasks to destroy all pending external textures.
  expire_task_scheduled_ = false;

  for (auto& cache : from_html_video_element_) {
    cache.value->Destroy();
  }
  from_html_video_element_.clear();

  for (auto& cache : from_video_frame_) {
    cache.value->Destroy();
  }
  from_video_frame_.clear();

  // GPUExternalTexture in expire list should be in from_html_video_element_ and
  // from_video_frame_. It has been destroyed when clean up the cache. Clear
  // list here is enough.
  expire_set_.clear();
}

void ExternalTextureCache::Add(HTMLVideoElement* video,
                               GPUExternalTexture* external_texture) {
  from_html_video_element_.insert(video, external_texture);
}

void ExternalTextureCache::Remove(HTMLVideoElement* video) {
  from_html_video_element_.erase(video);
}

void ExternalTextureCache::Add(VideoFrame* frame,
                               GPUExternalTexture* external_texture) {
  from_video_frame_.insert(frame, external_texture);
}

void ExternalTextureCache::Remove(VideoFrame* frame) {
  from_video_frame_.erase(frame);
}

void ExternalTextureCache::Trace(Visitor* visitor) const {
  visitor->Trace(from_html_video_element_);
  visitor->Trace(from_video_frame_);
  visitor->Trace(expire_set_);
  visitor->Trace(device_);
}

GPUDevice* ExternalTextureCache::device() const {
  return device_.Get();
}

void ExternalTextureCache::ExpireAtEndOfTask(
    GPUExternalTexture* external_texture) {
  CHECK(external_texture);
  expire_set_.insert(external_texture);

  if (expire_task_scheduled_) {
    return;
  }

  device()
      ->GetExecutionContext()
      ->GetTaskRunner(TaskType::kWebGPU)
      ->PostTask(FROM_HERE, WTF::BindOnce(&ExternalTextureCache::ExpireTask,
                                          WrapWeakPersistent(this)));
  expire_task_scheduled_ = true;
}

void ExternalTextureCache::ExpireTask() {
  // GPUDevice.destroy() call has destroyed all pending external textures.
  if (!expire_task_scheduled_) {
    return;
  }

  expire_task_scheduled_ = false;

  auto external_textures = std::move(expire_set_);
  for (auto& external_texture : external_textures) {
    external_texture->Expire();
  }
}

void ExternalTextureCache::ReferenceUntilGPUIsFinished(
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture) {
  CHECK(mailbox_texture);
  ExecutionContext* execution_context = device()->GetExecutionContext();

  // If device has no valid execution context. Release
  // the mailbox immediately.
  if (!execution_context) {
    return;
  }

  // Keep mailbox texture alive until callback returns.
  auto* callback = BindWGPUOnceCallback(
      [](scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
         WGPUQueueWorkDoneStatus) {},
      std::move(mailbox_texture));

  device()->queue()->GetHandle().OnSubmittedWorkDone(
      callback->UnboundCallback(), callback->AsUserdata());

  // Ensure commands are flushed.
  device()->EnsureFlush(ToEventLoop(execution_context));
}

// static
GPUExternalTexture* GPUExternalTexture::CreateImpl(
    ExternalTextureCache* cache,
    const GPUExternalTextureDescriptor* webgpu_desc,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer,
    std::optional<media::VideoFrame::ID> media_video_frame_unique_id,
    ExceptionState& exception_state) {
  CHECK(media_video_frame);

  PredefinedColorSpace dst_predefined_color_space;
  if (!ValidateAndConvertColorSpace(webgpu_desc->colorSpace(),
                                    dst_predefined_color_space,
                                    exception_state)) {
    return nullptr;
  }

  ExternalTexture external_texture =
      CreateExternalTexture(cache->device(), dst_predefined_color_space,
                            media_video_frame, video_renderer);

  if (external_texture.wgpu_external_texture == nullptr ||
      external_texture.mailbox_texture == nullptr) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  GPUExternalTexture* gpu_external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          cache, std::move(external_texture.wgpu_external_texture),
          external_texture.mailbox_texture, external_texture.is_zero_copy,
          media_video_frame->metadata().read_lock_fences_enabled,
          media_video_frame_unique_id, webgpu_desc->label());

  return gpu_external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::CreateExpired(
    ExternalTextureCache* cache,
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
          cache, cache->device()->GetHandle().CreateErrorExternalTexture(),
          nullptr /*mailbox_texture*/, false /*is_zero_copy*/,
          false /*read_lock_fences_enabled*/,
          std::nullopt /*media_video_frame_unique_id*/, webgpu_desc->label());

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromHTMLVideoElement(
    ExternalTextureCache* cache,
    HTMLVideoElement* video,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoElement(video, exception_state);
  if (!source.valid)
    return nullptr;

  // Ensure that video playback remains unaffected by preventing any
  // throttling when the video is not visible on the screen.
  DCHECK(video);
  if (auto* wmp = video->GetWebMediaPlayer()) {
    wmp->RequestVideoFrameCallback();
  }

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      cache, webgpu_desc, source.media_video_frame, source.video_renderer,
      source.media_video_frame_unique_id, exception_state);

  // WebGPU Spec requires that If the latest presented frame of video is not
  // the same frame from which texture was imported, set expired to true and
  // releasing ownership of the underlying resource and remove the texture from
  // active list. Listen to HTMLVideoElement and insert the texture into active
  // list for management.
  if (external_texture) {
    external_texture->SetVideo(video);
    cache->Add(video, external_texture);
  }

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromVideoFrame(
    ExternalTextureCache* cache,
    VideoFrame* frame,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoFrame(frame, exception_state);
  if (!source.valid)
    return nullptr;

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      cache, webgpu_desc, source.media_video_frame, source.video_renderer,
      std::nullopt, exception_state);

  // If the webcodec video frame has been closed or destroyed, set expired to
  // true, releasing ownership of the underlying resource and remove the texture
  // from active list. Listen to the VideoFrame and insert the texture into
  // active list for management.
  if (external_texture) {
    if (!external_texture->ListenToVideoFrame(frame)) {
      return nullptr;
    }

    cache->Add(frame, external_texture);
  }

  return external_texture;
}

GPUExternalTexture::GPUExternalTexture(
    ExternalTextureCache* cache,
    wgpu::ExternalTexture external_texture,
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
    bool is_zero_copy,
    bool read_lock_fences_enabled,
    std::optional<media::VideoFrame::ID> media_video_frame_unique_id,
    const String& label)
    : DawnObject<wgpu::ExternalTexture>(cache->device(),
                                        external_texture,
                                        label),
      mailbox_texture_(std::move(mailbox_texture)),
      is_zero_copy_(is_zero_copy),
      read_lock_fences_enabled_(read_lock_fences_enabled),
      media_video_frame_unique_id_(media_video_frame_unique_id),
      cache_(cache) {
  task_runner_ =
      device()->GetExecutionContext()->GetTaskRunner(TaskType::kWebGPU);

  // Mark GPUExternalTexture without back resources as destroyed because no need
  // to do real resource releasing.
  if (!mailbox_texture_)
    status_ = Status::Destroyed;
}

void GPUExternalTexture::Refresh() {
  CHECK(status_ != Status::Destroyed);

  if (active()) {
    return;
  }

  GetHandle().Refresh();
  status_ = Status::Active;
}

void GPUExternalTexture::Expire() {
  if (expired() || destroyed()) {
    return;
  }

  GetHandle().Expire();
  status_ = Status::Expired;
}

void GPUExternalTexture::Destroy() {
  DCHECK(!destroyed());
  DCHECK(mailbox_texture_);

  // One copy path finished video frame access after GPUExternalTexture
  // construction. Zero copy path needs to ensure all gpu commands
  // execution finished before destroy.
  if (isZeroCopy() && isReadLockFenceEnabled()) {
    cache_->ReferenceUntilGPUIsFinished(std::move(mailbox_texture_));
  }

  status_ = Status::Destroyed;
  mailbox_texture_.reset();
}

void GPUExternalTexture::SetVideo(HTMLVideoElement* video) {
  CHECK(video);
  video_ = video;
}

bool GPUExternalTexture::NeedsToUpdate() {
  CHECK(media_video_frame_unique_id_.has_value());
  CHECK(video_);

  if (IsCurrentFrameFromHTMLVideoElementValid()) {
    return false;
  }

  // Schedule source invalid task to remove GPUExternalTexture
  // from cache.
  OnSourceInvalidated();

  // If GPUExternalTexture is used in current task scope, don't do
  // reimport until current task scope finished.
  if (active()) {
    return false;
  }

  return true;
}

void GPUExternalTexture::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(video_);
  visitor->Trace(cache_);
  DawnObject<wgpu::ExternalTexture>::Trace(visitor);
}

bool GPUExternalTexture::IsCurrentFrameFromHTMLVideoElementValid() {
  CHECK(video_);
  CHECK(media_video_frame_unique_id_.has_value());

  WebMediaPlayer* media_player = video_->GetWebMediaPlayer();

  // HTMLVideoElement transition from having a WMP to not having one.
  if (!media_player) {
    return false;
  }

  // VideoFrame unique id is unique in the same process. Compare the unique id
  // with current video frame from compositor to detect a new presented
  // video frame and expire the GPUExternalTexture.
  if (media_video_frame_unique_id_ != media_player->CurrentFrameId()) {
    return false;
  }

  return true;
}

void GPUExternalTexture::OnSourceInvalidated() {
  CHECK(task_runner_);
  CHECK(task_runner_->BelongsToCurrentThread());

  // OnSourceInvalidated is called for both VideoFrame and HTMLVE.
  // VideoFrames are invalidated with and explicit close() call that
  // should mark the ExternalTexture destroyed immediately.
  // However HTMLVE could decide to advance in the middle of the task
  // that imported the ExternalTexture. In that case defer the invalidation
  // until the end of the task to preserve the semantic of ExternalTexture.
  if (status_ == Status::Active && video_) {
    if (!remove_from_cache_task_scheduled_) {
      task_runner_->PostTask(FROM_HERE,
                             WTF::BindOnce(&GPUExternalTexture::RemoveFromCache,
                                           WrapWeakPersistent(this)));
    }
    remove_from_cache_task_scheduled_ = true;
  } else {
    RemoveFromCache();
  }
}

void GPUExternalTexture::RemoveFromCache() {
  if (video_) {
    cache_->Remove(video_);
  } else if (frame_) {
    cache_->Remove(frame_);
  }

  Destroy();
}

bool GPUExternalTexture::ListenToVideoFrame(VideoFrame* frame) {
  if (!frame->handle()->WebGPURegisterExternalTextureExpireCallback(
          CrossThreadBindOnce(&GPUExternalTexture::OnVideoFrameClosed,
                              WrapCrossThreadWeakPersistent(this)))) {
    OnSourceInvalidated();
    return false;
  }

  frame_ = frame;
  return true;
}

void GPUExternalTexture::OnVideoFrameClosed() {
  CHECK(task_runner_);

  if (destroyed())
    return;

  // Expire the GPUExternalTexture here in the main thread to prevent it from
  // being used again (because WebGPU runs on the main thread). Expiring the
  // texture later in ExpireExternalTextureFromVideoFrame() could occur on a
  // worker thread and cause a race condition.
  Expire();

  if (task_runner_->BelongsToCurrentThread()) {
    OnSourceInvalidated();
    return;
  }

  // If current thread is not the one that creates GPUExternalTexture. Post task
  // to that thread to destroy the GPUExternalTexture.
  task_runner_->PostTask(FROM_HERE,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             &GPUExternalTexture::OnVideoFrameClosed,
                             WrapCrossThreadWeakPersistent(this))));
}

bool GPUExternalTexture::active() const {
  return status_ == Status::Active;
}

bool GPUExternalTexture::expired() const {
  return status_ == Status::Expired;
}

bool GPUExternalTexture::isZeroCopy() const {
  return is_zero_copy_;
}

bool GPUExternalTexture::isReadLockFenceEnabled() const {
  return read_lock_fences_enabled_;
}

bool GPUExternalTexture::destroyed() const {
  return status_ == Status::Destroyed;
}

}  // namespace blink
