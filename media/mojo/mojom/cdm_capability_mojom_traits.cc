// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/cdm_capability_mojom_traits.h"

#include <set>
#include <utility>

#include "base/notreached.h"

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
media::CdmCapabilityQueryStatus
EnumTraits<media::mojom::CdmCapabilityQueryStatus,
           media::CdmCapabilityQueryStatus>::
    FromMojom(media::mojom::CdmCapabilityQueryStatus input) {
  switch (input) {
    case media::mojom::CdmCapabilityQueryStatus::kSuccess:
      return media::CdmCapabilityQueryStatus::kSuccess;
    case media::mojom::CdmCapabilityQueryStatus::kUnknown:
      return media::CdmCapabilityQueryStatus::kUnknown;
    case media::mojom::CdmCapabilityQueryStatus::
        kHardwareSecureCodecNotSupported:
      return media::CdmCapabilityQueryStatus::kHardwareSecureCodecNotSupported;
    case media::mojom::CdmCapabilityQueryStatus::kNoSupportedVideoCodec:
      return media::CdmCapabilityQueryStatus::kNoSupportedVideoCodec;
    case media::mojom::CdmCapabilityQueryStatus::kNoSupportedEncryptionScheme:
      return media::CdmCapabilityQueryStatus::kNoSupportedEncryptionScheme;
    case media::mojom::CdmCapabilityQueryStatus::kUnsupportedKeySystem:
      return media::CdmCapabilityQueryStatus::kUnsupportedKeySystem;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationCdmNotSupported:
      return media::CdmCapabilityQueryStatus::kMediaFoundationCdmNotSupported;
    case media::mojom::CdmCapabilityQueryStatus::kDisconnectionError:
      return media::CdmCapabilityQueryStatus::kDisconnectionError;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationGetCdmFactoryFailed:
      return media::CdmCapabilityQueryStatus::
          kMediaFoundationGetCdmFactoryFailed;
    case media::mojom::CdmCapabilityQueryStatus::
        kCreateDummyMediaFoundationCdmFailed:
      return media::CdmCapabilityQueryStatus::
          kCreateDummyMediaFoundationCdmFailed;
    case media::mojom::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability:
      return media::CdmCapabilityQueryStatus::kUnexpectedEmptyCapability;
    case media::mojom::CdmCapabilityQueryStatus::kNoMediaDrmSupport:
      return media::CdmCapabilityQueryStatus::kNoMediaDrmSupport;
    case media::mojom::CdmCapabilityQueryStatus::
        kMediaFoundationGetExtendedDRMTypeSupportFailed:
      return media::CdmCapabilityQueryStatus::
          kMediaFoundationGetExtendedDRMTypeSupportFailed;
  }

  NOTREACHED();
}

}  // namespace mojo
