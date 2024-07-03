// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_MEDIA_PLAYER_TAG_RECORDER_H_
#define MEDIA_FILTERS_HLS_MEDIA_PLAYER_TAG_RECORDER_H_

#include "base/threading/sequence_bound.h"
#include "media/base/media_export.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/filters/hls_network_access.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/tag_recorder.h"

namespace media {

// A stripped down implementation of the HLS Manifest parser which can record
// statistics for playlists which are passed to Android's MediaPlayer.
class MEDIA_EXPORT HlsMediaPlayerTagRecorder : public hls::TagRecorder {
 public:
  explicit HlsMediaPlayerTagRecorder(
      std::unique_ptr<HlsNetworkAccess> network_access);

  ~HlsMediaPlayerTagRecorder() override;

  void Start(GURL root_playlist_uri);
  void AllowRecording();

  // TagRecorder implementation
  void SetMetric(Metric metric) override;
  void RecordError(uint32_t err_code) override;

 private:
  // Recorded in the AdvancedFeatureTagType histogram.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class AdvancedFeatureTagType : uint32_t {
    kContentSteering = 0,        // Multivariant
    kDiscontinuity = 1,          // Media
    kDiscontinuitySequence = 2,  // Media
    kGap = 3,                    // Media
    kKey = 4,                    // Media
    kPart = 5,                   // Media
    kSessionKey = 6,             // Multivariant
    kSkip = 7,                   // Media
    kXNonStandard = 8,           // Media,Multivariant
    kMaxValue = kXNonStandard,
  };

  // Recorded in the PlaylistSegmentType histogram.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class PlaylistSegmentType : uint32_t {
    kTS = 0,
    kMP4 = 1,
    kAAC = 2,
    kUnexpected = 3,
    kMaxValue = kUnexpected,
  };

  // Recorded in the SegmentEncryptionMode histogram.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class SegmentEncryptionMode : uint32_t {
    kNone = 0,
    kSegmentAES = 1,
    kSampleAES = 2,
    kSampleAESCTR = 3,
    kSampleAESCENC = 4,
    kISO230017 = 5,
    kMaxValue = kISO230017,
  };

  void OnPlaylistFetch(GURL root_playlist_uri,
                       HlsDataSourceProvider::ReadResult result);

  // Data recording methods
  void SetTopLevelPlaylistType(hls::Playlist::Kind kind);
  void SetAdvancedFeaturePresent(AdvancedFeatureTagType type);
  void SetSegmentTypePresent(PlaylistSegmentType type);
  void SetEncryptionModePresent(SegmentEncryptionMode mode);

  // Data saved, to be logged only if media player successfully can play.
  std::optional<uint32_t> advanced_feature_bitfield_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<uint32_t> playlist_segment_bitfield_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<uint32_t> segment_encryption_bitfield_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<uint32_t> playlist_invalid_errorcode_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<hls::Playlist::Kind> root_playlist_type_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Network access structure.
  std::unique_ptr<HlsNetworkAccess> network_access_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool recording_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Ensure that safe member fields are only accessed on the media sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsMediaPlayerTagRecorder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_MEDIA_PLAYER_TAG_RECORDER_H_
