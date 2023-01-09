// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_decode_accelerator_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/media_gpu_export.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"
#endif
#if BUILDFLAG(IS_MAC)
#include "media/gpu/mac/vt_video_decode_accelerator_mac.h"
#endif
#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"
#include "ui/gl/gl_implementation.h"
#elif BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_slice_video_decode_accelerator.h"
#include "media/gpu/v4l2/v4l2_video_decode_accelerator.h"
#include "ui/gl/gl_surface_egl.h"
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
#if BUILDFLAG(IS_WIN)
  capabilities.supported_profiles =
      DXVAVideoDecodeAccelerator::GetSupportedProfiles(gpu_preferences,
                                                       workarounds);
#elif BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#if BUILDFLAG(USE_VAAPI)
  capabilities.supported_profiles =
      VaapiVideoDecodeAccelerator::GetSupportedProfiles();
#elif BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      V4L2VideoDecodeAccelerator::GetSupportedProfiles(),
      &capabilities.supported_profiles);
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      V4L2SliceVideoDecodeAccelerator::GetSupportedProfiles(),
      &capabilities.supported_profiles);
#endif
#elif BUILDFLAG(IS_MAC)
  capabilities.supported_profiles =
      VTVideoDecodeAccelerator::GetSupportedProfiles(workarounds);
#endif

  return GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeCapabilities(
      capabilities);
}

}  // namespace

// static
MEDIA_GPU_EXPORT std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
GpuVideoDecodeAcceleratorFactory::Create(
    const GpuVideoDecodeGLClient& gl_client) {
  return base::WrapUnique(new GpuVideoDecodeAcceleratorFactory(gl_client));
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
  static gpu::VideoDecodeAcceleratorCapabilities capabilities =
      GetDecoderCapabilitiesInternal(gpu_preferences, workarounds);

#if BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
  // V4L2-only: the decoder devices may not be visible at the time the GPU
  // process is starting. If the capabilities vector is empty, try to query the
  // devices again in the hope that they will have appeared in the meantime.
  // TODO(crbug.com/948147): trigger query when an device add/remove event
  // (e.g. via udev) has happened instead.
  if (capabilities.supported_profiles.empty()) {
    VLOGF(1) << "Capabilities empty, querying again...";
    capabilities = GetDecoderCapabilitiesInternal(gpu_preferences, workarounds);
  }
#endif

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
#if BUILDFLAG(IS_WIN)
    &GpuVideoDecodeAcceleratorFactory::CreateDXVAVDA,
#endif

  // Usually only one of USE_VAAPI or USE_V4L2_CODEC is defined on ChromeOS,
  // except for Chromeboxes with companion video acceleration chips, which have
  // both. In those cases prefer the VA creation function.
#if BUILDFLAG(USE_VAAPI)
    &GpuVideoDecodeAcceleratorFactory::CreateVaapiVDA,
#elif BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
    &GpuVideoDecodeAcceleratorFactory::CreateV4L2VDA,
    &GpuVideoDecodeAcceleratorFactory::CreateV4L2SliceVDA,
#endif

#if BUILDFLAG(IS_MAC)
    &GpuVideoDecodeAcceleratorFactory::CreateVTVDA,
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

#if BUILDFLAG(IS_WIN)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateDXVAVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  DVLOG(0) << "Initializing DXVA HW decoder for windows.";
  decoder.reset(new DXVAVideoDecodeAccelerator(
      gl_client_.get_context, gl_client_.make_context_current,
      gl_client_.bind_image, workarounds, gpu_preferences, media_log));
  return decoder;
}
#endif

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVaapiVDA(
    const gpu::GpuDriverBugWorkarounds& /*workarounds*/,
    const gpu::GpuPreferences& /*gpu_preferences*/,
    MediaLog* /*media_log*/) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(new VaapiVideoDecodeAccelerator(gl_client_.make_context_current,
                                                gl_client_.bind_image));
  return decoder;
}
#elif BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateV4L2VDA(
    const gpu::GpuDriverBugWorkarounds& /*workarounds*/,
    const gpu::GpuPreferences& /*gpu_preferences*/,
    MediaLog* /*media_log*/) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (device.get()) {
    decoder.reset(new V4L2VideoDecodeAccelerator(
        gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
        gl_client_.get_context, gl_client_.make_context_current, device));
  }
  return decoder;
}

std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateV4L2SliceVDA(
    const gpu::GpuDriverBugWorkarounds& /*workarounds*/,
    const gpu::GpuPreferences& /*gpu_preferences*/,
    MediaLog* /*media_log*/) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (device.get()) {
    decoder.reset(new V4L2SliceVideoDecodeAccelerator(
        device, gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
        gl_client_.bind_image, gl_client_.make_context_current));
  }
  return decoder;
}
#endif

#if BUILDFLAG(IS_MAC)
std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVTVDA(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log) const {
  std::unique_ptr<VideoDecodeAccelerator> decoder;
  decoder.reset(
      new VTVideoDecodeAccelerator(gl_client_, workarounds, media_log));
  return decoder;
}
#endif

GpuVideoDecodeAcceleratorFactory::GpuVideoDecodeAcceleratorFactory(
    const GpuVideoDecodeGLClient& gl_client)
    : gl_client_(gl_client) {}
GpuVideoDecodeAcceleratorFactory::~GpuVideoDecodeAcceleratorFactory() = default;

}  // namespace media
