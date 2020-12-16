// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_webcodecs_video_frame.h"

#include "build/build_config.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_webcodecs_texture_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_webcodecs_video_frame_handle.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/color_transform.h"

namespace blink {

namespace {

#if defined(OS_WIN)
const char* kRequiredExtension = "GL_NV_EGL_stream_consumer_external";
#elif defined(OS_MAC)
const char* kRequiredExtension = "GL_ANGLE_texture_rectangle";
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
const char* kRequiredExtension = "GL_OES_EGL_image_external";
#else
const char* kRequiredExtension = "";
#endif

}  // namespace

WebGLWebCodecsVideoFrame::WebGLWebCodecsVideoFrame(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ContextGL()->RequestExtensionCHROMIUM(kRequiredExtension);

#if defined(OS_MAC)
  // texture_rectangle needs to be turned on for MAC.
  context->ContextGL()->Enable(GC3D_TEXTURE_RECTANGLE_ARB);
#endif

  // TODO(jie.a.chen@intel.com): More supports for HDR video.
#if defined(OS_WIN)
  sampler_type_ = "samplerExternalOES";
  sampler_func_ = "texture2D";
  formats_supported[media::PIXEL_FORMAT_NV12] = true;
  auto& components = format_to_components_map_[media::PIXEL_FORMAT_NV12];
  components[media::VideoFrame::kYPlane] = "r";
  components[media::VideoFrame::kUPlane] = "rg";
#elif defined(OS_MAC)
  sampler_type_ = "sampler2DRect";
  sampler_func_ = "texture2DRect";
  formats_supported[media::PIXEL_FORMAT_XRGB] = true;
  auto& components = format_to_components_map_[media::PIXEL_FORMAT_XRGB];
  components[media::VideoFrame::kYPlane] = "rgba";
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  sampler_type_ = "samplerExternalOES";
  sampler_func_ = "texture2D";
  formats_supported[media::PIXEL_FORMAT_ABGR] = true;
  auto& components = format_to_components_map_[media::PIXEL_FORMAT_ABGR];
  components[media::VideoFrame::kYPlane] = "rgb";
#endif
}

WebGLExtensionName WebGLWebCodecsVideoFrame::GetName() const {
  return kWebGLWebCodecsVideoFrameName;
}

bool WebGLWebCodecsVideoFrame::Supported(WebGLRenderingContextBase* context) {
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
  // TODO(jie.a.chen@intel.com): Add Linux support.
  return false;
#else
  return true;
#endif
}

const char* WebGLWebCodecsVideoFrame::ExtensionName() {
  return "WEBGL_webcodecs_video_frame";
}

void WebGLWebCodecsVideoFrame::Trace(Visitor* visitor) const {
  WebGLExtension::Trace(visitor);
}

WebGLWebCodecsVideoFrameHandle* WebGLWebCodecsVideoFrame::importVideoFrame(
    ExecutionContext* execution_context,
    VideoFrame* video_frame,
    ExceptionState& exception_state) {
  WebGLExtensionScopedContext scoped(this);
  if (!video_frame || scoped.IsLost())
    return nullptr;

  scoped_refptr<media::VideoFrame> frame = video_frame->frame();
  if (!frame->HasTextures()) {
    exception_state.ThrowTypeError("Unable to import a software video frame.");
    return nullptr;
  }

  if (!formats_supported[frame->format()]) {
    std::ostringstream ss;
    ss << "VideoPixelFormat:"
       << media::VideoPixelFormatToString(frame->format())
       << " is not supported yet.";
    exception_state.ThrowTypeError(ss.str().c_str());
    return nullptr;
  }

  const auto& components = format_to_components_map_[frame->format()];
  HeapVector<Member<WebGLWebCodecsTextureInfo>> info_array;
  for (size_t tex = 0; tex < frame->NumTextures(); ++tex) {
    WebGLWebCodecsTextureInfo* texture_info =
        MakeGarbageCollected<WebGLWebCodecsTextureInfo>();
    info_array.push_back(texture_info);
    texture_info->setSamplerType(sampler_type_.c_str());
    texture_info->setSamplerFunc(sampler_func_.c_str());
    texture_info->setComponents(components[tex].c_str());

    auto* gl = scoped.Context()->ContextGL();
    auto& mailbox_holder = frame->mailbox_holder(tex);
    gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    GLuint texture_id = 0;
    if (mailbox_holder.mailbox.IsSharedImage()) {
      texture_id = gl->CreateAndTexStorage2DSharedImageCHROMIUM(
          mailbox_holder.mailbox.name);
      gl->BeginSharedImageAccessDirectCHROMIUM(
          texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    } else {
      texture_id =
          gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
    }
    DCHECK_NE(texture_id, 0u);
    auto texture_target = frame->mailbox_holder(tex).texture_target;
    WebGLUnownedTexture* texture = MakeGarbageCollected<WebGLUnownedTexture>(
        scoped.Context(), texture_id, texture_target);
    texture_info->setTexture(texture);
    texture_info->setTarget(texture_target);
  }

  WebGLWebCodecsVideoFrameHandle* video_frame_handle =
      MakeGarbageCollected<WebGLWebCodecsVideoFrameHandle>();
  video_frame_handle->setTextureInfoArray(info_array);
  if (std::string(kRequiredExtension) != "") {
    video_frame_handle->setRequiredExtension(kRequiredExtension);
  }
  // TODO(jie.a.chen@intel.com): Is the colorspace/flip-y/pre-alpha of video
  // frame specific to OS only? For the same OS, does it vary for different
  // video streams?
  video_frame_handle->setFlipY(true);
  video_frame_handle->setPremultipliedAlpha(false);
  gfx::ColorSpace src_color_space = frame->ColorSpace();
#if defined(OS_WIN)
  video_frame_handle->setPixelFormat("NV12");
#elif defined(OS_MAC)
  video_frame_handle->setRequiredExtension("GL_ARB_texture_rectangle");
  video_frame_handle->setPixelFormat("XRGB");
  video_frame_handle->setPremultipliedAlpha(true);
  src_color_space = src_color_space.GetAsFullRangeRGB();
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  if (!frame->ColorSpace().IsValid()) {
    video_frame_handle->setPixelFormat("ABGR");
    src_color_space = gfx::ColorSpace::CreateSRGB();
  }
#endif
  VideoColorSpace* video_frame_color_space =
      MakeGarbageCollected<VideoColorSpace>();
  // TODO(jie.a.chen@intel.com): Add ToString() for color space members.
#if defined(OS_WIN)
  video_frame_color_space->setPrimaryID("BT709");
  video_frame_color_space->setTransferID("BT709");
  video_frame_color_space->setMatrixID("BT709");
  video_frame_color_space->setRangeID("LIMITED");
#elif defined(OS_MAC)
  video_frame_color_space->setPrimaryID("SMPTE240M");
  // TODO(jie.a.chen@intel.com): The actual BT709_APPLE is not available.
  video_frame_color_space->setTransferID("BT709");
  video_frame_color_space->setMatrixID("RGB");
  video_frame_color_space->setRangeID("FULL");
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  video_frame_color_space->setPrimaryID("BT709");
  video_frame_color_space->setTransferID("IEC61966_2_1");
  video_frame_color_space->setMatrixID("RGB");
  video_frame_color_space->setRangeID("FULL");
#endif
  video_frame_handle->setColorSpace(video_frame_color_space);

  gfx::ColorSpace dst_color_space = gfx::ColorSpace::CreateSRGB();
  std::unique_ptr<gfx::ColorTransform> color_transform(
      gfx::ColorTransform::NewColorTransform(
          src_color_space, dst_color_space,
          gfx::ColorTransform::Intent::INTENT_ABSOLUTE));
  video_frame_handle->setColorConversionShaderFunc(
      color_transform->GetShaderSource().c_str());

  // Bookkeeping of imported video frames.
  GLuint tex0 = info_array[0]->texture()->Object();
  tex0_to_video_frame_map_.insert(tex0, frame);

  return video_frame_handle;
}

bool WebGLWebCodecsVideoFrame::releaseVideoFrame(
    ExecutionContext* execution_context,
    WebGLWebCodecsVideoFrameHandle* handle,
    ExceptionState& exception_state) {
  DCHECK(handle);
  DCHECK(handle->hasTextureInfoArrayNonNull());
  WebGLExtensionScopedContext scoped(this);
  auto* gl = scoped.Context()->ContextGL();
  auto& info_array = handle->textureInfoArrayNonNull();
  GLuint tex0 = info_array[0]->texture()->Object();
  auto frame = tex0_to_video_frame_map_.Take(tex0);
  for (wtf_size_t i = 0; i < info_array.size(); ++i) {
    auto mailbox = frame->mailbox_holder(i).mailbox;
    GLuint texture = info_array[i]->texture()->Object();
    DCHECK_NE(texture, 0u);
    if (mailbox.IsSharedImage()) {
      gl->EndSharedImageAccessDirectCHROMIUM(texture);
    }
    gl->DeleteTextures(1, &texture);
  }
  media::WaitAndReplaceSyncTokenClient client(gl);
  frame->UpdateReleaseSyncToken(&client);
  return true;
}

}  // namespace blink
