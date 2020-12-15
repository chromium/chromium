// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace blink {

class WebGLWebCodecsVideoFrameHandle;

class WebGLWebCodecsVideoFrame final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLWebCodecsVideoFrame(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;

  void Trace(Visitor*) const override;

  WebGLWebCodecsVideoFrameHandle* importVideoFrame(ExecutionContext*,
                                                   blink::VideoFrame*,
                                                   ExceptionState&);

  bool releaseVideoFrame(ExecutionContext*,
                         WebGLWebCodecsVideoFrameHandle*,
                         ExceptionState&);

 private:
  std::bitset<media::PIXEL_FORMAT_MAX + 1> formats_supported;
  std::string sampler_type_;
  std::string sampler_func_;
  std::array<std::array<std::string, media::VideoFrame::kMaxPlanes>,
             media::PIXEL_FORMAT_MAX + 1>
      format_to_components_map_;

  using VideoFrameHandleMap = HashMap<GLuint, scoped_refptr<media::VideoFrame>>;
  // This holds the reference for all video frames being imported, but not
  // yet released.
  VideoFrameHandleMap tex0_to_video_frame_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_
