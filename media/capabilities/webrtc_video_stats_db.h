// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_H_
#define MEDIA_CAPABILITIES_WEBRTC_VIDEO_STATS_DB_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
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

    // Returns a concise string representation of the key without pixels for
    // querying the DB.
    std::string SerializeWithoutPixels() const;

    // For debug logging. NOT interchangeable with Serialize().
    std::string ToLogStringForDebug() const;

    static std::optional<int> ParsePixelsFromKey(std::string key);

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

  // VideoStatsCollection is used to return a collection of entries
  // corresponding to a certain key except for the number of pixels used. The
  // number of pixels is instead used as a key in the map for each
  // VideoStatsEntry.
  using VideoStatsCollection = base::flat_map<int, VideoStatsEntry>;

  virtual ~WebrtcVideoStatsDB() = default;

  // The maximum number of pixels that a stats key can have without being
  // considered out of range. This is used in sanity checks and is higher than
  // the maximum pixels value that can be saved to the database. For example, a
  // user may query the API for 8K resolution even though no data will be stored
  // for anything higher than 4K resolution.
  static constexpr int kPixelsAbsoluteMaxValue = 10 * 3840 * 2160;
  // The minimum number of pixels that a stats key can have to be considered to
  // be saved to the database. Set to 80% of the minimum pixels bucket.
  static constexpr int kPixelsMinValueToSave = 0.8 * 1280 * 720;
  // The maximum number of pixels that a stats key can have to be considered to
  // be saved to the database. Set to 120% of the largest pixels bucket.
  static constexpr int kPixelsMaxValueToSave = 1.2 * 3840 * 2160;
  // The minimum number of frames processed that a stats entry is based on. The
  // 99th percentile is not useful for anything less than 100 samples.
  static constexpr uint32_t kFramesProcessedMinValue = 100;
  // The maximum number of frames processed that a stats entry is based on.
  // Expected max number is around 30000.
  static constexpr uint32_t kFramesProcessedMaxValue = 60000;
  // Minimum valid 99th percentile of the processing time, which is either the
  // time needed for encoding or decoding.
  static constexpr float kP99ProcessingTimeMinValueMs = 0.0;
  // Maximum valid 99th percentile of the processing time, which is either the
  // time needed for encoding or decoding.
  static constexpr float kP99ProcessingTimeMaxValueMs = 10000.0;

  // Number of stats entries that are stored per configuration. The oldest
  // stats entry will be discarded when new stats are added if the list is
  // already full.
  static int GetMaxEntriesPerConfig();

  // Number of days after which a stats entry will be discarded. This
  // avoids users getting stuck with a bad capability prediction that may have
  // been due to one-off circumstances.
  static base::TimeDelta GetMaxTimeToKeepStats();

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

  // Returns the stats associated with `key`. The `get_stats_cb` will receive
  // the stats in addition to a boolean signaling if the call was successful.
  // VideoStatsEntry can be nullopt if there was no data associated with `key`.
  using GetVideoStatsCB =
      base::OnceCallback<void(bool, std::optional<VideoStatsEntry>)>;
  virtual void GetVideoStats(const VideoDescKey& key,
                             GetVideoStatsCB get_stats_cb) = 0;

  // Returns a collection of stats associated with `key` disregarding pixels.
  // The `get_stats_cb` will receive the stats in addition to a boolean
  // signaling if the call was successful. VideoStatsEntry can be nullopt if
  // there was no data associated with `key`.
  using GetVideoStatsCollectionCB =
      base::OnceCallback<void(bool, std::optional<VideoStatsCollection>)>;
  virtual void GetVideoStatsCollection(
      const VideoDescKey& key,
      GetVideoStatsCollectionCB get_stats_cb) = 0;

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
