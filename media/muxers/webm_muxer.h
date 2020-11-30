// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_WEBM_MUXER_H_
#define MEDIA_MUXERS_WEBM_MUXER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "third_party/libwebm/source/mkvmuxer.hpp"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media {

class VideoFrame;
class AudioParameters;

// Adapter class to manage a WebM container [1], a simplified version of a
// Matroska container [2], composed of an EBML header, and a single Segment
// including at least a Track Section and a number of SimpleBlocks each
// containing a single encoded video or audio frame. WebM container has no
// Trailer.
// Clients will push encoded VPx or AV1 video frames and Opus or PCM audio
// frames one by one via OnEncoded{Video|Audio}(). libwebm will eventually ping
// the WriteDataCB passed on contructor with the wrapped encoded data.
// WebmMuxer is designed for use on a single thread.
// [1] http://www.webmproject.org/docs/container/
// [2] http://www.matroska.org/technical/specs/index.html
class MEDIA_EXPORT WebmMuxer : public mkvmuxer::IMkvWriter {
 public:
  // Callback to be called when WebmMuxer is ready to write a chunk of data,
  // either any file header or a SingleBlock.
  using WriteDataCB = base::RepeatingCallback<void(base::StringPiece)>;

  // Container for the parameters that muxer uses that is extracted from
  // media::VideoFrame.
  struct MEDIA_EXPORT VideoParameters {
    explicit VideoParameters(scoped_refptr<media::VideoFrame> frame);
    VideoParameters(gfx::Size visible_rect_size,
                    double frame_rate,
                    VideoCodec codec,
                    base::Optional<gfx::ColorSpace> color_space);
    VideoParameters(const VideoParameters&);
    ~VideoParameters();
    gfx::Size visible_rect_size;
    double frame_rate;
    VideoCodec codec;
    base::Optional<gfx::ColorSpace> color_space;
  };

  // |audio_codec| should coincide with whatever is sent in OnEncodedAudio(),
  WebmMuxer(AudioCodec audio_codec,
            bool has_video_,
            bool has_audio_,
            const WriteDataCB& write_data_callback);
  ~WebmMuxer() override;

  // Sets the maximum duration interval to cause data output on
  // |write_data_callback|, provided frames are delivered. The WebM muxer can
  // hold on to audio frames almost indefinitely in the case video is recorded
  // and video frames are temporarily not delivered. When this method is used, a
  // new WebM cluster is forced when the next frame arrives |duration| after the
  // last write.
  // The maximum duration between forced clusters is internally limited to not
  // go below 100 ms.
  void SetMaximumDurationToForceDataOutput(base::TimeDelta interval);

  // Functions to add video and audio frames with |encoded_data.data()|
  // to WebM Segment. Either one returns true on success.
  // |encoded_alpha| represents the encode output of alpha channel when
  // available, can be nullptr otherwise.
  bool OnEncodedVideo(const VideoParameters& params,
                      std::string encoded_data,
                      std::string encoded_alpha,
                      base::TimeTicks timestamp,
                      bool is_key_frame);
  bool OnEncodedAudio(const media::AudioParameters& params,
                      std::string encoded_data,
                      base::TimeTicks timestamp);

  // WebmMuxer may hold on to data. Make sure it gets out on the next frame.
  void ForceDataOutputOnNextFrame();

  // Call to handle mute and tracks getting disabled.
  void SetLiveAndEnabled(bool track_live_and_enabled, bool is_video);

  void Pause();
  void Resume();

  // Drains and writes out all buffered frames and finalizes the segment.
  // Returns true on success, false otherwise.
  bool Flush();

  void ForceOneLibWebmErrorForTesting() { force_one_libwebm_error_ = true; }

 private:
  friend class WebmMuxerTest;

  // Methods for creating and adding video and audio tracks, called upon
  // receiving the first frame of a given Track.
  // AddVideoTrack adds |frame_size| and |frame_rate| to the Segment
  // info, although individual frames passed to OnEncodedVideo() can have any
  // frame size.
  void AddVideoTrack(const gfx::Size& frame_size,
                     double frame_rate,
                     const base::Optional<gfx::ColorSpace>& color_space);
  void AddAudioTrack(const media::AudioParameters& params);

  // IMkvWriter interface.
  mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) override;
  mkvmuxer::int64 Position() const override;
  mkvmuxer::int32 Position(mkvmuxer::int64 position) override;
  bool Seekable() const override;
  void ElementStartNotify(mkvmuxer::uint64 element_id,
                          mkvmuxer::int64 position) override;

  // Adds all currently buffered frames to the mkvmuxer in timestamp order,
  // until the queues are depleted.
  void FlushQueues();
  // Flushes out frames to the mkvmuxer while ensuring monotonically increasing
  // timestamps as per the WebM specification,
  // https://www.webmproject.org/docs/container/. Returns true on success and
  // false on mkvmuxer failure.
  //
  // Note that frames may still be around in the queues after this call. The
  // method stops flushing when timestamp monotonicity can't be guaranteed
  // anymore.
  bool PartiallyFlushQueues();
  // Flushes out the next frame in timestamp order from the queues. Returns true
  // on success and false on mkvmuxer failure.
  //
  // Note: it's assumed that at least one video or audio frame is queued.
  bool FlushNextFrame();
  // Calculates a monotonically increasing timestamp from an input |timestamp|
  // and a pointer to a previously stored |last_timestamp| by taking the maximum
  // of |timestamp| and *|last_timestamp|. Updates *|last_timestamp| if
  // |timestamp| is greater.
  base::TimeTicks UpdateLastTimestampMonotonically(
      base::TimeTicks timestamp,
      base::TimeTicks* last_timestamp);
  // Forces data output from |segment_| on the next frame if recording video,
  // and |min_data_output_interval_| was configured and has passed since the
  // last received video frame.
  void MaybeForceNewCluster();

  // Audio codec configured on construction. Video codec is taken from first
  // received frame.
  const AudioCodec audio_codec_;
  VideoCodec video_codec_;

  // Caller-side identifiers to interact with |segment_|, initialised upon
  // first frame arrival to Add{Video, Audio}Track().
  uint8_t video_track_index_;
  uint8_t audio_track_index_;

  // Origin of times for frame timestamps.
  base::TimeTicks first_frame_timestamp_video_;
  base::TimeTicks last_frame_timestamp_video_;
  base::TimeTicks first_frame_timestamp_audio_;
  base::TimeTicks last_frame_timestamp_audio_;

  // Variables to measure and accumulate, respectively, the time in pause state.
  std::unique_ptr<base::ElapsedTimer> elapsed_time_in_pause_;
  base::TimeDelta total_time_in_pause_;

  // TODO(ajose): Change these when support is added for multiple tracks.
  // http://crbug.com/528523
  const bool has_video_;
  const bool has_audio_;

  // Variables to track live and enabled state of audio and video.
  bool video_track_live_and_enabled_ = true;
  bool audio_track_live_and_enabled_ = true;

  // Maximum interval between data output callbacks (given frames arriving)
  base::TimeDelta max_data_output_interval_;

  // Last time data was output from |segment_|.
  base::TimeTicks last_data_output_timestamp_;

  // Last timestamp written into the segment.
  base::TimeDelta last_timestamp_written_;

  // Callback to dump written data as being called by libwebm.
  const WriteDataCB write_data_callback_;

  // Rolling counter of the position in bytes of the written goo.
  base::CheckedNumeric<mkvmuxer::int64> position_;

  // The MkvMuxer active element.
  mkvmuxer::Segment segment_;
  // Flag to force the next call to a |segment_| method to return false.
  bool force_one_libwebm_error_;

  struct EncodedFrame {
    std::string data;
    std::string alpha_data;
    base::TimeDelta
        relative_timestamp;  // relative to first_frame_timestamp_xxx_
    bool is_keyframe;
  };

  // The following two queues hold frames to ensure that monotonically
  // increasing timestamps are stored in the resulting webm file without
  // modifying the timestamps.
  base::circular_deque<EncodedFrame> audio_frames_;
  // If muxing audio and video, this queue holds frames until the first audio
  // frame appears.
  base::circular_deque<EncodedFrame> video_frames_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebmMuxer);
};

}  // namespace media

#endif  // MEDIA_MUXERS_WEBM_MUXER_H_
