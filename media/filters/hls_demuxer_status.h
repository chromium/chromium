// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_
#define MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_

#include "base/functional/callback.h"
#include "media/base/status.h"

namespace media {

struct HlsDemuxerStatusTraits {
  enum class Codes : StatusCodeType {
    // Bitstream / codec / container statuses
    kPlaylistUrlInvalid,
    kInvalidBitstream,
    kUnsupportedContainer,
    kUnsupportedCodec,
    kEncryptedMediaNotSupported,

    // Manifest and playlist statuses
    kRecursiveMultivariantPlaylists,
    kNoRenditions,
    kInvalidManifest,
    kInvalidSegmentUri,
  };
  static constexpr StatusGroupType Group() { return "HlsDemuxerStatus"; }
};

using HlsDemuxerStatus = TypedStatus<HlsDemuxerStatusTraits>;

template <typename T>
using HlsDemuxerStatusCb = base::OnceCallback<void(HlsDemuxerStatus::Or<T>)>;

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_
