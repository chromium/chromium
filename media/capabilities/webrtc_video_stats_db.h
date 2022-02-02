// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_H_
#define MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/check.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This defines the interface to be used by various media capabilities services
// to store/retrieve video encoding and decoding performance statistics.
class MEDIA_EXPORT WebrtcVideoStatsDB {
 public:
  // Simple description of video encode/decode complexity, serving as a key to
  // look up associated VideoStatsEntries in the database.
  struct MEDIA_EXPORT VideoDescKey {
    static VideoDescKey MakeBucketedKey(bool is_decode_stats,
                                        VideoCodecProfile codec_profile,
                                        bool hardware_accelerated,
                                        int pixels);

    // Returns a concise string representation of the key for storing in DB.
    std::string Serialize() const;

    // For debug logging. NOT interchangeable with Serialize().
    std::string ToLogStringForDebug() const;

    // Note: operator == and != are defined outside this class.
    const bool is_decode_stats;
    const VideoCodecProfile codec_profile;
    const bool hardware_accelerated;
    const int pixels;

   private:
    // All key's should be "bucketed" using MakeBucketedKey(...).
    VideoDescKey(bool is_decode_stats,
                 VideoCodecProfile codec_profile,
                 bool hardware_accelerated,
                 int pixels);
  };

  struct MEDIA_EXPORT VideoStats {
    VideoStats(uint32_t frames_processed,
               uint32_t key_frames_processed,
               float p99_processing_time_ms);
    VideoStats(double timestamp,
               uint32_t frames_processed,
               uint32_t key_frames_processed,
               float p99_processing_time_ms);
    VideoStats(const VideoStats& entry);
    VideoStats& operator=(const VideoStats& entry);

    // For debug logging.
    std::string ToLogString() const;

    // Note: operator == and != are defined outside this class.
    double timestamp;
    uint32_t frames_processed;
    uint32_t key_frames_processed;
    float p99_processing_time_ms;
  };

  // VideoStatsEntry saved to identify the capabilities related to a given
  // |VideoDescKey|.
  using VideoStatsEntry = std::vector<VideoStats>;

  virtual ~WebrtcVideoStatsDB() = default;

  // Run asynchronous initialization of database. Initialization must complete
  // before calling other APIs. |init_cb| must not be
  // a null callback.
  using InitializeCB = base::OnceCallback<void(bool)>;
  virtual void Initialize(InitializeCB init_cb) = 0;

  // Appends `video_stats` to existing entry associated with `key`. Will create
  // a new entry if none exists. The operation is asynchronous. The caller
  // should be aware of potential race conditions when calling this method for
  // the same `key` very close to other calls. `append_done_cb` will be run with
  // a bool to indicate whether the save succeeded.
  using AppendVideoStatsCB = base::OnceCallback<void(bool)>;
  virtual void AppendVideoStats(const VideoDescKey& key,
                                const VideoStats& video_stats,
                                AppendVideoStatsCB append_done_cb) = 0;

  // Returns the stats  associated with `key`. The `get_stats_cb` will receive
  // the stats in addition to a boolean signaling if the call was successful.
  // VideoStatsEntry can be nullptr if there was no data associated with `key`.
  using GetVideoStatsCB =
      base::OnceCallback<void(bool, std::unique_ptr<VideoStatsEntry>)>;
  virtual void GetVideoStats(const VideoDescKey& key,
                             GetVideoStatsCB get_stats_cb) = 0;

  // Clear all statistics from the DB.
  virtual void ClearStats(base::OnceClosure clear_done_cb) = 0;
};

MEDIA_EXPORT bool operator==(const WebrtcVideoStatsDB::VideoDescKey& x,
                             const WebrtcVideoStatsDB::VideoDescKey& y);
MEDIA_EXPORT bool operator!=(const WebrtcVideoStatsDB::VideoDescKey& x,
                             const WebrtcVideoStatsDB::VideoDescKey& y);
MEDIA_EXPORT bool operator==(const WebrtcVideoStatsDB::VideoStats& x,
                             const WebrtcVideoStatsDB::VideoStats& y);
MEDIA_EXPORT bool operator!=(const WebrtcVideoStatsDB::VideoStats& x,
                             const WebrtcVideoStatsDB::VideoStats& y);

}  // namespace media

#endif  // MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_H_
