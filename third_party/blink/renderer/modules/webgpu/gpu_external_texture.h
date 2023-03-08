// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_

#include <atomic>

#include "base/task/single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class GPUExternalTexture;
class GPUExternalTextureDescriptor;
class WebGPUMailboxTexture;
class HTMLVideoElement;
class VideoFrame;

// GPUExternalTexture uses auto expiry mechanism
// (https://www.w3.org/TR/webgpu/#-automatic-expiry-task-source). The
// mechanism requires webgpu to expire GPUExternalTexture when current task
// scope finished by posting expiration task. The expired GPUExternalTexture
// is invalid to submit and needs to call importExternalTexture() to get the
// refreshed GPUExternalTexture object. In implementation side,
// importExternalTexture() also wraps GPUExternalTexture with underly video
// frames. It is possible that multiple importExternalTexture() call with the
// same source tries to wrap the same underly video frames. So a cache system
// has been integrated to avoid re-creating the GPUExternalTexture again and
// again with the same video frame. So importExternalTexture() tries to do:
// - Search cache to see any hit. if not, create a new GPUExternalTexture and
// insert it into cache.
// - Refresh the external texture to un-expire it.
// - Post a task to expire this external texture after finishing current task.
// More details refers to
// https://www.w3.org/TR/webgpu/#external-texture-creation
class ExternalTextureCache : public GarbageCollected<ExternalTextureCache> {
 public:
  explicit ExternalTextureCache(GPUDevice* device);
  ExternalTextureCache(const ExternalTextureCache&) = delete;
  ExternalTextureCache& operator=(const ExternalTextureCache&) = delete;

  // Implement importExternalTexture() auto expiry mechanism.
  GPUExternalTexture* Import(ExecutionContext* execution_context,
                             const GPUExternalTextureDescriptor* descriptor,
                             ExceptionState& exception_state);

  // Destroy all cached GPUExternalTexture and clear all lists.
  void Destroy();

  void Add(HTMLVideoElement* video, GPUExternalTexture* external_texture);
  void Remove(HTMLVideoElement* video);

  void Add(VideoFrame* frame, GPUExternalTexture* external_texture);
  void Remove(VideoFrame* frame);

  void Trace(Visitor* visitor) const;
  GPUDevice* device() const;

 private:
  void ExpireAtEndOfTask(GPUExternalTexture* external_texture);
  void ExpireTask();

  // Keep a list of all active GPUExternalTexture. Eagerly destroy them
  // when the device is destroyed (via .destroy) to free the memory.
  HeapHashMap<WeakMember<HTMLVideoElement>, WeakMember<GPUExternalTexture>>
      from_html_video_element_;
  HeapHashMap<WeakMember<VideoFrame>, WeakMember<GPUExternalTexture>>
      from_video_frame_;

  bool expire_task_scheduled_ = false;
  HeapVector<Member<GPUExternalTexture>> expire_list_;

  Member<GPUDevice> device_;
};

class GPUExternalTexture : public DawnObject<WGPUExternalTexture> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUExternalTexture* CreateExpired(
      ExternalTextureCache* cache,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  static GPUExternalTexture* FromHTMLVideoElement(
      ExternalTextureCache* cache,
      HTMLVideoElement* video,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  static GPUExternalTexture* FromVideoFrame(
      ExternalTextureCache* cache,
      VideoFrame* frame,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  explicit GPUExternalTexture(
      ExternalTextureCache* cache,
      WGPUExternalTexture external_texture,
      scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
      bool is_zero_copy,
      absl::optional<media::VideoFrame::ID> media_video_frame_unique_id);

  GPUExternalTexture(const GPUExternalTexture&) = delete;
  GPUExternalTexture& operator=(const GPUExternalTexture&) = delete;

  bool isZeroCopy() const;

  void Destroy();
  void Expire();
  void Refresh();

  void ListenToHTMLVideoElement(HTMLVideoElement* video);
  void ListenToVideoFrame(VideoFrame* frame);

  // Check whether current VideoFrame is outdated informs
  // ScriptAnimationController.
  // Return true if current VideoFrame is latest and still need to trigger next
  // check.
  // Return false if current VideoFrame is outdated and the no need to trigger
  // future checks.
  bool ContinueCheckingCurrentVideoFrame();

  // GPUExternalTexture from VideoFrame expires when VideoFrame is closed. Note
  // that all back resources destroyed needs to happen on the thread that
  // GPUExternalTexture is created.
  // In multithread situation, the callback should change the state of external
  // texture to State::Expired and post a task to issue the destroy.
  void OnVideoFrameClosed();

  void Trace(Visitor* visitor) const override;

 private:
  //                     [1]           [2]
  // Creation -> [Active] --> [Expired] --> [Destroyed]
  //                ^            |
  //                |-------------
  //                     [3]
  //
  // [1] Happens when the current task finishes: the GPUExternalTexture cannot
  // be used util it is refreshed [2] Happens when the source changes frames,
  // the texture can no longer be refreshed. [3] Happens when the texture is
  // refreshed by being re-imported.
  enum class Status { Active, Expired, Destroyed };
  static GPUExternalTexture* CreateImpl(
      ExternalTextureCache* cache,
      const GPUExternalTextureDescriptor* webgpu_desc,
      scoped_refptr<media::VideoFrame> media_video_frame,
      media::PaintCanvasVideoRenderer* video_renderer,
      absl::optional<media::VideoFrame::ID> media_video_frame_unique_id,
      ExceptionState& exception_state);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().externalTextureSetLabel(GetHandle(), utf8_label.c_str());
  }

  // This is the function to push a task to destroy the external texture when
  // the imported video frame in GPUDevice cache is outdated. The function is
  // used as callback function and be registered to the imported
  // Blink::VideoFrame or HTMLVideoElement.
  void OnSourceInvalidated();

  // GPUDevice holds cache for GPUExternalTextures to handling import same
  // frame multiple time cases.
  void RemoveFromCache();

  bool active() const;
  bool expired() const;
  bool destroyed() const;

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;
  bool is_zero_copy_ = false;

  absl::optional<media::VideoFrame::ID> media_video_frame_unique_id_;
  WeakMember<HTMLVideoElement> video_;
  WeakMember<VideoFrame> frame_;
  WeakMember<ExternalTextureCache> cache_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::atomic<Status> status_ = Status::Expired;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
