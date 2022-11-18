// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fuchsia_media_codec_provider_impl.h"

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "media/base/supported_video_decoder_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

// fuchsia::mediacodec::CodecDescription does not provide enough codec info
// to determine if a media::VideoDecoderConfig is supported. The constant and
// the helper function below is to make a safe assumption that converts the type
// to media::SupportedVideoDecoderConfigs.
// TODO(fxbug.dev/85214): Remove the constant and the helper function below
// after more details are added to mediacodec.CodecDescription.
constexpr gfx::Size kFuchsiaDecodeSizeMax(1920, 1080);  // 1080p
media::SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigsForCodecList(
    const std::vector<fuchsia::mediacodec::CodecDescription>& codec_list) {
  media::SupportedVideoDecoderConfigs configs;
  for (const auto& codec_description : codec_list) {
    if (codec_description.mime_type == "video/h264" ||
        codec_description.mime_type == "video/h264-muti") {
      configs.emplace_back(media::VideoCodecProfile::H264PROFILE_MIN,
                           media::VideoCodecProfile::H264PROFILE_STEREOHIGH,
                           media::kDefaultSwDecodeSizeMin,
                           kFuchsiaDecodeSizeMax, codec_description.is_hw,
                           false);
    } else if (codec_description.mime_type == "video/vp8") {
      configs.emplace_back(media::VideoCodecProfile::VP8PROFILE_MIN,
                           media::VideoCodecProfile::VP8PROFILE_MAX,
                           media::kDefaultSwDecodeSizeMin,
                           kFuchsiaDecodeSizeMax, codec_description.is_hw,
                           false);
    } else if (codec_description.mime_type == "video/vp9") {
      // Only SD profiles are supported for VP9. HDR profiles (2 and 3) are not
      // supported.
      configs.emplace_back(media::VideoCodecProfile::VP9PROFILE_MIN,
                           media::VideoCodecProfile::VP9PROFILE_PROFILE1,
                           media::kDefaultSwDecodeSizeMin,
                           kFuchsiaDecodeSizeMax, codec_description.is_hw,
                           false);
    }
  }

  return configs;
}

}  // namespace

FuchsiaMediaCodecProviderImpl::FuchsiaMediaCodecProviderImpl() = default;
FuchsiaMediaCodecProviderImpl::~FuchsiaMediaCodecProviderImpl() = default;

void FuchsiaMediaCodecProviderImpl::AddReceiver(
    mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  receivers_.Add(this, std::move(receiver));
}

void FuchsiaMediaCodecProviderImpl::GetSupportedVideoDecoderConfigs(
    GetSupportedVideoDecoderConfigsCallback callback) {
  EnsureCodecFactory();

  pending_get_supported_vd_configs_callbacks_.emplace_back(std::move(callback));
  if (supported_video_decoder_configs_) {
    RunPendingGetSupportedVideoDecoderConfigsCallbacks();
  }
}

void FuchsiaMediaCodecProviderImpl::EnsureCodecFactory() {
  if (codec_factory_)
    return;

  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::mediacodec::CodecFactory>(codec_factory_.NewRequest());

  codec_factory_.set_error_handler(fit::bind_member(
      this, &FuchsiaMediaCodecProviderImpl::OnCodecFactoryDisconnected));
  codec_factory_.events().OnCodecList =
      fit::bind_member(this, &FuchsiaMediaCodecProviderImpl::OnCodecList);
}

void FuchsiaMediaCodecProviderImpl::OnCodecFactoryDisconnected(
    zx_status_t status) {
  ZX_LOG(ERROR, status) << "fuchsia.mediacodec.CodecFactory disconnected.";

  supported_video_decoder_configs_.reset();
  RunPendingGetSupportedVideoDecoderConfigsCallbacks();
}

void FuchsiaMediaCodecProviderImpl::OnCodecList(
    std::vector<::fuchsia::mediacodec::CodecDescription> codec_list) {
  supported_video_decoder_configs_.emplace(
      GetSupportedVideoDecoderConfigsForCodecList(codec_list));
  RunPendingGetSupportedVideoDecoderConfigsCallbacks();
}

void FuchsiaMediaCodecProviderImpl::
    RunPendingGetSupportedVideoDecoderConfigsCallbacks() {
  media::SupportedVideoDecoderConfigs configs = {};
  if (supported_video_decoder_configs_) {
    configs = supported_video_decoder_configs_.value();
  }

  for (auto& callback : pending_get_supported_vd_configs_callbacks_) {
    std::move(callback).Run(configs);
  }
  pending_get_supported_vd_configs_callbacks_.clear();
}

}  // namespace content
