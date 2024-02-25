// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CODEC_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CODEC_PROVIDER_IMPL_H_

#include <fuchsia/mediacodec/cpp/fidl.h>

#include <optional>

#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Implements media::mojom::FuchsiaMediFuchsiaMediaCodecProvider by calling
// out to the fuchsia::mediacodec::CodecFactory APIs. Runs on the IO thread in
// the Browser process.
//
// It is used in cases without a frame context, namely by WebRTC, and for
// querying of supported codecs.
class CONTENT_EXPORT FuchsiaMediaCodecProviderImpl final
    : public media::mojom::FuchsiaMediaCodecProvider {
 public:
  FuchsiaMediaCodecProviderImpl();
  ~FuchsiaMediaCodecProviderImpl() override;

  FuchsiaMediaCodecProviderImpl(const FuchsiaMediaCodecProviderImpl&) = delete;
  FuchsiaMediaCodecProviderImpl& operator=(
      const FuchsiaMediaCodecProviderImpl&) = delete;

  void AddReceiver(
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver);

  // media::mojom::FuchsiaMediaCodecProvider implementation.
  void CreateVideoDecoder(
      media::VideoCodec codec,
      media::mojom::VideoDecoderSecureMemoryMode secure_mode,
      fidl::InterfaceRequest<fuchsia::media::StreamProcessor>
          stream_processor_request) final;
  void GetSupportedVideoDecoderConfigs(
      GetSupportedVideoDecoderConfigsCallback callback) final;

 private:
  void EnsureCodecFactory();

  // Handlers for events on the codec factory API channel.
  void OnCodecFactoryDisconnected(zx_status_t status);
  void OnGetDetailedCodecDescriptions(
      fuchsia::mediacodec::CodecFactoryGetDetailedCodecDescriptionsResponse);

  void RunPendingGetSupportedVideoDecoderConfigsCallbacks();

  // Connection pointer to the platform codec factory.
  fuchsia::mediacodec::CodecFactoryPtr codec_factory_;

  // Cache of video decoder configurations supported by codec_factory_.
  std::optional<media::SupportedVideoDecoderConfigs>
      supported_video_decoder_configs_;

  // Holds GetSupportedVideoDecoderConfigs completion callbacks
  // from calls received before the list of supported configurations
  // has been fetched and cached.
  std::vector<GetSupportedVideoDecoderConfigsCallback>
      pending_get_supported_vd_configs_callbacks_;

  mojo::ReceiverSet<media::mojom::FuchsiaMediaCodecProvider> receivers_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CODEC_PROVIDER_IMPL_H_
