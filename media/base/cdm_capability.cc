// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_capability.h"

#include <utility>

namespace media {

VideoCodecInfo::VideoCodecInfo() = default;

VideoCodecInfo::VideoCodecInfo(
    base::flat_set<VideoCodecProfile> supported_profiles,
    bool supports_clear_lead)
    : supported_profiles(std::move(supported_profiles)),
      supports_clear_lead(supports_clear_lead) {}

VideoCodecInfo::VideoCodecInfo(
    base::flat_set<VideoCodecProfile> supported_profiles)
    : supported_profiles(std::move(supported_profiles)) {}

VideoCodecInfo::VideoCodecInfo(const VideoCodecInfo& other) = default;

VideoCodecInfo::~VideoCodecInfo() = default;

bool operator==(const VideoCodecInfo& lhs, const VideoCodecInfo& rhs) {
  return lhs.supported_profiles == rhs.supported_profiles &&
         lhs.supports_clear_lead == rhs.supports_clear_lead;
}

CdmCapability::CdmCapability() = default;

CdmCapability::CdmCapability(
    base::flat_set<AudioCodec> audio_codecs,
    VideoCodecMap video_codecs,
    base::flat_set<EncryptionScheme> encryption_schemes,
    base::flat_set<CdmSessionType> session_types)
    : audio_codecs(std::move(audio_codecs)),
      video_codecs(std::move(video_codecs)),
      encryption_schemes(std::move(encryption_schemes)),
      session_types(std::move(session_types)) {}

CdmCapability::CdmCapability(const CdmCapability& other) = default;

CdmCapability::~CdmCapability() = default;

bool operator==(const CdmCapability& lhs, const CdmCapability& rhs) {
  return lhs.audio_codecs == rhs.audio_codecs &&
         lhs.video_codecs == rhs.video_codecs &&
         lhs.encryption_schemes == rhs.encryption_schemes &&
         lhs.session_types == rhs.session_types;
}

}  // namespace media
