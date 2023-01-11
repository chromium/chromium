// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_PULL_FIFO_H_
#define MEDIA_BASE_AUDIO_PULL_FIFO_H_

#include <memory>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace media {
class AudioBus;

// A FIFO (First In First Out) buffer to handle mismatches in buffer sizes
// between a producer and consumer. The consumer will pull data from this FIFO.
// If data is already available in the FIFO, it is provided to the consumer.
// If insufficient data is available to satisfy the request, the FIFO will ask
// the producer for more data to fulfill a request.
class MEDIA_EXPORT AudioPullFifo {
 public:
  // Callback type for providing more data into the FIFO.  Expects AudioBus
  // to be completely filled with data upon return; zero padded if not enough
  // frames are available to satisfy the request.  |frame_delay| is the number
  // of output frames already processed and can be used to estimate delay.
  using ReadCB =
      base::RepeatingCallback<void(int frame_delay, AudioBus* audio_bus)>;

  // Constructs an AudioPullFifo with the specified |read_cb|, which is used to
  // read audio data to the FIFO if data is not already available. The internal
  // FIFO can contain |channel| number of channels, where each channel is of
  // length |frames| audio frames.
  AudioPullFifo(int channels, int frames, ReadCB read_cb);

  AudioPullFifo(const AudioPullFifo&) = delete;
  AudioPullFifo& operator=(const AudioPullFifo&) = delete;

  virtual ~AudioPullFifo();

  // Consumes |frames_to_consume| audio frames from the FIFO and copies
  // them to |destination|. If the FIFO does not have enough data, we ask
  // the producer to give us more data to fulfill the request using the
  // ReadCB implementation.
  void Consume(AudioBus* destination, int frames_to_consume);

  // Empties the FIFO without deallocating any memory.
  void Clear();

  // Returns the size of the fifo in number of frames.
  int SizeInFrames() const;

 private:
  // Attempt to fulfill the request using what is available in the FIFO.
  // Append new data to the |destination| starting at |write_pos|.
  int ReadFromFifo(AudioBus* destination, int frames_to_provide, int write_pos);

  // Source of data to the FIFO.
  const ReadCB read_cb_;

  // Temporary audio bus to hold the data from the producer.
  std::unique_ptr<AudioBus> fifo_;
  int fifo_index_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_PULL_FIFO_H_
