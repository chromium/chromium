// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/webm/webm_parser.h"
#include "media/formats/webm/webm_tracks_parser.h"

namespace media {

class MEDIA_EXPORT WebMClusterParser : public WebMParserClient {
 public:
  using TrackId = StreamParser::TrackId;
  using BufferQueue = base::circular_deque<scoped_refptr<StreamParserBuffer>>;
  using TextBufferQueueMap = std::map<TrackId, const BufferQueue>;

  // Numbers chosen to estimate the duration of a buffer if none is set and
  // there is not enough information to get a better estimate.
  enum {
    // Common 1k samples @44.1kHz
    kDefaultAudioBufferDurationInMs = 23,

    // Chosen to represent 16fps duration, which will prevent MSE stalls in
    // videos with frame-rates as low as 8fps.
    kDefaultVideoBufferDurationInMs = 63
  };

  // Opus packets encode the duration and other parameters in the 5 most
  // significant bits of the first byte. The index in this array corresponds
  // to the duration of each frame of the packet in microseconds. See
  // https://tools.ietf.org/html/rfc6716#page-14
  static const uint16_t kOpusFrameDurationsMu[];

 private:
  typedef StreamParserBuffer::Type TrackType;

  // Helper class that manages per-track state.
  class Track {
   public:
    Track(int track_num,
          TrackType track_type,
          base::TimeDelta default_duration,
          MediaLog* media_log);
    Track(const Track& other);
    ~Track();

    int track_num() const { return track_num_; }

    // If a buffer is currently held aside pending duration calculation, returns
    // its decode timestamp. Otherwise, returns kInfiniteDuration.
    DecodeTimestamp GetReadyUpperBound();

    // Prepares |ready_buffers_| for retrieval. Prior to calling,
    // |ready_buffers_| must be empty. Moves all |buffers_| with decode
    // timestamp before |before_timestamp| to |ready_buffers_|, preserving their
    // order.
    void ExtractReadyBuffers(const DecodeTimestamp before_timestamp);

    const BufferQueue& ready_buffers() const { return ready_buffers_; }

    // If |last_added_buffer_missing_duration_| is set, updates its duration
    // relative to |buffer|'s timestamp, and adds it to |buffers_| and unsets
    // |last_added_buffer_missing_duration_|. Then, if |buffer| is missing
    // duration, saves |buffer| into |last_added_buffer_missing_duration_|, or
    // otherwise adds |buffer| to |buffers_|.
    bool AddBuffer(scoped_refptr<StreamParserBuffer> buffer);

    // If |last_added_buffer_missing_duration_| is set, updates its duration to
    // be non-kNoTimestamp value of |{min|max}_frame_duration_| or a hard-coded
    // default, then adds it to |buffers_| and unsets
    // |last_added_buffer_missing_duration_|. (This method helps stream parser
    // emit all buffers in a media segment before signaling end of segment.)
    void ApplyDurationEstimateIfNeeded();

    // Clears |ready_buffers_| (use ExtractReadyBuffers() to fill it again).
    // Leaves as-is |buffers_| and any possibly held-aside buffer that is
    // missing duration.
    void ClearReadyBuffers();

    // Clears all buffer state, including any possibly held-aside buffer that
    // was missing duration, and all contents of |buffers_| and
    // |ready_buffers_|.
    void Reset();

    base::TimeDelta default_duration() const { return default_duration_; }

   private:
    // Helper that sanity-checks |buffer| duration, updates
    // |{min|max}_frame_duration_|, and adds |buffer| to |buffers_|. Returns
    // false if |buffer| failed sanity check and therefore was not added to
    // |buffers_|. Returns true otherwise.
    bool QueueBuffer(scoped_refptr<StreamParserBuffer> buffer);

    // Helper that calculates the buffer duration to use in
    // ApplyDurationEstimateIfNeeded().
    base::TimeDelta GetDurationEstimate();

    // Counts the number of estimated durations used in this track. Used to
    // prevent log spam for MEDIA_LOG()s about estimated duration.
    int num_duration_estimates_ = 0;

    int track_num_;
    TrackType track_type_;

    // Parsed track buffers, each with duration and in (decode) timestamp order,
    // that have not yet been extracted into |ready_buffers_|. Note that up to
    // one additional buffer missing duration may be tracked by
    // |last_added_buffer_missing_duration_|.
    BufferQueue buffers_;
    scoped_refptr<StreamParserBuffer> last_added_buffer_missing_duration_;

    // Buffers in (decode) timestamp order that were previously parsed into and
    // extracted from |buffers_|. Buffers are moved from |buffers_| to
    // |ready_buffers_| by ExtractReadyBuffers() if they are below a specified
    // upper bound timestamp. Track users can therefore extract only those
    // parsed buffers which are "ready" for emission (all before some maximum
    // timestamp).
    BufferQueue ready_buffers_;

    // If kNoTimestamp, then |{min|max}_frame_duration_| will be used.
    base::TimeDelta default_duration_;

    // Tracks the max duration seen for this track. Used to estimate block
    // durations at the end of clusters. Max minimizes the chance of introducing
    // gaps in the buffered range and is safe for audio because we don't splice
    // on estimated durations. See http://crbug.com/396634.
    base::TimeDelta max_frame_duration_;

    MediaLog* media_log_;
  };

  typedef std::map<int, Track> TextTrackMap;

 public:
  WebMClusterParser(int64_t timecode_scale_ns,
                    int audio_track_num,
                    base::TimeDelta audio_default_duration,
                    int video_track_num,
                    base::TimeDelta video_default_duration,
                    const WebMTracksParser::TextTracks& text_tracks,
                    const std::set<int64_t>& ignored_tracks,
                    const std::string& audio_encryption_key_id,
                    const std::string& video_encryption_key_id,
                    const AudioCodec audio_codec,
                    MediaLog* media_log);
  ~WebMClusterParser() override;

  // Resets the parser state so it can accept a new cluster.
  void Reset();

  // Parses a WebM cluster element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  base::TimeDelta cluster_start_time() const { return cluster_start_time_; }

  // Get the current ready buffers resulting from Parse().
  // If the parse reached the end of cluster and the last buffer was held aside
  // due to missing duration, the buffer is given an estimated duration and
  // included in the result.
  // Otherwise, if there are is a buffer held aside due to missing duration for
  // any of the tracks, no buffers with same or greater (decode) timestamp will
  // be included in the buffers.
  // The returned deques are cleared by Parse() or Reset() and updated by the
  // next calls to Get{Audio,Video}Buffers().
  // If no Parse() or Reset() has occurred since the last call to Get{Audio,
  // Video,Text}Buffers(), then the previous BufferQueue& is returned again
  // without any recalculation.
  void GetBuffers(StreamParser::BufferQueueMap* buffers);

  // Constructs and returns a subset of |text_track_map_| containing only
  // tracks with non-empty buffer queues produced by the last Parse() and
  // filtered to exclude any buffers that have (decode) timestamp same or
  // greater than the lowest (decode) timestamp across all tracks of any buffer
  // held aside due to missing duration (unless the end of cluster has been
  // reached).
  // The returned map is cleared by Parse() or Reset() and updated by the next
  // call to GetTextBuffers().
  // If no Parse() or Reset() has occurred since the last call to
  // GetTextBuffers(), then the previous TextBufferQueueMap& is returned again
  // without any recalculation.
  const TextBufferQueueMap& GetTextBuffers();

  // Returns true if the last Parse() call stopped at the end of a cluster.
  bool cluster_ended() const { return cluster_ended_; }

 private:
  // WebMParserClient methods.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;

  bool ParseBlock(bool is_simple_block,
                  const uint8_t* buf,
                  int size,
                  const uint8_t* additional,
                  int additional_size,
                  int duration,
                  int64_t discard_padding,
                  bool reference_block_set);
  bool OnBlock(bool is_simple_block,
               int track_num,
               int timecode,
               int duration,
               const uint8_t* data,
               int size,
               const uint8_t* additional,
               int additional_size,
               int64_t discard_padding,
               bool is_keyframe);

  // Resets the Track objects associated with each text track.
  void ResetTextTracks();

  // Clears the the ready buffers associated with each text track.
  void ClearTextTrackReadyBuffers();

  // Helper method for Get{Audio,Video,Text}Buffers() that recomputes
  // |ready_buffer_upper_bound_| and calls ExtractReadyBuffers() on each track.
  // If |cluster_ended_| is true, first applies duration estimate if needed for
  // |audio_| and |video_| and sets |ready_buffer_upper_bound_| to
  // kInfiniteDuration. Otherwise, sets |ready_buffer_upper_bound_| to the
  // minimum upper bound across |audio_| and |video_|. (Text tracks can have no
  // buffers missing duration, so they are not involved in calculating the upper
  // bound.)
  // Parse() or Reset() must be called between calls to UpdateReadyBuffers() to
  // clear each track's ready buffers and to reset |ready_buffer_upper_bound_|
  // to kNoDecodeTimestamp().
  void UpdateReadyBuffers();

  // Search for the indicated track_num among the text tracks.  Returns NULL
  // if that track num is not a text track.
  Track* FindTextTrack(int track_num);

  // Attempts to read the duration from the encoded audio data, returning as
  // TimeDelta or kNoTimestamp if duration cannot be retrieved. This obviously
  // violates layering rules, but is useful for MSE to know duration in cases
  // where it isn't explicitly given and cannot be calculated for Blocks at the
  // end of a Cluster (the next Cluster in playback-order may not be the next
  // Cluster we parse, so we can't simply use the delta of the first Block in
  // the next Cluster). Avoid calling if encrypted; may produce unexpected
  // output. See implementation for supported codecs.
  base::TimeDelta TryGetEncodedAudioDuration(const uint8_t* data, int size);

  // Reads Opus packet header to determine packet duration. Duration returned
  // as TimeDelta or kNoTimestamp upon failure to read duration from packet.
  base::TimeDelta ReadOpusDuration(const uint8_t* data, int size);

  // Tracks the number of MEDIA_LOGs made in process of reading encoded
  // duration. Useful to prevent log spam.
  int num_duration_errors_ = 0;

  double timecode_multiplier_;  // Multiplier used to convert timecodes into
                                // microseconds.
  std::set<int64_t> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  const AudioCodec audio_codec_;

  WebMListParser parser_;

  int64_t last_block_timecode_ = -1;
  std::unique_ptr<uint8_t[]> block_data_;
  int block_data_size_ = -1;
  int64_t block_duration_ = -1;
  int64_t block_add_id_ = -1;

  std::unique_ptr<uint8_t[]> block_additional_data_;
  // Must be 0 if |block_additional_data_| is null. Must be > 0 if
  // |block_additional_data_| is NOT null.
  int block_additional_data_size_ = 0;

  int64_t discard_padding_ = -1;
  bool discard_padding_set_ = false;

  bool reference_block_set_ = false;

  int64_t cluster_timecode_ = -1;
  base::TimeDelta cluster_start_time_;
  bool cluster_ended_ = false;

  Track audio_;
  Track video_;
  TextTrackMap text_track_map_;

  // Subset of |text_track_map_| maintained by GetTextBuffers(), and cleared by
  // ClearTextTrackReadyBuffers(). Callers of GetTextBuffers() get a const-ref
  // to this member.
  TextBufferQueueMap text_buffers_map_;

  // Limits the range of buffers returned by Get{Audio,Video,Text}Buffers() to
  // this exclusive upper bound. Set to kNoDecodeTimestamp(), meaning not yet
  // calculated, by Reset() and Parse(). If kNoDecodeTimestamp(), then
  // Get{Audio,Video,Text}Buffers() will calculate it to be the minimum (decode)
  // timestamp across all tracks' |last_buffer_missing_duration_|, or
  // kInfiniteDuration if no buffers are currently missing duration.
  DecodeTimestamp ready_buffer_upper_bound_;

  MediaLog* media_log_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMClusterParser);
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CLUSTER_PARSER_H_
