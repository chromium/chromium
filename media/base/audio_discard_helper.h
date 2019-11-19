// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DISCARD_HELPER_H_
#define MEDIA_BASE_AUDIO_DISCARD_HELPER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace media {

class AudioBuffer;

// Helper class for managing timestamps and discard events around decoding.
class MEDIA_EXPORT AudioDiscardHelper {
 public:
  // |sample_rate| is the sample rate of decoded data which will be handed into
  // the ProcessBuffers() call.
  //
  // |decoder_delay| is the number of frames a decoder will output before data
  // corresponding to the first encoded buffer is output.  Callers only need to
  // specify this if the decoder inserts frames which have no corresponding
  // encoded buffer.
  //
  // |delayed_discard| indicates that the codec requires two encoded buffers
  // before the first decoded output buffer.  Used only for vorbis currently.
  //
  // For example, most MP3 decoders will output 529 junk frames before the data
  // corresponding to the first encoded buffer is output.  These frames are not
  // represented in the encoded data stream and instead are an artifact of how
  // most MP3 decoders work.  See http://lame.sourceforge.net/tech-FAQ.txt
  AudioDiscardHelper(int sample_rate,
                     size_t decoder_delay,
                     bool delayed_discard);
  ~AudioDiscardHelper();

  // Converts a TimeDelta to a frame count based on the constructed sample rate.
  // |duration| must be positive.
  size_t TimeDeltaToFrames(base::TimeDelta duration) const;

  // Resets internal state and indicates that |initial_discard| of upcoming
  // frames should be discarded.
  void Reset(size_t initial_discard);

  // Applies discard padding from the encoded buffer along with any initial
  // discards.  |decoded_buffer| may be NULL, if not the timestamp and duration
  // will be set after discards are applied.  Returns true if |decoded_buffer|
  // exists after processing discard events.  Returns false if |decoded_buffer|
  // was NULL, is completely discarded, or a processing error occurs.
  //
  // If AudioDiscardHelper is not initialized() the timestamp of the first
  // |encoded_buffer| will be used as the basis for all future timestamps set on
  // |decoded_buffer|s.  If the first buffer has a negative timestamp it will be
  // clamped to zero.
  bool ProcessBuffers(const DecoderBuffer& encoded_buffer,
                      AudioBuffer* decoded_buffer);

  // Whether any buffers have been processed.
  bool initialized() const {
    return timestamp_helper_.base_timestamp() != kNoTimestamp;
  }

 private:
  // The sample rate of the decoded audio samples.  Used by TimeDeltaToFrames()
  // and the timestamp helper.
  const int sample_rate_;

  // Some codecs output extra samples during the first decode.  In order to trim
  // DiscardPadding correctly the helper must know the offset into the decoded
  // buffers at which real samples start.
  const size_t decoder_delay_;

  // Used to regenerate sample accurate timestamps for decoded buffers.  The
  // timestamp of the first encoded buffer seen by ProcessBuffers() is used as
  // the base timestamp.
  AudioTimestampHelper timestamp_helper_;

  // The number of frames to discard from the front of the next buffer.  Can be
  // set by Reset() and added to by a front DiscardPadding larger than its
  // associated buffer.
  size_t discard_frames_;

  // The last encoded buffer timestamp seen by ProcessBuffers() or kNoTimestamp
  // if no buffers have been seen thus far.  Used to issue warnings for buffer
  // sequences with non-monotonic timestamps.
  base::TimeDelta last_input_timestamp_;

  // Certain codecs require two encoded buffers before they'll output the first
  // decoded buffer.  In this case DiscardPadding must be carried over from the
  // previous encoded buffer.  Set during construction.
  const bool delayed_discard_;
  DecoderBuffer::DiscardPadding delayed_discard_padding_;

  // When |decoder_delay_| > 0, the number of frames which should be discarded
  // from the next buffer.  The index at which to start discarding is calculated
  // by subtracting |delayed_end_discard_| from |decoder_delay_|.
  size_t delayed_end_discard_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioDiscardHelper);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DISCARD_HELPER_H_
