// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_WEBM_MUXER_H_
#define MEDIA_MUXERS_WEBM_MUXER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "media/muxers/muxer.h"
#include "third_party/libwebm/source/mkvmuxer.hpp"

namespace media {

class AudioParameters;

// Adapter class to manage a WebM container [1], a simplified version of a
// Matroska container [2], composed of an EBML header, and a single Segment
// including at least a Track Section and a number of SimpleBlocks each
// containing a single encoded video or audio frame. WebM container has no
// Trailer.
// Clients will push encoded VPx or AV1 video frames and Opus or PCM audio
// frames one by one via OnEncoded{Video|Audio}(). libwebm will eventually ping
// the WriteDataCB passed on constructor with the wrapped encoded data.
// WebmMuxer is designed for use on a single thread.
// [1] http://www.webmproject.org/docs/container/
// [2] http://www.matroska.org/technical/specs/index.html
class MEDIA_EXPORT WebmMuxer : public Muxer {
 public:
  // Defines an interface for delegates of WebmMuxer which should define how to
  // implement the |mkvmuxer::IMkvWriter| APIs (e.g. whether to support
  // non-seekable live mode writing, or seekable file mode writing).
  class MEDIA_EXPORT Delegate : public mkvmuxer::IMkvWriter {
   public:
    Delegate();
    ~Delegate() override;

    base::TimeTicks last_data_output_timestamp() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return last_data_output_timestamp_;
    }

    // Initializes the given |segment| according to the mode desired by the
    // concrete implementation of this delegate.
    virtual void InitSegment(mkvmuxer::Segment* segment) = 0;

    // mkvmuxer::IMkvWriter:
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final;

   protected:
    // Does the actual writing of |len| bytes from the given |buf| depending on
    // the mode desired by the concrete implementation of this delegate.
    // Returns 0 on success, -1 otherwise.
    virtual mkvmuxer::int32 DoWrite(const void* buf, mkvmuxer::uint32 len) = 0;

    SEQUENCE_CHECKER(sequence_checker_);

    // The current writing position as set by libwebm.
    base::CheckedNumeric<mkvmuxer::int64> position_
        GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    // Last time data was written via Write().
    base::TimeTicks last_data_output_timestamp_
        GUARDED_BY_CONTEXT(sequence_checker_);
  };

  // `audio_codec` should coincide with whatever is sent in OnEncodedAudio(),
  // If set, `max_data_output_interval` indicates the allowed maximum time for
  // data output into the delegate provided frames are provided.
  WebmMuxer(AudioCodec audio_codec,
            bool has_video_,
            bool has_audio_,
            std::unique_ptr<Delegate> delegate,
            std::optional<base::TimeDelta> max_data_output_interval);

  WebmMuxer(const WebmMuxer&) = delete;
  WebmMuxer& operator=(const WebmMuxer&) = delete;
  ~WebmMuxer() override;

  // Drains and writes out all buffered frames and finalizes the segment.
  // Returns true on success, false otherwise.
  bool Flush() override;
  bool PutFrame(EncodedFrame frame,
                base::TimeDelta relative_timestamp) override;

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
                     const std::optional<gfx::ColorSpace>& color_space);
  void AddAudioTrack(const AudioParameters& params);
  bool WriteWebmFrame(EncodedFrame frame, base::TimeDelta relative_timestamp);

  // Forces data output from |segment_| on the next frame if recording video,
  // and |min_data_output_interval_| was configured and has passed since the
  // last received video frame.
  void MaybeForceNewCluster();

  // Audio codec configured on construction. Video codec is taken from first
  // received frame.
  const AudioCodec audio_codec_;
  VideoCodec video_codec_ = VideoCodec::kUnknown;

  // Caller-side identifiers to interact with |segment_|, initialised upon
  // first frame arrival to Add{Video, Audio}Track().
  uint8_t video_track_index_ = 0;
  uint8_t audio_track_index_ = 0;

  // TODO(ajose): Change these when support is added for multiple tracks.
  // http://crbug.com/528523
  const bool has_video_;
  const bool has_audio_;

  // Maximum interval between data output callbacks (given frames arriving).
  // The muxer can hold on to audio frames almost indefinitely in the case video
  // is recorded and video frames are temporarily not delivered. When this
  // method is used, a new WebM cluster is forced when the next frame arrives
  // |duration| after the last write.
  // The maximum duration between forced clusters is internally limited to not
  // go below 100 ms.
  // TODO(crbug.com/40876732): consider if cluster output should be based on
  // media timestamps.
  base::TimeDelta max_data_output_interval_;

  // Last timestamp written into the segment.
  base::TimeDelta last_timestamp_written_;

  std::unique_ptr<Delegate> delegate_;

  // The MkvMuxer active element.
  mkvmuxer::Segment segment_;
  // Flag to force the next call to a |segment_| method to return false.
  bool force_one_libwebm_error_ = false;

  // Frames held until all track headers have been written.
  base::circular_deque<std::tuple<EncodedFrame, base::TimeDelta>>
      buffered_frames_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_WEBM_MUXER_H_
