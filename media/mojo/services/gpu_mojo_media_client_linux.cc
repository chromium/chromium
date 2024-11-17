// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "ui/gfx/buffer_types.h"

namespace media {

namespace {

BASE_FEATURE(kAcceleratedVideoDecodeLinuxZeroCopyGL,
             "AcceleratedVideoDecodeLinuxZeroCopyGL",
             base::FEATURE_DISABLED_BY_DEFAULT);

VideoDecoderType GetPreferredLinuxDecoderImplementation() {
  // VaapiVideoDecoder flag is required for VaapiVideoDecoder.
  if (!base::FeatureList::IsEnabled(kAcceleratedVideoDecodeLinux)) {
    return VideoDecoderType::kUnknown;
  }

  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
      return VideoDecoderType::kOutOfProcess;
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy:
      // The browser process ensures that this path is never reached for this
      // OOP-VD mode.
      NOTREACHED();
    case media::OOPVDMode::kDisabled:
      break;
  }

#if BUILDFLAG(USE_VAAPI)
  return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
  return VideoDecoderType::kV4L2;
#endif
}

std::vector<Fourcc> GetPreferredRenderableFourccs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  std::vector<Fourcc> renderable_fourccs;
#if BUILDFLAG(ENABLE_VULKAN)
  // Support for zero-copy NV12/P010 textures preferentially.
  if (gpu_preferences.gr_context_type == gpu::GrContextType::kVulkan) {
    renderable_fourccs.emplace_back(Fourcc::NV12);
    renderable_fourccs.emplace_back(Fourcc::P010);
  } else
#endif  // BUILDFLAG(ENABLE_VULKAN)
#if BUILDFLAG(IS_OZONE)
    // Allow zero-copy formats with GL for testing or in controlled
    // environments.
    if (gpu_preferences.gr_context_type == gpu::GrContextType::kGL &&
        base::FeatureList::IsEnabled(kAcceleratedVideoDecodeLinuxZeroCopyGL)) {
      // Importing NV12 and P010 buffers requires EGL_EXT_image_dma_buf_import.
      // GLX can only import native pixmap of format AR24. Ozone expose this
      // capability through gpu_feature_info so we can selectively allow hw
      // accelerated formats.
      if (base::Contains(
              gpu_feature_info
                  .supported_buffer_formats_for_gl_native_pixmap_import,
              gfx::BufferFormat::YUV_420_BIPLANAR)) {
        renderable_fourccs.emplace_back(Fourcc::NV12);
      }
      if (base::Contains(
              gpu_feature_info
                  .supported_buffer_formats_for_gl_native_pixmap_import,
              gfx::BufferFormat::P010)) {
        renderable_fourccs.emplace_back(Fourcc::P010);
      }
    }
#endif  // BUILDFLAG(IS_OZONE)

  // Support 1-copy argb textures.
  //
  // TODO(crbug.com/349428388): For VP9 Profile2 and HEVC Main 10 10-bit video,
  // the current implementation requires additional VPP to convert the NV12/P010
  // format to a renderable format AR24. While this VPP introduces a loss of
  // color depth (P010 -> AR24), it should be optimized for zero-copy path in
  // the future.
  renderable_fourccs.emplace_back(Fourcc::AR24);

  return renderable_fourccs;
}

VideoDecoderType GetActualPlatformDecoderImplementation(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  // On linux, Vaapi and V4L2 have GL restrictions.
  switch (GetPreferredLinuxDecoderImplementation()) {
    case VideoDecoderType::kUnknown:
      return VideoDecoderType::kUnknown;
    case VideoDecoderType::kOutOfProcess:
      return VideoDecoderType::kOutOfProcess;
    case VideoDecoderType::kV4L2:
      if (gpu_preferences.gr_context_type == gpu::GrContextType::kGL) {
        if (base::FeatureList::IsEnabled(kAcceleratedVideoDecodeLinuxGL)) {
          return VideoDecoderType::kV4L2;
        } else {
          return VideoDecoderType::kUnknown;
        }
      }
      return VideoDecoderType::kV4L2;
    case VideoDecoderType::kVaapi: {
      // Allow VaapiVideoDecoder on GL.
      if (gpu_preferences.gr_context_type == gpu::GrContextType::kGL) {
        if (base::FeatureList::IsEnabled(kAcceleratedVideoDecodeLinuxGL)) {
          return VideoDecoderType::kVaapi;
        } else {
          return VideoDecoderType::kUnknown;
        }
      }
#if BUILDFLAG(ENABLE_VULKAN)
      if (gpu_preferences.gr_context_type != gpu::GrContextType::kVulkan) {
        return VideoDecoderType::kUnknown;
      }
      if (!base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
        return VideoDecoderType::kUnknown;
      }
      if (!base::FeatureList::IsEnabled(features::kDefaultANGLEVulkan)) {
        return VideoDecoderType::kUnknown;
      }
      // If Vulkan is active, check Vulkan info if VaapiVideoDecoder is allowed.
      if (!gpu_info.vulkan_info.has_value()) {
        return VideoDecoderType::kUnknown;
      }
      if (gpu_info.vulkan_info->physical_devices.empty()) {
        return VideoDecoderType::kUnknown;
      }
      constexpr int kIntel = 0x8086;
      const auto& device = gpu_info.vulkan_info->physical_devices[0];
      switch (device.properties.vendorID) {
        case kIntel: {
          if (device.properties.driverVersion < VK_MAKE_VERSION(21, 1, 5)) {
            return VideoDecoderType::kUnknown;
          }
          return VideoDecoderType::kVaapi;
        }
        default: {
          // NVIDIA drivers have a broken implementation of most va_* methods,
          // ARM & AMD aren't tested yet, and ImgTec/Qualcomm don't have a vaapi
          // driver.
          if (base::FeatureList::IsEnabled(kVaapiIgnoreDriverChecks)) {
            return VideoDecoderType::kVaapi;
          }
          return VideoDecoderType::kUnknown;
        }
      }
#else
      return VideoDecoderType::kUnknown;
#endif  // BUILDFLAG(ENABLE_VULKAN)
    }
    default:
      return VideoDecoderType::kUnknown;
  }
}

}  // namespace

class GpuMojoMediaClientLinux final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientLinux(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {}
  ~GpuMojoMediaClientLinux() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    const auto decoder_type =
        GetActualPlatformDecoderImplementation(gpu_preferences_, gpu_info_);
    // The browser process guarantees this CHECK.
    CHECK_EQ(!!traits.oop_video_decoder,
             (decoder_type == VideoDecoderType::kOutOfProcess));

    switch (decoder_type) {
      case VideoDecoderType::kOutOfProcess: {
        // TODO(b/195769334): for out-of-process video decoding, we don't need a
        // |frame_pool| because the buffers will be allocated and managed
        // out-of-process.
        auto frame_pool = std::make_unique<PlatformVideoFramePool>();

        auto frame_converter = MailboxVideoFrameConverter::Create(
            gpu_task_runner_, traits.get_command_buffer_stub_cb);
        return VideoDecoderPipeline::Create(
            gpu_workarounds_, traits.task_runner, std::move(frame_pool),
            std::move(frame_converter),
            GetPreferredRenderableFourccs(gpu_preferences_, gpu_feature_info_),
            traits.media_log->Clone(), std::move(traits.oop_video_decoder),
            /*in_video_decoder_process=*/false);
      }
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2: {
        auto frame_pool = std::make_unique<PlatformVideoFramePool>();
        auto frame_converter = MailboxVideoFrameConverter::Create(
            gpu_task_runner_, traits.get_command_buffer_stub_cb);
        return VideoDecoderPipeline::Create(
            gpu_workarounds_, traits.task_runner, std::move(frame_pool),
            std::move(frame_converter),
            GetPreferredRenderableFourccs(gpu_preferences_, gpu_feature_info_),
            traits.media_log->Clone(), /*oop_video_decoder=*/{},
            /*in_video_decoder_process=*/false);
      }
      default:
        return nullptr;
    }
  }

  void NotifyPlatformDecoderSupport(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<void(
          mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) final {
    switch (
        GetActualPlatformDecoderImplementation(gpu_preferences_, gpu_info_)) {
      case VideoDecoderType::kOutOfProcess:
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        VideoDecoderPipeline::NotifySupportKnown(std::move(oop_video_decoder),
                                                 std::move(cb));
        break;
      default:
        std::move(cb).Run(std::move(oop_video_decoder));
    }
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    VideoDecoderType decoder_implementation =
        GetActualPlatformDecoderImplementation(gpu_preferences_, gpu_info_);
    base::UmaHistogramEnumeration("Media.VaapiLinux.SupportedVideoDecoder",
                                  decoder_implementation);
    switch (decoder_implementation) {
      case VideoDecoderType::kOutOfProcess:
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        return VideoDecoderPipeline::GetSupportedConfigs(decoder_implementation,
                                                         gpu_workarounds_);
      default:
        return std::nullopt;
    }
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    // Determine the preferred decoder based purely on compile-time and run-time
    // flags. This is not intended to determine whether the selected decoder can
    // be successfully initialized or used to decode.
    return GetPreferredLinuxDecoderImplementation();
  }
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientLinux>(traits);
}

}  // namespace media
