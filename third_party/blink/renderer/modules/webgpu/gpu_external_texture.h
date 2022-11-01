// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_

#include <atomic>

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ExceptionState;
class GPUExternalTextureDescriptor;
class HTMLVideoElement;
class VideoFrame;
class WebGPUMailboxTexture;

class GPUExternalTexture : public DawnObject<WGPUExternalTexture> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUExternalTexture* Create(
      GPUDevice* device,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  static GPUExternalTexture* CreateExpired(
      GPUDevice* device,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  explicit GPUExternalTexture(
      GPUDevice* device,
      WGPUExternalTexture external_texture,
      scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
      absl::optional<media::VideoFrame::ID> media_video_frame_unique_id);

  GPUExternalTexture(const GPUExternalTexture&) = delete;
  GPUExternalTexture& operator=(const GPUExternalTexture&) = delete;

  void Destroy();

  bool expired() const;

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
  // The initial state of GPUExternalTexture is Expired. After listening to the
  // imported HTMLVE/VideoFrame, the state should be set to
  // ListenToHTMLVideoElement/VideoFrame, and then should only be changed in the
  // order: ListenToHTMLVideoElement/VideoFrame(->Expired)->Destroyed.
  enum class Status {
    ListenToHTMLVideoElement,
    ListenToVideoFrame,
    Expired,
    Destroyed
  };
  static GPUExternalTexture* FromHTMLVideoElement(
      GPUDevice* device,
      HTMLVideoElement* video,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  static GPUExternalTexture* FromVideoFrame(
      GPUDevice* device,
      VideoFrame* frame,
      const GPUExternalTextureDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  static GPUExternalTexture* CreateImpl(
      GPUDevice* device,
      const GPUExternalTextureDescriptor* webgpu_desc,
      scoped_refptr<media::VideoFrame> media_video_frame,
      media::PaintCanvasVideoRenderer* video_renderer,
      absl::optional<media::VideoFrame::ID> media_video_frame_unique_id,
      ExceptionState& exception_state);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().externalTextureSetLabel(GetHandle(), utf8_label.c_str());
  }

  // This is the function to expire the external texture when the imported
  // Blink::VideoFrame has been closed. The function is used as callback
  // function and be registered to the imported Blink::VideoFrame.
  void ExpireExternalTextureFromVideoFrame();

  // This is the function to expire the external texture when the imported
  // HTMLVideoElement it imported from has been closed. The function is used
  // as callback function and be registered to the imported HTMLVideoElement.
  void ExpireExternalTextureFromHTMLVideoElement();

  void ExpireExternalTexture();

  bool destroyed() const;

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;

  absl::optional<media::VideoFrame::ID> media_video_frame_unique_id_;
  WeakMember<HTMLVideoElement> video_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::atomic<Status> status_ = Status::Expired;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
