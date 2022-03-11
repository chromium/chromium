// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_capability.h"

#include <utility>

namespace media {

CdmCapability::CdmCapability() = default;

CdmCapability::CdmCapability(
    std::vector<AudioCodec> audio_codecs,
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
