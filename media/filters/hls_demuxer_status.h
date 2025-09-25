// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_
#define MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_

#include "base/functional/callback.h"
#include "media/base/status.h"

namespace media {

struct HlsDemuxerStatusTraits {
  // WARNING: These values are reported to metrics. Entries should not be
  // renumbered and numeric values should not be reused. When adding new
  // entries, tools/metrics/histograms/enums.xml.
  enum class Codes : StatusCodeType {
    kOk = 0,

    // The segment was fetched, but there are no bytes
    kNoSegmentData = 1,

    // The manifest metadata is not sufficient to parse the content.
    kInsufficientCryptoMetadata = 2,

    // The decryption step failed!
    kFailedToDecryptSegment = 3,

    // The method type (AES, none, etc...) was not supported.
    kUnsupportedCryptoMethod = 4,

    // The container format (mp4, ts, etc...) was not supported
    kUnsupportedContainer = 5,

    // Multivariant playlists aren't allowed to load other multivariant
    // playlists. This will change slightly once interstitials are supported.
    kRecursiveMultivariantPlaylists = 6,

    // There are no supported renditions for this multivariant playlist.
    kNoRenditions = 7,

    // The manifest failed to parse. This should always be bundled with a
    // hls::ParseStatus cause, and should often record that cause to UMA.
    kInvalidManifest = 8,

    // Wrapper statused for the HLS network stack error codes:
    //  - Error: There was an error (404, CORS, https downgrade, etc)
    //  - Aborted: The HLS demuxer aborted it's own network call
    //  - Stopped: The request was made after the network stack was stopped.
    kNetworkReadError = 9,
    kNetworkReadAborted = 10,
    kNetworkReadStopped = 11,

    // When fetching an updated media playlist for live content, a multivariant
    // playlist was returned instead.
    kUpdateRequiresMediaPlaylist = 12,

    // Adding a new role to chunk demuxer failed because the mime type was bad.
    kInvalidMimeType = 13,

    // We detected one of the renditions as live and one as VOD, and these cant
    // mix together.
    kMixedVodLiveRenditions = 14,

    // The playback rate for live content is only allowed at 1 or 0.
    kInvalidLivePlaybackRate = 15,

    // The loaded ranges are after the media time, so we won't be able to serve
    // content.
    kInvalidLoadedRanges = 16,

    // Data could not be appended to chunk demuxer.
    kCouldNotAppendData = 17,

    // When no data was ever loaded from segments, we should fail with this
    // error during initialization.
    kNoDataEverAppended = 18,

    // Keep this at the end and equal to the highest value ever used.
    kMaxValue = kNoDataEverAppended,
  };

  static constexpr StatusGroupType Group() { return "HlsDemuxerStatus"; }

  static constexpr std::string ReadableCodeName(Codes code) {
    switch (code) {
      case Codes::kOk:
        return "OK";
      case Codes::kNoSegmentData:
        return "No segment data";
      case Codes::kInsufficientCryptoMetadata:
        return "Insufficient crypto metadata";
      case Codes::kFailedToDecryptSegment:
        return "Failed to decrypt segment";
      case Codes::kUnsupportedCryptoMethod:
        return "Unsupported crypto method";
      case Codes::kUnsupportedContainer:
        return "Unsupported container";
      case Codes::kRecursiveMultivariantPlaylists:
        return "Recursive multivariant playlists";
      case Codes::kNoRenditions:
        return "No renditions";
      case Codes::kInvalidManifest:
        return "Invalid manifest";
      case Codes::kNetworkReadError:
        return "Network read error";
      case Codes::kNetworkReadAborted:
        return "Network read aborted";
      case Codes::kNetworkReadStopped:
        return "Network read stopped";
      case Codes::kUpdateRequiresMediaPlaylist:
        return "Update requires media playlist";
      case Codes::kInvalidMimeType:
        return "Invalid mime type";
      case Codes::kMixedVodLiveRenditions:
        return "Mixed VOD and live renditions";
      case Codes::kInvalidLivePlaybackRate:
        return "Invalid live playback rate";
      case Codes::kInvalidLoadedRanges:
        return "Loaded ranges conflict with media time";
      case Codes::kCouldNotAppendData:
        return "Could not append data to ChunkDemuxer";
      case Codes::kNoDataEverAppended:
        return "Segment queue never had any data";
    }
  }

  static TypedStatus<HlsDemuxerStatusTraits> FromReadStatus(
      HlsDataSourceProvider::ReadStatus&& s) {
    switch (s.code()) {
      case HlsDataSourceProvider::ReadStatus::Codes::kError:
        return {Codes::kNetworkReadError, std::move(s)};
      case HlsDataSourceProvider::ReadStatus::Codes::kAborted:
        return {Codes::kNetworkReadAborted, std::move(s)};
      case HlsDataSourceProvider::ReadStatus::Codes::kStopped:
        return {Codes::kNetworkReadStopped, std::move(s)};
    }
  }
};

using HlsDemuxerStatus = TypedStatus<HlsDemuxerStatusTraits>;

template <typename T>
using HlsDemuxerStatusCb = base::OnceCallback<void(HlsDemuxerStatus::Or<T>)>;

using HlsDemuxerStatusCallback = base::OnceCallback<void(HlsDemuxerStatus)>;

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DEMUXER_STATUS_H_
