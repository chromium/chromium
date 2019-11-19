// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_image.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/common/gl_helper.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<viz::ContextProviderCommandBuffer> provider)
    : provider_(std::move(provider)) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {
  provider_->RemoveObserver(this);
}

bool WebGraphicsContext3DProviderImpl::BindToCurrentThread() {
  // TODO(danakj): Could plumb this result out to the caller so they know to
  // retry or not, if any client cared to know if it should retry or not.
  // Call AddObserver here instead of in constructor so that it's called on the
  // correct thread.
  provider_->AddObserver(this);
  return provider_->BindToCurrentThread() == gpu::ContextResult::kSuccess;
}

gpu::InterfaceBase* WebGraphicsContext3DProviderImpl::InterfaceBase() {
  if (ContextGL())
    return ContextGL();
  if (RasterInterface())
    return RasterInterface();
  if (WebGPUInterface())
    return WebGPUInterface();
  return nullptr;
}

gpu::gles2::GLES2Interface* WebGraphicsContext3DProviderImpl::ContextGL() {
  return provider_->ContextGL();
}

gpu::raster::RasterInterface*
WebGraphicsContext3DProviderImpl::RasterInterface() {
  return provider_->RasterInterface();
}

gpu::webgpu::WebGPUInterface*
WebGraphicsContext3DProviderImpl::WebGPUInterface() {
  return provider_->WebGPUInterface();
}

GrContext* WebGraphicsContext3DProviderImpl::GetGrContext() {
  return provider_->GrContext();
}

const gpu::Capabilities& WebGraphicsContext3DProviderImpl::GetCapabilities()
    const {
  return provider_->ContextCapabilities();
}

const gpu::GpuFeatureInfo& WebGraphicsContext3DProviderImpl::GetGpuFeatureInfo()
    const {
  return provider_->GetGpuFeatureInfo();
}

const blink::WebglPreferences&
WebGraphicsContext3DProviderImpl::GetWebglPreferences() const {
  static bool initialized = false;
  static blink::WebglPreferences prefs;
  if (!initialized) {
    initialized = true;
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    auto gpu_feature_info = GetGpuFeatureInfo();

    if (gpu_feature_info.IsWorkaroundEnabled(MAX_MSAA_SAMPLE_COUNT_2))
      prefs.msaa_sample_count = 2;
    else if (gpu_feature_info.IsWorkaroundEnabled(MAX_MSAA_SAMPLE_COUNT_4))
      prefs.msaa_sample_count = 4;

    if (command_line->HasSwitch(switches::kWebglMSAASampleCount)) {
      std::string sample_count =
          command_line->GetSwitchValueASCII(switches::kWebglMSAASampleCount);
      uint32_t count;
      if (base::StringToUint(sample_count, &count)) {
        prefs.msaa_sample_count = count;
      }
    }

    if (command_line->HasSwitch(switches::kWebglAntialiasingMode)) {
      std::string mode =
          command_line->GetSwitchValueASCII(switches::kWebglAntialiasingMode);
      if (mode == "none") {
        prefs.anti_aliasing_mode = blink::kAntialiasingModeNone;
      } else if (mode == "explicit") {
        prefs.anti_aliasing_mode = blink::kAntialiasingModeMSAAExplicitResolve;
      } else if (mode == "implicit") {
        prefs.anti_aliasing_mode = blink::kAntialiasingModeMSAAImplicitResolve;
      } else {
        prefs.anti_aliasing_mode = blink::kAntialiasingModeUnspecified;
      }
    }

    // Set default context limits for WebGL.
#if defined(OS_ANDROID)
    prefs.max_active_webgl_contexts = 8u;
#else
    prefs.max_active_webgl_contexts = 16u;
#endif
    prefs.max_active_webgl_contexts_on_worker = 4u;

    if (command_line->HasSwitch(switches::kMaxActiveWebGLContexts)) {
      std::string max_contexts =
          command_line->GetSwitchValueASCII(switches::kMaxActiveWebGLContexts);
      uint32_t max_val;
      if (base::StringToUint(max_contexts, &max_val)) {
        // It shouldn't be common for users to override this. If they do,
        // just override both values.
        prefs.max_active_webgl_contexts = max_val;
        prefs.max_active_webgl_contexts_on_worker = max_val;
      }
    }
  }

  return prefs;
}

viz::GLHelper* WebGraphicsContext3DProviderImpl::GetGLHelper() {
  if (!gl_helper_) {
    gl_helper_ = std::make_unique<viz::GLHelper>(provider_->ContextGL(),
                                                 provider_->ContextSupport());
  }
  return gl_helper_.get();
}

void WebGraphicsContext3DProviderImpl::SetLostContextCallback(
    base::RepeatingClosure c) {
  context_lost_callback_ = std::move(c);
}

void WebGraphicsContext3DProviderImpl::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> c) {
  provider_->ContextSupport()->SetErrorMessageCallback(std::move(c));
}

void WebGraphicsContext3DProviderImpl::OnContextLost() {
  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

cc::ImageDecodeCache* WebGraphicsContext3DProviderImpl::ImageDecodeCache(
    SkColorType color_type) {
  DCHECK(GetGrContext()->colorTypeSupportedAsImage(color_type));
  auto cache_iterator = image_decode_cache_map_.find(color_type);
  if (cache_iterator != image_decode_cache_map_.end())
    return cache_iterator->second.get();

  // This denotes the allocated GPU memory budget for the cache used for
  // book-keeping. The cache indicates when the total memory locked exceeds this
  // budget in cc::DecodedDrawImage.
  static const size_t kMaxWorkingSetBytes = 64 * 1024 * 1024;

  // TransferCache is used only with OOP raster.
  const bool use_transfer_cache = false;

  auto insertion_result = image_decode_cache_map_.emplace(
      color_type,
      std::make_unique<cc::GpuImageDecodeCache>(
          provider_.get(), use_transfer_cache, color_type, kMaxWorkingSetBytes,
          provider_->ContextCapabilities().max_texture_size,
          cc::PaintImage::kDefaultGeneratorClientId));
  DCHECK(insertion_result.second);
  cache_iterator = insertion_result.first;
  return cache_iterator->second.get();
}

gpu::SharedImageInterface*
WebGraphicsContext3DProviderImpl::SharedImageInterface() {
  return provider_->SharedImageInterface();
}

void WebGraphicsContext3DProviderImpl::CopyVideoFrame(
    media::PaintCanvasVideoRenderer* video_renderer,
    media::VideoFrame* video_frame,
    cc::PaintCanvas* canvas) {
  video_renderer->Copy(video_frame, canvas, context_provider());
}

}  // namespace content
