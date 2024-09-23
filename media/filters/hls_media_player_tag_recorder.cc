// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_media_player_tag_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "media/base/media_serializers.h"
#include "media/filters/hls_network_access_impl.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"

namespace media {

namespace {

template <typename T>
void LogBitfieldHistogram(uint32_t bitfield, const char* name) {
  for (uint32_t i = 0; i < static_cast<uint32_t>(T::kMaxValue); i++) {
    if (bitfield & (1 << i)) {
      base::UmaHistogramEnumeration(name, static_cast<T>(i));
    }
  }
}

}  // namespace

HlsMediaPlayerTagRecorder::HlsMediaPlayerTagRecorder(
    std::unique_ptr<HlsNetworkAccess> network_access)
    : network_access_(std::move(network_access)) {}

HlsMediaPlayerTagRecorder::~HlsMediaPlayerTagRecorder() = default;

void HlsMediaPlayerTagRecorder::SetMetric(Metric metric) {
  switch (metric) {
    case hls::TagRecorder::Metric::kSegmentTS: {
      SetSegmentTypePresent(PlaylistSegmentType::kTS);
      break;
    }
    case hls::TagRecorder::Metric::kSegmentMP4: {
      SetSegmentTypePresent(PlaylistSegmentType::kMP4);
      break;
    }
    case hls::TagRecorder::Metric::kSegmentAAC: {
      SetSegmentTypePresent(PlaylistSegmentType::kAAC);
      break;
    }
    case hls::TagRecorder::Metric::kSegmentOther: {
      SetSegmentTypePresent(PlaylistSegmentType::kUnexpected);
      break;
    }
    case hls::TagRecorder::Metric::kContentSteering: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kContentSteering);
      break;
    }
    case hls::TagRecorder::Metric::kDiscontinuity: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kDiscontinuity);
      break;
    }
    case hls::TagRecorder::Metric::kDiscontinuitySequence: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kDiscontinuitySequence);
      break;
    }
    case hls::TagRecorder::Metric::kGap: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kGap);
      break;
    }
    case hls::TagRecorder::Metric::kKey: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kKey);
      break;
    }
    case hls::TagRecorder::Metric::kPart: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kPart);
      break;
    }
    case hls::TagRecorder::Metric::kSessionKey: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kSessionKey);
      break;
    }
    case hls::TagRecorder::Metric::kSkip: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kSkip);
      break;
    }
    case hls::TagRecorder::Metric::kUnknownTag: {
      SetAdvancedFeaturePresent(AdvancedFeatureTagType::kXNonStandard);
      break;
    }
    case hls::TagRecorder::Metric::kSegmentAES: {
      SetEncryptionModePresent(SegmentEncryptionMode::kSegmentAES);
      break;
    }
    case hls::TagRecorder::Metric::kSample: {
      SetEncryptionModePresent(SegmentEncryptionMode::kSampleAES);
      break;
    }
    case hls::TagRecorder::Metric::kNoCrypto: {
      SetEncryptionModePresent(SegmentEncryptionMode::kNone);
      break;
    }
    case hls::TagRecorder::Metric::kAESCTR: {
      SetEncryptionModePresent(SegmentEncryptionMode::kSampleAESCTR);
      break;
    }
    case hls::TagRecorder::Metric::kAESCENC: {
      SetEncryptionModePresent(SegmentEncryptionMode::kSampleAESCENC);
      break;
    }
    case hls::TagRecorder::Metric::kISO230017: {
      SetEncryptionModePresent(SegmentEncryptionMode::kISO230017);
      break;
    }
  }
}

void HlsMediaPlayerTagRecorder::RecordError(uint32_t err_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_ && !playlist_invalid_errorcode_.has_value()) {
    base::UmaHistogramSparse("Media.HLS.UnparsableManifest", err_code);
  }
  playlist_invalid_errorcode_ = err_code;
}

void HlsMediaPlayerTagRecorder::OnPlaylistFetch(
    GURL root_playlist_uri,
    HlsDataSourceProvider::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    return;
  }

  auto stream = std::move(result).value();
  auto m_info = hls::Playlist::IdentifyPlaylist(stream->AsString());
  if (!m_info.has_value()) {
    RecordError(static_cast<uint32_t>(std::move(m_info).error().code()));
    return;
  }

  SetTopLevelPlaylistType((*m_info).kind);
  switch ((*m_info).kind) {
    case hls::Playlist::Kind::kMultivariantPlaylist: {
      auto maybe = hls::MultivariantPlaylist::Parse(
          stream->AsString(), root_playlist_uri, (*m_info).version, this);
      if (!maybe.has_value()) {
        RecordError(static_cast<uint32_t>(std::move(maybe).error().code()));
        return;
      }
      break;
    }
    case hls::Playlist::Kind::kMediaPlaylist: {
      auto maybe =
          hls::MediaPlaylist::Parse(stream->AsString(), root_playlist_uri,
                                    (*m_info).version, nullptr, this);
      if (!maybe.has_value()) {
        RecordError(static_cast<uint32_t>(std::move(maybe).error().code()));
        return;
      }
      break;
    }
  }
}

void HlsMediaPlayerTagRecorder::SetTopLevelPlaylistType(
    hls::Playlist::Kind kind) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_ && !root_playlist_type_.has_value()) {
    base::UmaHistogramBoolean(
        "Media.HLS.MultivariantPlaylist",
        kind == hls::Playlist::Kind::kMultivariantPlaylist);
  }
  root_playlist_type_ = kind;
}

void HlsMediaPlayerTagRecorder::SetAdvancedFeaturePresent(
    AdvancedFeatureTagType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_ && !advanced_feature_bitfield_.has_value()) {
    base::UmaHistogramEnumeration("Media.HLS.AdvancedFeatureTags", type);
  }
  advanced_feature_bitfield_ = advanced_feature_bitfield_.value_or(0) |
                               (1 << static_cast<uint32_t>(type));
}

void HlsMediaPlayerTagRecorder::SetSegmentTypePresent(
    PlaylistSegmentType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_ && !playlist_segment_bitfield_.has_value()) {
    base::UmaHistogramEnumeration("Media.HLS.PlaylistSegmentExtension", type);
  }
  playlist_segment_bitfield_ = playlist_segment_bitfield_.value_or(0) |
                               (1 << static_cast<uint32_t>(type));
}

void HlsMediaPlayerTagRecorder::SetEncryptionModePresent(
    SegmentEncryptionMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_ && !segment_encryption_bitfield_.has_value()) {
    base::UmaHistogramEnumeration("Media.HLS.EncryptionMode", mode);
  }
  segment_encryption_bitfield_ = segment_encryption_bitfield_.value_or(0) |
                                 (1 << static_cast<uint32_t>(mode));
}

void HlsMediaPlayerTagRecorder::AllowRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_enabled_) {
    return;
  }
  recording_enabled_ = true;

  if (playlist_invalid_errorcode_.has_value()) {
    base::UmaHistogramSparse("Media.HLS.UnparsableManifest",
                             playlist_invalid_errorcode_.value());
  }
  if (root_playlist_type_.has_value()) {
    base::UmaHistogramBoolean("Media.HLS.MultivariantPlaylist",
                              root_playlist_type_.value() ==
                                  hls::Playlist::Kind::kMultivariantPlaylist);
  }
  if (advanced_feature_bitfield_.has_value()) {
    LogBitfieldHistogram<AdvancedFeatureTagType>(
        advanced_feature_bitfield_.value(), "Media.HLS.AdvancedFeatureTags");
  }
  if (playlist_segment_bitfield_.has_value()) {
    LogBitfieldHistogram<PlaylistSegmentType>(
        playlist_segment_bitfield_.value(),
        "Media.HLS.PlaylistSegmentExtension");
  }
  if (segment_encryption_bitfield_.has_value()) {
    LogBitfieldHistogram<SegmentEncryptionMode>(
        segment_encryption_bitfield_.value(), "Media.HLS.EncryptionMode");
  }
}

void HlsMediaPlayerTagRecorder::Start(GURL root_playlist_uri) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_access_->ReadManifest(
      root_playlist_uri,
      base::BindOnce(&HlsMediaPlayerTagRecorder::OnPlaylistFetch,
                     weak_factory_.GetWeakPtr(), root_playlist_uri));
}

}  // namespace media
