// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_

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
      absl::optional<int> media_video_frame_unique_id);

  GPUExternalTexture(const GPUExternalTexture&) = delete;
  GPUExternalTexture& operator=(const GPUExternalTexture&) = delete;

  void Destroy();

  bool expired() const { return expired_; }

  void ListenToHTMLVideoElement(HTMLVideoElement* video);

  // Check whether current VideoFrame is outdated informs
  // ScriptAnimationController.
  // Return true if current VideoFrame is latest and still need to trigger next
  // check.
  // Return false if current VideoFrame is outdated and the no need to trigger
  // future checks.
  bool ContinueCheckingCurrentVideoFrame();

  void Trace(Visitor* visitor) const override;

 private:
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
      absl::optional<int> media_video_frame_unique_id,
      ExceptionState& exception_state);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().externalTextureSetLabel(GetHandle(), utf8_label.c_str());
  }

  void DestroyActiveExternalTexture();

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture_;

  absl::optional<int> media_video_frame_unique_id_;
  WeakMember<HTMLVideoElement> video_;

  // This attribute marks whether GPUExternalTexture will be destroyed.
  bool expired_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_EXTERNAL_TEXTURE_H_
