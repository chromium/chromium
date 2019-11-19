// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/webm/webm_audio_client.h"
#include "media/formats/webm/webm_content_encodings_client.h"
#include "media/formats/webm/webm_parser.h"
#include "media/formats/webm/webm_video_client.h"

namespace media {

// Parser for WebM Tracks element.
class MEDIA_EXPORT WebMTracksParser : public WebMParserClient {
 public:
  WebMTracksParser(MediaLog* media_log, bool ignore_text_tracks);
  ~WebMTracksParser() override;

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t audio_track_num() const { return audio_track_num_; }
  int64_t video_track_num() const { return video_track_num_; }

  // If TrackEntry DefaultDuration field existed for the associated audio or
  // video track, returns that value converted from ns to base::TimeDelta with
  // precision not greater than |timecode_scale_in_ns|. Defaults to
  // kNoTimestamp.
  base::TimeDelta GetAudioDefaultDuration(
      const int64_t timecode_scale_in_ns) const;
  base::TimeDelta GetVideoDefaultDuration(
      const int64_t timecode_scale_in_ns) const;

  const std::set<int64_t>& ignored_tracks() const { return ignored_tracks_; }

  const std::string& audio_encryption_key_id() const {
    return audio_encryption_key_id_;
  }

  const AudioDecoderConfig& audio_decoder_config() {
    return audio_decoder_config_;
  }

  const std::string& video_encryption_key_id() const {
    return video_encryption_key_id_;
  }

  const VideoDecoderConfig& video_decoder_config() {
    return video_decoder_config_;
  }

  typedef std::map<int, TextTrackConfig> TextTracks;

  const TextTracks& text_tracks() const {
    return text_tracks_;
  }

  int detected_audio_track_count() const { return detected_audio_track_count_; }

  int detected_video_track_count() const { return detected_video_track_count_; }

  int detected_text_track_count() const { return detected_text_track_count_; }

  // Note: Calling media_tracks() method passes the ownership of the MediaTracks
  // object from WebMTracksParser to the caller (which is typically
  // WebMStreamParser object). So this method must be called only once, after
  // track parsing has been completed.
  std::unique_ptr<MediaTracks> media_tracks() {
    CHECK(media_tracks_.get());
    return std::move(media_tracks_);
  }

 private:
  // To test PrecisionCappedDefaultDuration.
  FRIEND_TEST_ALL_PREFIXES(WebMTracksParserTest, PrecisionCapping);

  // Returns the conversion of |duration_in_ns| to a microsecond-granularity
  // TimeDelta with precision no greater than |timecode_scale_in_ns|.
  // Returns kNoTimestamp if |duration_in_ns| is <= 0, or the capped precision
  // of the converted |duration_in_ns| is < 1 microsecond.
  // Commonly, |timecode_scale_in_ns| is 1000000 (1 millisecond), though the
  // muxed stream could have used a different time scale.
  base::TimeDelta PrecisionCappedDefaultDuration(
      const int64_t timecode_scale_in_ns,
      const int64_t duration_in_ns) const;

  void Reset();
  void ResetTrackEntry();

  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;
  bool OnString(int id, const std::string& str) override;

  bool reset_on_next_parse_;
  int64_t track_type_;
  int64_t track_num_;
  std::string track_name_;
  std::string track_language_;
  std::string codec_id_;
  std::vector<uint8_t> codec_private_;
  int64_t seek_preroll_;
  int64_t codec_delay_;
  int64_t default_duration_;
  std::unique_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64_t audio_track_num_;
  int64_t audio_default_duration_;
  int64_t video_track_num_;
  int64_t video_default_duration_;
  bool ignore_text_tracks_;
  TextTracks text_tracks_;
  std::set<int64_t> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  MediaLog* media_log_;

  WebMAudioClient audio_client_;
  AudioDecoderConfig audio_decoder_config_;

  WebMVideoClient video_client_;
  VideoDecoderConfig video_decoder_config_;

  int detected_audio_track_count_;
  int detected_video_track_count_;
  int detected_text_track_count_;
  std::unique_ptr<MediaTracks> media_tracks_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
