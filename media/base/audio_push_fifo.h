// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PUSH_FIFO_H_
#define MEDIA_BASE_AUDIO_PUSH_FIFO_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// Yet another FIFO for audio data that re-buffers audio to a desired buffer
// size.  Unlike AudioFifo and AudioBlockFifo, this FIFO cannot overflow: The
// client is required to provide a callback that is called synchronously during
// a push whenever enough data becomes available.  This implementation
// eliminates redundant memory copies when the input buffer size always matches
// the desired buffer size.
class MEDIA_EXPORT AudioPushFifo final {
 public:
  // Called synchronously zero, one, or multiple times during a call to Push()
  // to deliver re-buffered audio.  |frame_delay| refers to the position of the
  // first frame in |output| relative to the first frame in the Push() call.  If
  // negative, this indicates the output contains some data from a prior call to
  // Push().  If zero or positive, the output contains data from the current
  // call to Push().  Clients can use this to adjust timestamps.
  using OutputCallback =
      base::RepeatingCallback<void(const AudioBus& output_bus,
                                   int frame_delay)>;

  // Creates a new AudioPushFifo which delivers re-buffered audio by running
  // |callback|.
  explicit AudioPushFifo(const OutputCallback& callback);

  ~AudioPushFifo();

  // Returns the number of frames in each AudioBus delivered to the
  // OutputCallback.
  int frames_per_buffer() const { return frames_per_buffer_; }

  // Must be called at least once before the first call to Push().  May be
  // called later (e.g., to support an audio format change).
  void Reset(int frames_per_buffer);

  // Pushes all audio channel data from |input_bus| through the FIFO.  This will
  // result in zero, one, or multiple synchronous calls to the OutputCallback
  // provided in the constructor.  If the |input_bus| has a different number of
  // channels than the prior Push() call, any currently-queued frames will be
  // dropped.
  void Push(const AudioBus& input_bus);

  // Flushes any enqueued frames by invoking the OutputCallback with those
  // frames plus padded zero samples.  If there are no frames currently
  // enqueued, OutputCallback is not run.
  void Flush();

 private:
  const OutputCallback callback_;

  int frames_per_buffer_;

  // Queue of frames pending for delivery.
  std::unique_ptr<AudioBus> audio_queue_;
  int queued_frames_;

  DISALLOW_COPY_AND_ASSIGN(AudioPushFifo);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PUSH_FIFO_H_
