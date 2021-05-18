// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_CAPABILITY_H_
#define MEDIA_CDM_CDM_CAPABILITY_H_

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
  CdmCapability();
  CdmCapability(std::vector<AudioCodec> audio_codecs,
                std::vector<VideoCodec> video_codecs,
                base::flat_set<EncryptionScheme> encryption_schemes,
                base::flat_set<CdmSessionType> session_types);
  CdmCapability(const CdmCapability& other);
  ~CdmCapability();

  // List of audio codecs supported by the CDM (e.g. opus). This is the set of
  // codecs supported by the media pipeline using the CDM, where CDM does the
  // decryption, and the media pipeline does decoding. As this is generic, not
  // all profiles or levels of the specified codecs may actually be supported.
  std::vector<AudioCodec> audio_codecs;

  // List of video codecs supported by the CDM (e.g. vp8). This is the set of
  // codecs supported by the media pipeline using the CDM, where CDM does the
  // decryption, and the media pipeline does decoding. As this is generic, not
  // all profiles or levels of the specified codecs may actually be supported.
  // TODO(crbug.com/796725) Find a way to include profiles and levels.
  std::vector<VideoCodec> video_codecs;

  // List of encryption schemes supported by the CDM (e.g. cenc).
  base::flat_set<EncryptionScheme> encryption_schemes;

  // List of session types supported by the CDM.
  base::flat_set<CdmSessionType> session_types;
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_CAPABILITY_H_
