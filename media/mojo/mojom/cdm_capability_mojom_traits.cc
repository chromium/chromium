// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/cdm_capability_mojom_traits.h"

#include <set>
#include <utility>

namespace mojo {

namespace {

template <typename T>
bool AreUnique(const std::vector<T>& values) {
  std::set<T> unique_values;
  for (const auto& value : values)
    unique_values.emplace(value);
  return values.size() == unique_values.size();
}

}  // namespace

// static
bool StructTraits<media::mojom::VideoCodecInfoDataView, media::VideoCodecInfo>::
    Read(media::mojom::VideoCodecInfoDataView input,
         media::VideoCodecInfo* output) {
  std::vector<media::VideoCodecProfile> supported_profiles;
  if (!input.ReadSupportedProfiles(&supported_profiles)) {
    return false;
  }

  *output = media::VideoCodecInfo(std::move(supported_profiles),
                                  input.supports_clear_lead());
  return true;
}

// static
bool StructTraits<media::mojom::CdmCapabilityDataView, media::CdmCapability>::
    Read(media::mojom::CdmCapabilityDataView input,
         media::CdmCapability* output) {
  std::vector<media::AudioCodec> audio_codecs;
  if (!input.ReadAudioCodecs(&audio_codecs))
    return false;

  media::CdmCapability::VideoCodecMap video_codecs;
  if (!input.ReadVideoCodecs(&video_codecs))
    return false;

  std::vector<media::EncryptionScheme> encryption_schemes;
  if (!input.ReadEncryptionSchemes(&encryption_schemes))
    return false;

  std::vector<media::CdmSessionType> session_types;
  if (!input.ReadSessionTypes(&session_types))
    return false;

  base::Version version;
  if (!input.ReadVersion(&version)) {
    return false;
  }

  // |encryption_schemes|, |session_types| and |audio_codecs| are converted
  // to a base::flat_map implicitly.
  *output =
      media::CdmCapability(std::move(audio_codecs), std::move(video_codecs),
                           std::move(encryption_schemes),
                           std::move(session_types), std::move(version));
  return true;
}

// static
media::mojom::CdmCapabilityQueryStatus EnumTraits<
    media::mojom::CdmCapabilityQueryStatus,
    media::CdmCapabilityQueryStatus>::ToMojom(media::CdmCapabilityQueryStatus
                                                  input) {
  switch (input) {
    case media::CdmCapabilityQueryStatus::kSuccess:
      return media::mojom::CdmCapabilityQueryStatus::kSuccess;
    case media::CdmCapabilityQueryStatus::kUnknown:
      return media::mojom::CdmCapabilityQueryStatus::kUnknown;
    case media::CdmCapabilityQueryStatus::kHardwareSecureCodecNotSupported:
      return media::mojom::CdmCapabilityQueryStatus::
          kHardwareSecureCodecNotSupported;
    case media::CdmCapabilityQueryStatus::kNoSupportedVideoCodec:
      return media::mojom::CdmCapabilityQueryStatus::kNoSupportedVideoCodec;
    case media::CdmCapabilityQueryStatus::kNoSupportedEncryptionScheme:
      return media::mojom::CdmCapabilityQueryStatus::
          kNoSupportedEncryptionScheme;
    case media::CdmCapabilityQueryStatus::kUnsupportedKeySystem:
      return media::mojom::CdmCapabilityQueryStatus::kUnsupportedKeySystem;
    case media::CdmCapabilityQueryStatus::kMediaFoundationCdmNotSupported:
      return media::mojom::CdmCapabilityQueryStatus::
          kMediaFoundationCdmNotSupported;
    case media::CdmCapabilityQueryStatus::kDisconnectionError:
      return media::mojom::CdmCapabilityQueryStatus::kDisconnectionError;
    case media::CdmCapabilityQueryStatus::kMediaFoundationGetCdmFactoryFailed:
      return media::mojom::CdmCapabilityQueryStatus::
          kMediaFoundationGetCdmFactoryFailed;
    case media::CdmCapabilityQueryStatus::kCreateDummyMediaFoundationCdmFailed:
      return media::mojom::CdmCapabilityQueryStatus::
          kCreateDummyMediaFoundationCdmFailed;
    case media::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability:
      return media::mojom::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability;
    case media::CdmCapabilityQueryStatus::kNoMediaDrmSupport:
      return media::mojom::CdmCapabilityQueryStatus::kNoMediaDrmSupport;
    case media::CdmCapabilityQueryStatus::
        kMediaFoundationGetExtendedDRMTypeSupportFailed:
      return media::mojom::CdmCapabilityQueryStatus::
          kMediaFoundationGetExtendedDRMTypeSupportFailed;
  }

  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::CdmCapabilityQueryStatus,
                media::CdmCapabilityQueryStatus>::
    FromMojom(media::mojom::CdmCapabilityQueryStatus input,
              media::CdmCapabilityQueryStatus* output) {
  switch (input) {
    case media::mojom::CdmCapabilityQueryStatus::kSuccess:
      *output = media::CdmCapabilityQueryStatus::kSuccess;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kUnknown:
      *output = media::CdmCapabilityQueryStatus::kUnknown;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::
        kHardwareSecureCodecNotSupported:
      *output =
          media::CdmCapabilityQueryStatus::kHardwareSecureCodecNotSupported;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kNoSupportedVideoCodec:
      *output = media::CdmCapabilityQueryStatus::kNoSupportedVideoCodec;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kNoSupportedEncryptionScheme:
      *output = media::CdmCapabilityQueryStatus::kNoSupportedEncryptionScheme;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kUnsupportedKeySystem:
      *output = media::CdmCapabilityQueryStatus::kUnsupportedKeySystem;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationCdmNotSupported:
      *output =
          media::CdmCapabilityQueryStatus::kMediaFoundationCdmNotSupported;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kDisconnectionError:
      *output = media::CdmCapabilityQueryStatus::kDisconnectionError;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationGetCdmFactoryFailed:
      *output =
          media::CdmCapabilityQueryStatus::kMediaFoundationGetCdmFactoryFailed;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::
        kCreateDummyMediaFoundationCdmFailed:
      *output =
          media::CdmCapabilityQueryStatus::kCreateDummyMediaFoundationCdmFailed;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability:
      *output = media::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::kNoMediaDrmSupport:
      *output = media::CdmCapabilityQueryStatus::kNoMediaDrmSupport;
      return true;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationGetExtendedDRMTypeSupportFailed:
      *output = media::CdmCapabilityQueryStatus::
          kMediaFoundationGetExtendedDRMTypeSupportFailed;
      return true;
  }

  NOTREACHED();
}

}  // namespace mojo
