// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/android/build_info.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/gpu/android/android_video_surface_chooser_impl.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/gpu/android/direct_shared_image_video_provider.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/media_codec_video_decoder.h"
#include "media/gpu/android/pooled_shared_image_video_provider.h"
#include "media/gpu/android/video_frame_factory_impl.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/android_mojo_util.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/gpu/android/ndk_audio_encoder.h"
#endif

using media::android_mojo_util::CreateMediaDrmStorage;
using media::android_mojo_util::CreateProvisionFetcher;

namespace media {

class GpuMojoMediaClientAndroid final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientAndroid(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {
    android_overlay_factory_cb_ = std::move(traits.android_overlay_factory_cb);
  }
  ~GpuMojoMediaClientAndroid() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    scoped_refptr<gpu::RefCountedLock> ref_counted_lock;

    // When this feature is enabled, CodecImage, CodecBufferWaitCoordinator and
    // other media classes used in MCVD path will be accessed by multiple gpu
    // threads. To implement thread safetyness, we are using a global ref
    // counted lock here. CodecImage, CodecOutputBufferRenderer,
    // CodecBufferWaitCoordinator expects this ref counted lock to be held by
    // the classes which are accessing them (AndroidVideoImageBacking, MRE,
    // FrameInfoHelper etc.)
    if (features::NeedThreadSafeAndroidMedia()) {
      ref_counted_lock = base::MakeRefCounted<gpu::RefCountedLock>();
    }

    std::unique_ptr<SharedImageVideoProvider> image_provider =
        std::make_unique<DirectSharedImageVideoProvider>(
            gpu_task_runner_, traits.get_command_buffer_stub_cb,
            ref_counted_lock);

    if (base::FeatureList::IsEnabled(kUsePooledSharedImageVideoProvider)) {
      // Wrap |image_provider| in a pool.
      image_provider = PooledSharedImageVideoProvider::Create(
          gpu_task_runner_, traits.get_command_buffer_stub_cb,
          std::move(image_provider), ref_counted_lock);
    }
    // TODO(liberato): Create this only if we're using Vulkan, else it's
    // ignored.  If we can tell that here, then VideoFrameFactory can use it
    // as a signal about whether it's supposed to get YCbCrInfo rather than
    // requiring the provider to set |is_vulkan| in the ImageRecord.
    auto frame_info_helper = FrameInfoHelper::Create(
        gpu_task_runner_, traits.get_command_buffer_stub_cb, ref_counted_lock);

    return MediaCodecVideoDecoder::Create(
        gpu_preferences_, gpu_feature_info_, traits.media_log->Clone(),
        DeviceInfo::GetInstance(),
        CodecAllocator::GetInstance(gpu_task_runner_),
        std::make_unique<AndroidVideoSurfaceChooserImpl>(
            DeviceInfo::GetInstance()->IsSetOutputSurfaceSupported()),
        android_overlay_factory_cb_, std::move(traits.request_overlay_info_cb),
        std::make_unique<VideoFrameFactoryImpl>(
            gpu_task_runner_, gpu_preferences_, std::move(image_provider),
            MaybeRenderEarlyManager::Create(gpu_task_runner_, ref_counted_lock),
            std::move(frame_info_helper), ref_counted_lock),
        ref_counted_lock);
  }

  std::optional<SupportedAudioDecoderConfigs>
  GetPlatformSupportedAudioDecoderConfigs() final {
    SupportedAudioDecoderConfigs audio_configs;
    if (base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_P) {
      audio_configs.emplace_back(AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC);
    }
    return audio_configs;
  }

  std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    return MediaCodecVideoDecoder::GetSupportedConfigs();
  }

  std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) final {
    return std::make_unique<MediaCodecAudioDecoder>(std::move(task_runner));
  }

  std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner) final {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (__builtin_available(android NDK_MEDIA_CODEC_MIN_API, *)) {
      return std::make_unique<NdkAudioEncoder>(std::move(task_runner));
    }
#endif
    return nullptr;
  }

  std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) final {
    return std::make_unique<AndroidCdmFactory>(
        base::BindRepeating(&CreateProvisionFetcher, frame_interfaces),
        base::BindRepeating(&CreateMediaDrmStorage, frame_interfaces));
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    return VideoDecoderType::kMediaCodec;
  }

  AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
};

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientAndroid>(traits);
}

}  // namespace media
