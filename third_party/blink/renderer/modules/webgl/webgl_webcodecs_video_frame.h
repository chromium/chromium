// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace media {
class GpuMemoryBufferVideoFramePool;
}  // namespace media

namespace blink {

class WebGLWebCodecsVideoFrameHandle;

class WebGLWebCodecsVideoFrame final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLWebCodecsVideoFrame() override;

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
  void OnHardwareVideoFrameCreated(
      base::WaitableEvent* waitable_event,
      scoped_refptr<media::VideoFrame> video_frame);

  void InitializeGpuMemoryBufferPool();

  std::bitset<media::PIXEL_FORMAT_MAX + 1> formats_supported;
  std::array<std::array<std::string, media::VideoFrame::kMaxPlanes>,
             media::PIXEL_FORMAT_MAX + 1>
      format_to_components_map_;

  using VideoFrameHandleMap = HashMap<GLuint, scoped_refptr<media::VideoFrame>>;
  // This holds the reference for all video frames being imported, but not
  // yet released.
  VideoFrameHandleMap tex0_to_video_frame_map_;

  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<media::VideoFrame> hardware_video_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_WEBCODECS_VIDEO_FRAME_H_
