// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fuchsia_media_codec_provider_impl.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>

#include <optional>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"

namespace content {

namespace {

std::optional<std::string> GetMimeTypeForVideoCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kH264:
      return "video/h264";
    case media::VideoCodec::kVP8:
      return "video/vp8";
    case media::VideoCodec::kVP9:
      return "video/vp9";
    case media::VideoCodec::kHEVC:
      return "video/hevc";
    case media::VideoCodec::kAV1:
      return "video/av1";

    case media::VideoCodec::kVC1:
    case media::VideoCodec::kMPEG2:
    case media::VideoCodec::kMPEG4:
    case media::VideoCodec::kTheora:
    case media::VideoCodec::kDolbyVision:
      return std::nullopt;

    case media::VideoCodec::kUnknown:
      break;
  }

  NOTREACHED();
}

media::VideoCodecProfile ConvertToVideoCodecProfile(
    const fuchsia::media::CodecProfile& profile) {
  switch (profile) {
    case fuchsia::media::CodecProfile::H264PROFILE_BASELINE:
      return media::VideoCodecProfile::H264PROFILE_BASELINE;
    case fuchsia::media::CodecProfile::H264PROFILE_MAIN:
      return media::VideoCodecProfile::H264PROFILE_MAIN;
    case fuchsia::media::CodecProfile::H264PROFILE_EXTENDED:
      return media::VideoCodecProfile::H264PROFILE_EXTENDED;
    case fuchsia::media::CodecProfile::H264PROFILE_HIGH:
      return media::VideoCodecProfile::H264PROFILE_HIGH;
    case fuchsia::media::CodecProfile::H264PROFILE_HIGH10PROFILE:
      return media::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
    case fuchsia::media::CodecProfile::H264PROFILE_HIGH422PROFILE:
      return media::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
    case fuchsia::media::CodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return media::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case fuchsia::media::CodecProfile::H264PROFILE_SCALABLEBASELINE:
      return media::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
    case fuchsia::media::CodecProfile::H264PROFILE_SCALABLEHIGH:
      return media::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
    case fuchsia::media::CodecProfile::H264PROFILE_STEREOHIGH:
      return media::VideoCodecProfile::H264PROFILE_STEREOHIGH;
    case fuchsia::media::CodecProfile::H264PROFILE_MULTIVIEWHIGH:
      return media::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
    case fuchsia::media::CodecProfile::VP8PROFILE_ANY:
      return media::VideoCodecProfile::VP8PROFILE_ANY;
    case fuchsia::media::CodecProfile::VP9PROFILE_PROFILE0:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE0;
    case fuchsia::media::CodecProfile::VP9PROFILE_PROFILE1:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE1;
    case fuchsia::media::CodecProfile::VP9PROFILE_PROFILE2:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE2;
    case fuchsia::media::CodecProfile::VP9PROFILE_PROFILE3:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE3;
    case fuchsia::media::CodecProfile::HEVCPROFILE_MAIN:
      return media::VideoCodecProfile::HEVCPROFILE_MAIN;
    case fuchsia::media::CodecProfile::HEVCPROFILE_MAIN10:
      return media::VideoCodecProfile::HEVCPROFILE_MAIN10;
    case fuchsia::media::CodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      return media::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
    default:
      NOTIMPLEMENTED() << "Unknown codec profile: " << profile;
      return media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

media::SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigsForCodecList(
    const std::vector<fuchsia::mediacodec::DetailedCodecDescription>&
        detailed_codec_list) {
  media::SupportedVideoDecoderConfigs configs;

  for (const auto& codec_description : detailed_codec_list) {
    if (!codec_description.has_codec_type() || !codec_description.has_is_hw() ||
        !codec_description.has_mime_type() ||
        !codec_description.has_profile_descriptions()) {
      LOG(WARNING) << "Missing required fields when parsing the "
                      "DetailedCodecDescription. Skipped.";
      continue;
    }

    if (
        // Only use platform codecs that are accelerated.
        !codec_description.is_hw() ||
        // Exclude non-video codecs.
        codec_description.mime_type().find("video") != 0 ||
        // Exclude non-decoder codecs.
        codec_description.codec_type() !=
            fuchsia::mediacodec::CodecType::DECODER ||
        !codec_description.profile_descriptions()
             .is_decoder_profile_descriptions()) {
      continue;
    }

    for (const auto& profile_description :
         codec_description.profile_descriptions()
             .decoder_profile_descriptions()) {
      if (!profile_description.has_profile() ||
          !profile_description.has_min_image_size() ||
          !profile_description.has_max_image_size()) {
        LOG(WARNING) << "Missing required fields when parsing the "
                        "DecoderProfileDescription. Skipped.";
        continue;
      }

      const auto video_codec_profile =
          ConvertToVideoCodecProfile(profile_description.profile());
      if (video_codec_profile ==
          media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN) {
        continue;
      }

      configs.emplace_back(
          video_codec_profile, video_codec_profile,
          gfx::Size(profile_description.min_image_size().width,
                    profile_description.min_image_size().height),
          gfx::Size(profile_description.max_image_size().width,
                    profile_description.max_image_size().height),
          profile_description.has_allow_encryption()
              ? profile_description.allow_encryption()
              : false,
          profile_description.has_require_encryption()
              ? profile_description.require_encryption()
              : false);
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

// media::mojom::FuchsiaMediaCodecProvider implementation.
void FuchsiaMediaCodecProviderImpl::CreateVideoDecoder(
    media::VideoCodec codec,
    media::mojom::VideoDecoderSecureMemoryMode secure_mode,
    fidl::InterfaceRequest<fuchsia::media::StreamProcessor>
        stream_processor_request) {
  fuchsia::mediacodec::CreateDecoder_Params decoder_params;

  // Set format details ordinal to 0. Decoder doesn't change the format, so
  // the value doesn't matter.
  decoder_params.mutable_input_details()->set_format_details_version_ordinal(0);

  auto mime_type = GetMimeTypeForVideoCodec(codec);
  if (!mime_type) {
    // Drop `stream_processor_request` if the codec is not supported.
    return;
  }
  decoder_params.mutable_input_details()->set_mime_type(mime_type.value());

  switch (secure_mode) {
    case media::mojom::VideoDecoderSecureMemoryMode::CLEAR:
      // Use defaults for non-secure mode.
      break;

    case media::mojom::VideoDecoderSecureMemoryMode::SECURE:
      decoder_params.set_secure_input_mode(
          fuchsia::mediacodec::SecureMemoryMode::ON);
      decoder_params.set_secure_output_mode(
          fuchsia::mediacodec::SecureMemoryMode::ON);
      break;

    case media::mojom::VideoDecoderSecureMemoryMode::SECURE_OUTPUT:
      decoder_params.set_secure_output_mode(
          fuchsia::mediacodec::SecureMemoryMode::ON);
      break;
  }

  // Video demuxers return each video frame in a separate packet. This field
  // must be set to get frame timestamps on the decoder output.
  decoder_params.set_promise_separate_access_units_on_input(true);

  // We use `fuchsia.mediacodec` only for hardware decoders. Renderer will
  // handle software decoding if hardware decoder is not available.
  decoder_params.set_require_hw(true);

  auto decoder_factory = base::ComponentContextForProcess()
                             ->svc()
                             ->Connect<fuchsia::mediacodec::CodecFactory>();
  decoder_factory->CreateDecoder(std::move(decoder_params),
                                 std::move(stream_processor_request));
}

void FuchsiaMediaCodecProviderImpl::GetSupportedVideoDecoderConfigs(
    GetSupportedVideoDecoderConfigsCallback callback) {
  EnsureCodecFactory();

  pending_get_supported_vd_configs_callbacks_.emplace_back(std::move(callback));
  if (supported_video_decoder_configs_) {
    RunPendingGetSupportedVideoDecoderConfigsCallbacks();
  }
}

// End of media::mojom::FuchsiaMediaCodecProvider implementation.

void FuchsiaMediaCodecProviderImpl::EnsureCodecFactory() {
  if (codec_factory_)
    return;

  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::mediacodec::CodecFactory>(codec_factory_.NewRequest());

  codec_factory_.set_error_handler(fit::bind_member(
      this, &FuchsiaMediaCodecProviderImpl::OnCodecFactoryDisconnected));
  codec_factory_->GetDetailedCodecDescriptions(fit::bind_member(
      this, &FuchsiaMediaCodecProviderImpl::OnGetDetailedCodecDescriptions));
}

void FuchsiaMediaCodecProviderImpl::OnCodecFactoryDisconnected(
    zx_status_t status) {
  ZX_LOG(ERROR, status) << "fuchsia.mediacodec.CodecFactory disconnected.";

  supported_video_decoder_configs_.reset();
  RunPendingGetSupportedVideoDecoderConfigsCallbacks();
}

void FuchsiaMediaCodecProviderImpl::OnGetDetailedCodecDescriptions(
    fuchsia::mediacodec::CodecFactoryGetDetailedCodecDescriptionsResponse
        response) {
  if (response.has_codecs()) {
    supported_video_decoder_configs_.emplace(
        GetSupportedVideoDecoderConfigsForCodecList(response.codecs()));
  }
  RunPendingGetSupportedVideoDecoderConfigsCallbacks();
}

void FuchsiaMediaCodecProviderImpl::
    RunPendingGetSupportedVideoDecoderConfigsCallbacks() {
  media::SupportedVideoDecoderConfigs configs =
      supported_video_decoder_configs_.value_or(
          std::vector<media::SupportedVideoDecoderConfig>{});

  for (auto& callback :
       std::exchange(pending_get_supported_vd_configs_callbacks_, {})) {
    std::move(callback).Run(configs);
  }
}

}  // namespace content
