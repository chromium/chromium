// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class HTMLVideoElement;
class VideoFrameMetadata;
struct WebGLVideoFrameUploadMetadata;

class WebGLVideoTexture final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLVideoTexture(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;

  void Trace(Visitor*) const override;

  // Get video frame from video frame compositor and bind it to platform
  // texture.
  VideoFrameMetadata* shareVideoImageWEBGL(ExecutionContext*,
                                           unsigned,
                                           HTMLVideoElement*,
                                           ExceptionState&);

  bool releaseVideoImageWEBGL(ExecutionContext*, unsigned, ExceptionState&);

  // Helper method for filling in WebGLVideoFrameUploadMetadata. Will be default
  // initialized (skipped = false) if the metadata API is disabled.
  static WebGLVideoFrameUploadMetadata CreateVideoFrameUploadMetadata(
      const media::VideoFrame* frame,
      media::VideoFrame::ID already_uploaded_id);

 private:
  Member<VideoFrameMetadata> current_frame_metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_
