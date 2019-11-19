/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_

#include <cstdint>
#include "base/callback_forward.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class GrContext;

namespace cc {
class ImageDecodeCache;
class PaintCanvas;
}  // namespace cc

namespace media {
class PaintCanvasVideoRenderer;
class VideoFrame;
}  // namespace media

namespace gpu {
struct Capabilities;
struct GpuFeatureInfo;
class InterfaceBase;
class SharedImageInterface;

namespace gles2 {
class GLES2Interface;
}

namespace raster {
class RasterInterface;
}

namespace webgpu {
class WebGPUInterface;
}
}

namespace viz {
class GLHelper;
}

namespace blink {
enum AntialiasingMode {
  kAntialiasingModeUnspecified,
  kAntialiasingModeNone,
  kAntialiasingModeMSAAImplicitResolve,
  kAntialiasingModeMSAAExplicitResolve,
};

struct WebglPreferences {
  AntialiasingMode anti_aliasing_mode = kAntialiasingModeUnspecified;
  uint32_t msaa_sample_count = 8;
  uint32_t eqaa_storage_sample_count = 4;
  // WebGL-specific numeric limits.
  uint32_t max_active_webgl_contexts = 0;
  uint32_t max_active_webgl_contexts_on_worker = 0;
};

class WebGraphicsContext3DProvider {
 public:
  virtual ~WebGraphicsContext3DProvider() = default;

  virtual gpu::InterfaceBase* InterfaceBase() = 0;
  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;
  virtual gpu::raster::RasterInterface* RasterInterface() = 0;
  virtual gpu::webgpu::WebGPUInterface* WebGPUInterface() = 0;
  virtual bool BindToCurrentThread() = 0;
  virtual GrContext* GetGrContext() = 0;
  virtual const gpu::Capabilities& GetCapabilities() const = 0;
  virtual const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const = 0;
  virtual const WebglPreferences& GetWebglPreferences() const = 0;
  // Creates a viz::GLHelper after first call and returns that instance. This
  // method cannot return null.
  virtual viz::GLHelper* GetGLHelper() = 0;

  virtual void SetLostContextCallback(base::RepeatingClosure) = 0;
  virtual void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char* msg, int32_t id)>) = 0;
  // Return a static software image decode cache for a given color type.
  virtual cc::ImageDecodeCache* ImageDecodeCache(SkColorType color_type) = 0;
  virtual gpu::SharedImageInterface* SharedImageInterface() = 0;
  virtual void CopyVideoFrame(media::PaintCanvasVideoRenderer* video_render,
                              media::VideoFrame* video_frame,
                              cc::PaintCanvas* canvas) = 0;
};

}  // namespace blink

#endif
