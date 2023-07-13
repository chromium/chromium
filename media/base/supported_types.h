// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SUPPORTED_TYPES_H_
#define MEDIA_BASE_SUPPORTED_TYPES_H_

#include "base/containers/flat_set.h"
#include "media/base/media_export.h"
#include "media/base/media_types.h"

namespace media {

// These functions will attempt to delegate to MediaClient (when present) to
// describe what types of media are supported. When no MediaClient is provided,
// they will fall back to calling the Default functions below.
MEDIA_EXPORT bool IsSupportedAudioType(const AudioType& type);
MEDIA_EXPORT bool IsSupportedVideoType(const VideoType& type);

// These functions describe what media/ alone supports. They do not call out to
// MediaClient and do not describe media/ embedder customization. Callers should
// generally prefer the non-Default APIs above.
MEDIA_EXPORT bool IsDefaultSupportedAudioType(const AudioType& type);
MEDIA_EXPORT bool IsDefaultSupportedVideoType(const VideoType& type);

// This function describe if the specific video codec is a built into the binary
// or not.
MEDIA_EXPORT bool IsBuiltInVideoCodec(VideoCodec codec);

// This function lets the caller add additional codec profiles to those
// supported by default. Used primarily to add hardware codec profiles once
// support is known.
MEDIA_EXPORT void UpdateDefaultSupportedVideoProfiles(
    const base::flat_set<VideoCodecProfile>& profiles);

// This function lets the caller add additional codec and profile to those
// supported by default. Used primarily to add platform supported codecs once
// support is known.
MEDIA_EXPORT void UpdateDefaultSupportedAudioTypes(
    const base::flat_set<AudioType>& types);
}  // namespace media

#endif  // MEDIA_BASE_SUPPORTED_TYPES_H_
