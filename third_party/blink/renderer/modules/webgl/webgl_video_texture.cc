// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"

#include "build/build_config.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_metadata.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture_enum.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

namespace blink {

WebGLVideoTexture::WebGLVideoTexture(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_WEBGL_video_texture");
}

WebGLExtensionName WebGLVideoTexture::GetName() const {
  return kWebGLVideoTextureName;
}

bool WebGLVideoTexture::Supported(WebGLRenderingContextBase* context) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/776222): support extension on Android
  return false;
#else
  return true;
#endif
}

const char* WebGLVideoTexture::ExtensionName() {
  return "WEBGL_video_texture";
}

void WebGLVideoTexture::Trace(Visitor* visitor) const {
  visitor->Trace(current_frame_metadata_);
  WebGLExtension::Trace(visitor);
}

VideoFrameMetadata* WebGLVideoTexture::shareVideoImageWEBGL(
    ExecutionContext* execution_context,
    unsigned target,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  WebGLExtensionScopedContext scoped(this);
  if (!video || scoped.IsLost())
    return nullptr;

  if (target != GL_TEXTURE_VIDEO_IMAGE_WEBGL) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "WEBGLVideoTexture",
                                        "invalid texture target");
  }

  if (!scoped.Context()->ValidateHTMLVideoElement(
          execution_context->GetSecurityOrigin(), "WEBGLVideoTexture", video,
          exception_state)) {
    return nullptr;
  }

  if (!scoped.Context()->ValidateTexFuncDimensions(
          "WEBGLVideoTexture", WebGLRenderingContextBase::kTexImage, target, 0,
          video->videoWidth(), video->videoHeight(), 1)) {
    return nullptr;
  }

  WebGLTexture* texture =
      scoped.Context()->ValidateTextureBinding("WEBGLVideoTexture", target);
  if (!texture) {
    exception_state.ThrowTypeError(
        "Failed to get correct binding texture for WEBGL_video_texture");
    return nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/776222): support extension on Android
  NOTIMPLEMENTED();
  return nullptr;
#else
  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  scoped_refptr<media::VideoFrame> media_video_frame;
  if (auto* wmp = video->GetWebMediaPlayer()) {
    media_video_frame = wmp->GetCurrentFrameThenUpdate();
    video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!media_video_frame || !video_renderer)
    return nullptr;

  // For WebGL last-uploaded-frame-metadata API.
  auto metadata = CreateVideoFrameUploadMetadata(
      media_video_frame.get(), texture->GetLastUploadedVideoFrameId());
  if (metadata.skipped) {
    texture->UpdateLastUploadedFrame(metadata);
    DCHECK(current_frame_metadata_);
    return current_frame_metadata_;
  }

  target = GL_TEXTURE_2D;

  viz::RasterContextProvider* raster_context_provider = nullptr;
  if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
    if (auto* context_provider = wrapper->ContextProvider())
      raster_context_provider = context_provider->RasterContextProvider();
  }

  // TODO(shaobo.yan@intel.com) : A fallback path or exception needs to be
  // added when video is not using gpu decoder.
  const bool success = video_renderer->PrepareVideoFrameForWebGL(
      raster_context_provider, scoped.Context()->ContextGL(),
      std::move(media_video_frame), target, texture->Object());
  if (!success) {
    exception_state.ThrowTypeError("Failed to share video to texture.");
    return nullptr;
  }

  if (RuntimeEnabledFeatures::ExtraWebGLVideoTextureMetadataEnabled())
    texture->UpdateLastUploadedFrame(metadata);

  if (!current_frame_metadata_)
    current_frame_metadata_ = VideoFrameMetadata::Create();

  // TODO(crbug.com/776222): These should be read from the VideoFrameCompositor
  // when the VideoFrame is retrieved in WebMediaPlayerImpl. These fields are
  // not currently saved in VideoFrameCompositor, so VFC::ProcessNewFrame()
  // would need to save the current time as well as the presentation time.
  current_frame_metadata_->setPresentationTime(
      metadata.timestamp.InMicrosecondsF());
  current_frame_metadata_->setExpectedDisplayTime(
      metadata.expected_timestamp.InMicrosecondsF());

  current_frame_metadata_->setWidth(metadata.visible_rect.width());
  current_frame_metadata_->setHeight(metadata.visible_rect.height());
  current_frame_metadata_->setMediaTime(metadata.timestamp.InSecondsF());

  // This is a required field. It is supposed to be monotonically increasing for
  // video.requestVideoFrameCallback, but it isn't used yet for
  // WebGLVideoTexture.
  current_frame_metadata_->setPresentedFrames(0);
  return current_frame_metadata_;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool WebGLVideoTexture::releaseVideoImageWEBGL(
    ExecutionContext* execution_context,
    unsigned target,
    ExceptionState& exception_state) {
  // NOTE: In current WEBGL_video_texture status, there is no lock on video
  // frame. So this API doesn't need to do anything.
  return true;
}

// static
WebGLVideoFrameUploadMetadata WebGLVideoTexture::CreateVideoFrameUploadMetadata(
    const media::VideoFrame* frame,
    media::VideoFrame::ID already_uploaded_id) {
  DCHECK(frame);
  WebGLVideoFrameUploadMetadata metadata = {};
  if (!RuntimeEnabledFeatures::ExtraWebGLVideoTextureMetadataEnabled())
    return metadata;

  metadata.frame_id = frame->unique_id();
  metadata.visible_rect = frame->visible_rect();
  metadata.timestamp = frame->timestamp();
  if (frame->metadata().frame_duration.has_value()) {
    metadata.expected_timestamp =
        frame->timestamp() + *frame->metadata().frame_duration;
  };

  // Skip uploading frames which have already been uploaded.
  if (already_uploaded_id == frame->unique_id())
    metadata.skipped = true;
  return metadata;
}

}  // namespace blink
