// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/memory/ptr_util.h"
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
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "media/mojo/services/android_mojo_util.h"
#include "media/mojo/services/mojo_media_drm_storage.h"
#include "media/mojo/services/mojo_provision_fetcher.h"

using media::android_mojo_util::CreateMediaDrmStorage;
using media::android_mojo_util::CreateProvisionFetcher;

namespace media {

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    const VideoDecoderTraits& traits) {
  std::unique_ptr<SharedImageVideoProvider> image_provider =
      std::make_unique<DirectSharedImageVideoProvider>(
          traits.gpu_task_runner, traits.get_command_buffer_stub_cb);

  if (base::FeatureList::IsEnabled(kUsePooledSharedImageVideoProvider)) {
    // Wrap |image_provider| in a pool.
    image_provider = PooledSharedImageVideoProvider::Create(
        traits.gpu_task_runner, traits.get_command_buffer_stub_cb,
        std::move(image_provider));
  }
  // TODO(liberato): Create this only if we're using Vulkan, else it's
  // ignored.  If we can tell that here, then VideoFrameFactory can use it
  // as a signal about whether it's supposed to get YCbCrInfo rather than
  // requiring the provider to set |is_vulkan| in the ImageRecord.
  auto frame_info_helper = FrameInfoHelper::Create(
      traits.gpu_task_runner, traits.get_command_buffer_stub_cb);

  return MediaCodecVideoDecoder::Create(
      traits.gpu_preferences, traits.gpu_feature_info,
      traits.media_log->Clone(), DeviceInfo::GetInstance(),
      CodecAllocator::GetInstance(traits.gpu_task_runner),
      std::make_unique<AndroidVideoSurfaceChooserImpl>(
          DeviceInfo::GetInstance()->IsSetOutputSurfaceSupported()),
      traits.android_overlay_factory_cb,
      std::move(traits.request_overlay_info_cb),
      std::make_unique<VideoFrameFactoryImpl>(
          traits.gpu_task_runner, traits.gpu_preferences,
          std::move(image_provider),
          MaybeRenderEarlyManager::Create(traits.gpu_task_runner),
          std::move(frame_info_helper)));
}

SupportedVideoDecoderConfigs GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  return MediaCodecVideoDecoder::GetSupportedConfigs();
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<MediaCodecAudioDecoder>(std::move(task_runner));
}

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return std::make_unique<AndroidCdmFactory>(
      base::BindRepeating(&CreateProvisionFetcher, frame_interfaces),
      base::BindRepeating(&CreateMediaDrmStorage, frame_interfaces));
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences) {
  return VideoDecoderType::kMediaCodec;
}

}  // namespace media