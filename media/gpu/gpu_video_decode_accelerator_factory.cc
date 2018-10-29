// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_decode_accelerator_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/media_gpu_export.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"
#endif
#if defined(OS_MACOSX)
#include "media/gpu/vt_video_decode_accelerator_mac.h"
#endif
#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_slice_video_decode_accelerator.h"
#include "media/gpu/v4l2/v4l2_video_decode_accelerator.h"
#include "ui/gl/gl_surface_egl.h"
#endif
#if defined(OS_ANDROID)
#include "media/gpu/android/android_video_decode_accelerator.h"
#include "media/gpu/android/android_video_surface_chooser_impl.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/gpu/android/device_info.h"
#endif
#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"
#include "ui/gl/gl_implementation.h"
#endif

namespace media {

namespace {

gpu::VideoDecodeAcceleratorCapabilities GetDecoderCapabilitiesInternal(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  if (gpu_preferences.disable_accelerated_video_decode)
    return gpu::VideoDecodeAcceleratorCapabilities();

  // Query VDAs for their capabilities and construct a set of supported
  // profiles for current platform. This must be done in the same order as in
  // CreateVDA(), as we currently preserve additional capabilities (such as
  // resolutions supported) only for the first VDA supporting the given codec
  // profile (instead of calculating a superset).
  // TODO(posciak,henryhsu): improve this so that we choose a superset of
  // resolutions and other supported profile parameters.
  VideoDecodeAccelerator::Capabilities capabilities;
#if defined(OS_WIN)
  capabilities.supported_profiles =
      DXVAVideoDecodeAccelerator::GetSupportedProfiles(gpu_preferences,
                                                       workarounds);
#elif BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)
  VideoDecodeAccelerator::SupportedProfiles vda_profiles;
#if BUILDFLAG(USE_V4L2_CODEC)
  vda_profiles = V4L2VideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
  vda_profiles = V4L2SliceVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#if BUILDFLAG(USE_VAAPI)
  vda_profiles = VaapiVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#elif defined(OS_MACOSX)
  capabilities.supported_profiles =
      VTVideoDecodeAccelerator::GetSupportedProfiles();
#elif defined(OS_ANDROID)
  capabilities =
      AndroidVideoDecodeAccelerator::GetCapabilities(gpu_preferences);
#endif

  return GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeCapabilities(
      capabilities);
}

}  // namespace

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::Create(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb) {
  return base::WrapUnique(new GpuVideoDecodeAcceleratorFactory(
      get_gl_context_cb, make_context_current_cb, bind_image_cb,
      GetContextGroupCallback(), AndroidOverlayMojoFactoryCB()));
}

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::CreateWithGLES2Decoder(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const GetContextGroupCallback& get_context_group_cb,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb) {
  return base::WrapUnique(new GpuVideoDecodeAcceleratorFactory(
      get_gl_context_cb, make_context_current_cb, bind_image_cb,
      get_context_group_cb, overlay_factory_cb));
}

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::CreateWithNoGL() {
  return Create(GetGLContextCallback(), MakeGLContextCurrentCallback(),
                BindGLImageCallback());
}

// static
MEDIA_GPU_EXPORT gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  // Cache the capabilities so that they will not be computed more than once per
  // GPU process. It is assumed that |gpu_preferences| and |workarounds| do not
  // change between calls.
  // TODO(sandersd): Move cache to GpuMojoMediaClient once
  // |video_decode_accelerator_capabilities| is removed from GPUInfo.
  static const gpu::VideoDecodeAcceleratorCapabilities capabilities =
      GetDecoderCapabilitiesInternal(gpu_preferences, workarounds);
  return capabilities;
}

MEDIA_GPU_EXPORT std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVDA(
    VideoDecodeAccelerator::Client* client,
    const VideoDecodeAccelerator::Config& config,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (gpu_preferences.disable_accelerated_video_decode)
    return nullptr;

  // Array of Create..VDA() function pointers, potentially usable on current
  // platform. This list is ordered by priority, from most to least preferred,
  // if applicable. This list must be in the same order as the querying order
  // in GetDecoderCapabilities() above.
  using CreateVDAFp = std::unique_ptr<VideoDecodeAccelerator> (
      GpuVideoDecodeAcceleratorFactory::*)(const gpu::GpuDriverBugWorkarounds&,
                                           const gpu::GpuPreferences&,
                                           MediaLog* media_log) const;
  const CreateVDAFp create_vda_fps[] = {
#if defined(OS_WIN)
    &GpuVideoDecodeAcceleratorFactory::CreateDXVAVDA,
#endif
#if BUILDFLAG(USE_V4L2_CODEC)
    &GpuVideoDecodeAcceleratorFactory::CreateV4L2VDA,
    &GpuVideoDecodeAcceleratorFactory::CreateV4L2SVDA,
#endif
#if BUILDFLAG(USE_VAAPI)
    &GpuVideoDecodeAcceleratorFactory::CreateVaapiVDA,
#endif
#if defined(OS_MACOSX)
    &GpuVideoDecodeAcceleratorFactory::CreateVTVDA,
#endif
#if defined(OS_ANDROID)
    &GpuVideoDecodeAcceleratorFactory::CreateAndroidVDA,
#endif
  };

  std::unique_ptr<VideoDecodeAccelerator> vda;

  for (const auto& create_vda_function : create_vda_fps) {
    vda = (this->*create_vda_function)(workarounds, gpu_preferences, media_log);
    if (vda && vda->Initialize(config, client))
      return vda;
  }

  return nullptr;
}

#if defined(OS_WIN)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateDXVAVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  DVLOG(0) << "Initializing DXVA HW decoder for windows.";
  decoder.reset(new DXVAVideoDecodeAccelerator(
      get_gl_context_cb_, make_context_current_cb_, bind_image_cb_, workarounds,
      gpu_preferences, media_log));
  return decoder;
}
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateV4L2VDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (device.get()) {
    decoder.reset(new V4L2VideoDecodeAccelerator(
        gl::GLSurfaceEGL::GetHardwareDisplay(), get_gl_context_cb_,
        make_context_current_cb_, device));
  }
  return decoder;
}

std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateV4L2SVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (device.get()) {
    decoder.reset(new V4L2SliceVideoDecodeAccelerator(
        device, gl::GLSurfaceEGL::GetHardwareDisplay(), bind_image_cb_,
        make_context_current_cb_));
  }
  return decoder;
}
#endif

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVaapiVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new VaapiVideoDecodeAccelerator(make_context_current_cb_,
                                                bind_image_cb_));
  return decoder;
}
#endif

#if defined(OS_MACOSX)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVTVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new VTVideoDecodeAccelerator(bind_image_cb_, media_log));
  return decoder;
}
#endif

#if defined(OS_ANDROID)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateAndroidVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new AndroidVideoDecodeAccelerator(
      CodecAllocator::GetInstance(base::ThreadTaskRunnerHandle::Get()),
      std::make_unique<AndroidVideoSurfaceChooserImpl>(
          DeviceInfo::GetInstance()->IsSetOutputSurfaceSupported()),
      make_context_current_cb_, get_context_group_cb_, overlay_factory_cb_,
      DeviceInfo::GetInstance()));
  return decoder;
}
#endif

GpuVideoDecodeAcceleratorFactory::GpuVideoDecodeAcceleratorFactory(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const GetContextGroupCallback& get_context_group_cb,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      get_context_group_cb_(get_context_group_cb),
      overlay_factory_cb_(overlay_factory_cb) {}

GpuVideoDecodeAcceleratorFactory::~GpuVideoDecodeAcceleratorFactory() = default;

}  // namespace media
