// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TRACK_H_
#define MEDIA_BASE_MEDIA_TRACK_H_

#include <string>

#include "base/types/strong_alias.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class MEDIA_EXPORT MediaTrack {
 public:
  enum class Type { kAudio, kVideo };

  enum class VideoKind {
    kNone,
    kAlternative,
    kCaptions,
    kMain,
    kSign,
    kSubtitles,
    kCommentary
  };

  enum class AudioKind {
    kNone,
    kAlternative,
    kDescriptions,
    kMain,
    kMainDescriptions,
    kTranslation,
    kCommentary
  };

  using Id = base::StrongAlias<class IdTag, std::string>;
  using Kind = base::StrongAlias<class KindTag, std::string>;
  using Label = base::StrongAlias<class LabelTag, std::string>;
  using Language = base::StrongAlias<class LanguageTag, std::string>;

  static MediaTrack CreateVideoTrack(const std::string& id,
                                     VideoKind kind,
                                     const std::string& label,
                                     const std::string& language,
                                     bool enabled,
                                     StreamParser::TrackId stream_id = 0);

  static MediaTrack CreateAudioTrack(const std::string& id,
                                     AudioKind kind,
                                     const std::string& label,
                                     const std::string& language,
                                     bool enabled,
                                     StreamParser::TrackId stream_id = 0,
                                     bool exclusive = false);

  static MediaTrack::Kind VideoKindToString(MediaTrack::VideoKind kind);
  static MediaTrack::Kind AudioKindToString(MediaTrack::AudioKind kind);

  ~MediaTrack();
  MediaTrack(const MediaTrack&);
  MediaTrack& operator=(const MediaTrack&) = default;

  Type type() const { return type_; }
  const Id& track_id() const { return track_id_; }
  const Kind& kind() const { return kind_; }
  const Label& label() const { return label_; }
  const Language& language() const { return language_; }
  StreamParser::TrackId stream_id() const { return stream_id_; }
  bool enabled() const { return enabled_; }
  bool exclusive() const { return exclusive_; }

  void set_id(Id id) {
    DCHECK(track_id_.value().empty() && !id.value().empty());
    track_id_ = id;
  }

 private:
  friend class MediaTracks;

  MediaTrack(Type type,
             StreamParser::TrackId stream_id,
             const Id& track_id,
             const Kind& kind,
             const Label& label,
             const Language& lang,
             bool enabled,
             bool exclusive);

  Type type_;
  bool enabled_;

  // A video track is always exclusive. An audio track is exclusive only if it
  // is created that way. Exclusive audio tracks, when enabled, will disable
  // other audio tracks.
  bool exclusive_;

  // |stream_id_| is read from the bytestream and is guaranteed to be
  // unique only within the scope of single bytestream's initialization segment.
  // But we might have multiple bytestreams (MediaSource might have multiple
  // SourceBuffers attached to it, which translates into ChunkDemuxer having
  // multiple SourceBufferStates and multiple bytestreams) or subsequent init
  // segments may redefine the bytestream ids. Thus bytestream track ids are not
  // guaranteed to be unique at the Demuxer and HTMLMediaElement level. So we
  // generate truly unique media track |track_id_| on the Demuxer level.
  StreamParser::TrackId stream_id_;
  Id track_id_;

  // These properties are read from input streams by stream parsers as specified
  // in https://dev.w3.org/html5/html-sourcing-inband-tracks/.
  Kind kind_;
  Label label_;
  Language language_;
};

// Helper for logging.
MEDIA_EXPORT const char* TrackTypeToStr(MediaTrack::Type type);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TRACK_H_
