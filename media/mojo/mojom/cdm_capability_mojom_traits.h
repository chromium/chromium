// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_

#include "base/containers/flat_set.h"
#include "media/base/audio_codecs.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/key_system_support.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::VideoCodecInfoDataView,
                    media::VideoCodecInfo> {
  static const base::flat_set<media::VideoCodecProfile>& supported_profiles(
      const media::VideoCodecInfo& input) {
    return input.supported_profiles;
  }

  static const bool& supports_clear_lead(const media::VideoCodecInfo& input) {
    return input.supports_clear_lead;
  }

  static bool Read(media::mojom::VideoCodecInfoDataView input,
                   media::VideoCodecInfo* output);
};

template <>
struct StructTraits<media::mojom::CdmCapabilityDataView, media::CdmCapability> {
  static const base::flat_set<media::AudioCodec>& audio_codecs(
      const media::CdmCapability& input) {
    return input.audio_codecs;
  }

  static const media::CdmCapability::VideoCodecMap& video_codecs(
      const media::CdmCapability& input) {
    return input.video_codecs;
  }

  // List of encryption schemes supported by the CDM (e.g. cenc).
  static const base::flat_set<media::EncryptionScheme>& encryption_schemes(
      const media::CdmCapability& input) {
    return input.encryption_schemes;
  }

  // List of session types supported by the CDM.
  static const base::flat_set<media::CdmSessionType>& session_types(
      const media::CdmCapability& input) {
    return input.session_types;
  }

  static bool Read(media::mojom::CdmCapabilityDataView input,
                   media::CdmCapability* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CDM_CAPABILITY_MOJOM_TRAITS_H_
