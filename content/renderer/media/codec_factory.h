// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CODEC_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CODEC_FACTORY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "media/base/decoder.h"
#include "media/base/media_log.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

// Assists to GpuVideoAcceleratorFactoriesImpl on hardware decoder and encoder
// functionalities.
//
// It is a base class that handles the encoder resources
// via media::mojom::VideoEncodeAcceleratorProvider. Its derived classes need
// to implement how to connect to hardware decoder resources.
class CONTENT_EXPORT CodecFactory {
 public:
  // `media_task_runner` - task runner for running multi-media operations.
  // `context_provider` - context provider for creating a video decoder.
  // `video_decode_accelerator_enabled` - whether the video decode accelerator
  //    is enabled.
  // `video_encode_accelerator_enabled` - whether the video encode accelerator
  //    is enabled.
  // `pending_vea_provider_remote` - bound pending
  //    media::mojom::VideoEncodeAcceleratorProvider remote.
  CodecFactory(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      bool video_decode_accelerator_enabled,
      bool video_encode_accelerator_enabled,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          pending_vea_provider_remote);

  CodecFactory(const CodecFactory&) = delete;
  CodecFactory& operator=(const CodecFactory&) = delete;
  virtual ~CodecFactory();

  // `gpu_factories` - pointer to the GpuVideoAcceleratorFactories that
  //    owns |this|.
  // `media_log` - process-wide pointer to log to chrome://media-internals log.
  // `request_overlay_info_cb` - callback that gets the overlay information.
  // `rendering_color_space` - color space for the purpose of color conversion.
  //
  // Derived class should construct its own type of video decoder.
  virtual std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      media::MediaLog* media_log,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& rendering_color_space) = 0;

  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator();

  // Returns VideoDecoderType::kUnknown in cases where IsDecoderSupportKnown()
  // is false.
  // Otherwise, it returns the type of decoder that provided the
  // configs for the config support check.
  media::VideoDecoderType GetVideoDecoderType();

  // Returns a nullopt if we have not yet gotten the configs.
  // Returns an optional that contains an empty vector if we have gotten the
  // result and there are no supported configs.
  std::optional<media::SupportedVideoDecoderConfigs>
  GetSupportedVideoDecoderConfigs();

  // Returns a nullopt if we have not yet gotten the profiles.
  // Returns an optional that contains an empty vector if we have gotten the
  // result and there are no supported profiles.
  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles();

  // Returns true if media::SupportedVideoDecoderConfigs are populated.
  bool IsDecoderSupportKnown();

  // Returns true if media::VideoEncodeAccelerator::SupportedProfiles are
  // populated.
  bool IsEncoderSupportKnown();

  // If the current decoder support is not yet known, use this to register a
  // callback that is notified once the support is known. At that point, calling
  // GetSupportedVideoDecoderConfigs will give the set of supported decoder
  // configs.
  //
  // There is no way to unsubscribe a callback, it is recommended to use a
  // WeakPtr if you need this feature.
  void NotifyDecoderSupportKnown(base::OnceClosure callback);

  // If the current encoder support is not yet known, use this to register a
  // callback that is notified once the support is known. At that point, calling
  // GetVideoEncodeAcceleratorSupportedProfiles will give the set of supported
  // encoder profiles.
  //
  // There is no way to unsubscribe a callback, it is recommended to use a
  // WeakPtr if you need this feature.
  void NotifyEncoderSupportKnown(base::OnceClosure callback);

  // Provides this instance with the gpu channel token for the
  // associated gpu channel.
  void OnChannelTokenReady(const base::UnguessableToken& token,
                           int32_t route_id);

 protected:
  class Notifier {
   public:
    Notifier();
    ~Notifier();

    void Register(base::OnceClosure callback);
    void Notify();

    bool is_notified() { return is_notified_; }

   private:
    bool is_notified_ = false;
    std::vector<base::OnceClosure> callbacks_;
  };

  void OnDecoderSupportFailed();
  void OnGetSupportedDecoderConfigs();

  void OnEncoderSupportFailed();
  void OnGetVideoEncodeAcceleratorSupportedProfiles(
      const media::VideoEncodeAccelerator::SupportedProfiles&
          supported_profiles);
  bool IsEncoderReady() EXCLUSIVE_LOCKS_REQUIRED(supported_profiles_lock_);

  // Task runner on the Media thread for running multi-media operations
  // (e.g., creating a video decoder).
  // In Fuchsia, it needs to be started with the IO message pump for FIDL calls.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Shared pointer to a shared context provider. All access should happen only
  // on the media thread.
  const scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;

  // Whether video acceleration encoding/decoding should be enabled.
  const bool video_decode_accelerator_enabled_;
  const bool video_encode_accelerator_enabled_;

  base::Lock supported_profiles_lock_;

  // If the Optional is empty, then we have not yet gotten the configs.
  // If the Optional contains an empty vector, then we have gotten the result
  // and there are no supported configs.
  std::optional<media::SupportedVideoDecoderConfigs> supported_decoder_configs_
      GUARDED_BY(supported_profiles_lock_);
  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
      supported_vea_profiles_ GUARDED_BY(supported_profiles_lock_);

  media::VideoDecoderType video_decoder_type_
      GUARDED_BY(supported_profiles_lock_) = media::VideoDecoderType::kUnknown;

  Notifier decoder_support_notifier_ GUARDED_BY(supported_profiles_lock_);
  Notifier encoder_support_notifier_ GUARDED_BY(supported_profiles_lock_);

 private:
  void BindOnTaskRunner(
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          pending_vea_provider_remote);

  mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider> vea_provider_;
  base::UnguessableToken channel_token_;
  int32_t route_id_;
};

// CodecFactoryDefault is the default derived class, which has no
// decoder provider. It does not have any supported video decoder configs and
// returns a null pointer when creating a hardware video decoder.
class CodecFactoryDefault final : public CodecFactory {
 public:
  CodecFactoryDefault(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      bool video_decode_accelerator_enabled,
      bool video_encode_accelerator_enabled,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          pending_vea_provider_remote);
  ~CodecFactoryDefault() override;

  // Returns nullptr since there is no decoder provider.
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      media::MediaLog* media_log,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& rendering_color_space) override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CODEC_FACTORY_H_
