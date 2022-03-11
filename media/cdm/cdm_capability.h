// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_CAPABILITY_H_
#define MEDIA_CDM_CDM_CAPABILITY_H_

#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media {

// Capabilities supported by a Content Decryption Module.
struct MEDIA_EXPORT CdmCapability {
  using VideoCodecMap = std::map<VideoCodec, std::vector<VideoCodecProfile>>;

  CdmCapability();
  CdmCapability(std::vector<AudioCodec> audio_codecs,
                VideoCodecMap video_codecs,
                base::flat_set<EncryptionScheme> encryption_schemes,
                base::flat_set<CdmSessionType> session_types);
  CdmCapability(const CdmCapability& other);
  ~CdmCapability();

  // List of audio codecs supported by the CDM (e.g. opus). This is the set of
  // codecs supported by the media pipeline using the CDM. This does not include
  // codec profiles, as in general Chromium doesn't handle audio codec profiles
  // separately.
  std::vector<AudioCodec> audio_codecs;

  // Map of video codecs and associated profiles supported by the CDM
  // (e.g. vp8). This is the set of codecs supported by the media pipeline
  // using the CDM. For a supported VideoCodec, if the vector of
  // VideoCodecProfiles is empty, then it assumes that all profiles of the
  // specified codecs may actually be supported.
  VideoCodecMap video_codecs;

  // List of encryption schemes supported by the CDM (e.g. cenc).
  base::flat_set<EncryptionScheme> encryption_schemes;

  // List of session types supported by the CDM.
  base::flat_set<CdmSessionType> session_types;
};

bool MEDIA_EXPORT operator==(const CdmCapability& lhs,
                             const CdmCapability& rhs);

}  // namespace media

#endif  // MEDIA_CDM_CDM_CAPABILITY_H_
