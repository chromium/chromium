// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_webcodecs_video_frame.h"

#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_webcodecs_texture_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_webcodecs_video_frame_handle.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

#if BUILDFLAG(IS_WIN)
const char kRequiredExtension[] = "GL_NV_EGL_stream_consumer_external";
#elif BUILDFLAG(IS_MAC)
const char kRequiredExtension[] = "GL_ANGLE_texture_rectangle";
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
const char kRequiredExtension[] = "GL_OES_EGL_image_external";
#else
const char kRequiredExtension[] = "";
#define NO_REQUIRED_EXTENSIONS
#endif

void GetMediaTaskRunnerAndGpuFactoriesOnMainThread(
    scoped_refptr<base::SequencedTaskRunner>* media_task_runner_out,
    media::GpuVideoAcceleratorFactories** gpu_factories_out,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  *media_task_runner_out = Platform::Current()->MediaThreadTaskRunner();
  *gpu_factories_out = Platform::Current()->GetGpuFactories();
  waitable_event->Signal();
}

}  // namespace

WebGLWebCodecsVideoFrame::WebGLWebCodecsVideoFrame(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ContextGL()->RequestExtensionCHROMIUM(kRequiredExtension);

#if BUILDFLAG(IS_MAC)
  // texture_rectangle needs to be turned on for MAC.
  context->ContextGL()->Enable(GC3D_TEXTURE_RECTANGLE_ARB);
#endif

#if BUILDFLAG(IS_WIN)
  formats_supported[media::PIXEL_FORMAT_NV12] = true;
  auto& components_nv12 = format_to_components_map_[media::PIXEL_FORMAT_NV12];
  components_nv12[media::VideoFrame::kYPlane] = "r";
  components_nv12[media::VideoFrame::kUPlane] = "rg";
#elif BUILDFLAG(IS_MAC)
  formats_supported[media::PIXEL_FORMAT_XRGB] = true;
  auto& components_xrgb = format_to_components_map_[media::PIXEL_FORMAT_XRGB];
  components_xrgb[media::VideoFrame::kYPlane] = "rgba";
#elif BUILDFLAG(IS_ANDROID)
  formats_supported[media::PIXEL_FORMAT_ABGR] = true;
  auto& components_abgr = format_to_components_map_[media::PIXEL_FORMAT_ABGR];
  components_abgr[media::VideoFrame::kYPlane] = "rgb";

  // GpuMemoryBufferVideoFramePool
  formats_supported[media::PIXEL_FORMAT_NV12] = true;
  auto& components_nv12 = format_to_components_map_[media::PIXEL_FORMAT_NV12];
  components_nv12[media::VideoFrame::kYPlane] = "r";
  components_nv12[media::VideoFrame::kUPlane] = "rg";
#elif BUILDFLAG(IS_CHROMEOS)
  formats_supported[media::PIXEL_FORMAT_ABGR] = true;
  auto& components_abgr = format_to_components_map_[media::PIXEL_FORMAT_ABGR];
  components_abgr[media::VideoFrame::kYPlane] = "rgb";
#endif
}

WebGLWebCodecsVideoFrame::~WebGLWebCodecsVideoFrame() {
  if (gpu_memory_buffer_pool_) {
    media_task_runner_->DeleteSoon(FROM_HERE,
                                   std::move(gpu_memory_buffer_pool_));
  }
}

WebGLExtensionName WebGLWebCodecsVideoFrame::GetName() const {
  return kWebGLWebCodecsVideoFrameName;
}

bool WebGLWebCodecsVideoFrame::Supported(WebGLRenderingContextBase* context) {
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_FUCHSIA)
  // TODO(jie.a.chen@intel.com): Add Linux support.
  return false;
#elif BUILDFLAG(IS_MAC)
  // This extension is only supported on the passthrough command
  // decoder on macOS.
  DrawingBuffer* drawing_buffer = context->GetDrawingBuffer();
  if (!drawing_buffer)
    return false;
  return drawing_buffer->GetGraphicsInfo().using_passthrough_command_decoder;
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

  const char* sampler_type = "sampler2D";
  const char* sampler_func = "texture2D";
  gfx::ColorSpace src_color_space = gfx::ColorSpace::CreateREC709();
  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_UNKNOWN;
#if BUILDFLAG(IS_WIN)
  sampler_type = "samplerExternalOES";
  pixel_format = media::PIXEL_FORMAT_NV12;
#elif BUILDFLAG(IS_MAC)
  sampler_type = "sampler2DRect";
  sampler_func = "texture2DRect";
  pixel_format = media::PIXEL_FORMAT_XRGB;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  sampler_type = "samplerExternalOES";
  pixel_format = media::PIXEL_FORMAT_ABGR;
  src_color_space = gfx::ColorSpace::CreateSRGB();
#endif

  scoped_refptr<media::VideoFrame> frame = video_frame->frame();
  if (!frame->HasTextures()) {
    InitializeGpuMemoryBufferPool();
    base::WaitableEvent waitable_event;
    media_task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(
            &media::GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame,
            base::Unretained(gpu_memory_buffer_pool_.get()),
            base::RetainedRef(frame),
            WTF::BindOnce(
                &WebGLWebCodecsVideoFrame::OnHardwareVideoFrameCreated,
                WrapWeakPersistent(this), WTF::Unretained(&waitable_event))));
    waitable_event.Wait();

    if (frame == hardware_video_frame_) {
      exception_state.ThrowTypeError(
          "Unable to import a software video frame.");
      return nullptr;
    }
    frame = std::move(hardware_video_frame_);
#if BUILDFLAG(IS_WIN)
    sampler_type = "sampler2D";
#elif BUILDFLAG(IS_ANDROID)
    sampler_type = "sampler2D";
    pixel_format = frame->format();
    src_color_space = frame->ColorSpace();
#endif
  }

  if (!formats_supported[pixel_format]) {
    exception_state.ThrowTypeError(
        String::Format("VideoPixelFormat:%s is not supported yet.",
                       media::VideoPixelFormatToString(pixel_format).c_str()));
    return nullptr;
  }

  const auto& components = format_to_components_map_[pixel_format];
  HeapVector<Member<WebGLWebCodecsTextureInfo>> info_array;
  for (size_t tex = 0; tex < frame->NumTextures(); ++tex) {
    WebGLWebCodecsTextureInfo* texture_info =
        MakeGarbageCollected<WebGLWebCodecsTextureInfo>();
    info_array.push_back(texture_info);
    texture_info->setSamplerType(sampler_type);
    texture_info->setSamplerFunc(sampler_func);
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
#ifndef NO_REQUIRED_EXTENSIONS
  video_frame_handle->setRequiredExtension(kRequiredExtension);
#endif
  // Remove "PIXEL_FORMAT_" prefix
  auto&& video_pixel_format =
      V8VideoPixelFormat::Create(&VideoPixelFormatToString(pixel_format)[13]);
  DCHECK(video_pixel_format);
  video_frame_handle->setPixelFormat(*video_pixel_format);

  // TODO(jie.a.chen@intel.com): Is the colorspace/flip-y/pre-alpha of video
  // frame specific to OS only? For the same OS, does it vary for different
  // video streams?
  video_frame_handle->setFlipY(true);
  video_frame_handle->setPremultipliedAlpha(false);
#if BUILDFLAG(IS_WIN)
  DCHECK(frame->format() == media::PIXEL_FORMAT_NV12);
  src_color_space = frame->ColorSpace();
#elif BUILDFLAG(IS_MAC)
  video_frame_handle->setRequiredExtension("GL_ARB_texture_rectangle");
  video_frame_handle->setPremultipliedAlpha(true);
  src_color_space = frame->ColorSpace();
  src_color_space = src_color_space.GetAsFullRangeRGB();
#endif
  VideoColorSpace* video_frame_color_space =
      MakeGarbageCollected<VideoColorSpace>(src_color_space);
  video_frame_handle->setColorSpace(video_frame_color_space);

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
  WebGLExtensionScopedContext scoped(this);
  auto* gl = scoped.Context()->ContextGL();
  auto& info_array = handle->textureInfoArray();
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

void WebGLWebCodecsVideoFrame::OnHardwareVideoFrameCreated(
    base::WaitableEvent* waitable_event,
    scoped_refptr<media::VideoFrame> video_frame) {
  hardware_video_frame_ = std::move(video_frame);
  waitable_event->Signal();
}

void WebGLWebCodecsVideoFrame::InitializeGpuMemoryBufferPool() {
  if (!worker_task_runner_) {
    worker_task_runner_ = worker_pool::CreateSequencedTaskRunner({});
  }
  if (!gpu_memory_buffer_pool_) {
    media::GpuVideoAcceleratorFactories* gpu_factories = nullptr;
    if (IsMainThread()) {
      media_task_runner_ = Platform::Current()->MediaThreadTaskRunner();
      gpu_factories = Platform::Current()->GetGpuFactories();
    } else {
      base::WaitableEvent waitable_event;
      // TODO(crbug.com/1164152): Lift the main thread restriction.
      if (PostCrossThreadTask(
              *Thread::MainThread()->GetTaskRunner(
                  MainThreadTaskRunnerRestricted()),
              FROM_HERE,
              CrossThreadBindOnce(
                  &GetMediaTaskRunnerAndGpuFactoriesOnMainThread,
                  CrossThreadUnretained(&media_task_runner_),
                  CrossThreadUnretained(&gpu_factories),
                  CrossThreadUnretained(&waitable_event)))) {
        waitable_event.Wait();
      }
    }
    gpu_memory_buffer_pool_ =
        std::make_unique<media::GpuMemoryBufferVideoFramePool>(
            media_task_runner_, worker_task_runner_, gpu_factories);
  }
}

}  // namespace blink
