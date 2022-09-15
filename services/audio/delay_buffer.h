// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DELAY_BUFFER_H_
#define SERVICES_AUDIO_DELAY_BUFFER_H_

#include <memory>

#include "base/containers/circular_deque.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

// Records and maintains a recent history of an audio signal, then allows
// read-back starting from part of the recording. While this looks a lot like a
// FIFO, it is not because Read() will typically not be reading from one end of
// a queue.
//
// The audio format is the same throughout all operations, as this DelayBuffer
// does not resample or remix the audio. Also, for absolute precision, it uses
// frame counts to track the timing of the audio recorded and read.
//
// The typical use case is the loopback audio system: In this scenario, the
// service has an audio output stream running for local playback, and the
// stream's audio is timed to play back in the near future (usually, 1 ms to 20
// ms, depending on the platform). When loopback is active, that audio will be
// copied into this DelayBuffer via calls to Write(). Then, the loopback audio
// stream implementation will Read() the audio at a time in the recent past
// (approximately 20 ms before "now," but this will vary slightly). Because of
// clock drift concerns, the loopback implementation will slightly compress/
// stretch the audio signal it pulls out of this buffer, to maintain
// synchronization, and this will cause it to vary the number of frames read for
// each successive Read() call.
class DelayBuffer {
 public:
  // Use sample counts as a measure of audio signal position.
  using FrameTicks = int64_t;

  // Construct a DelayBuffer that keeps at least |history_size| un-read frames
  // recorded.
  explicit DelayBuffer(int history_size);

  DelayBuffer(const DelayBuffer&) = delete;
  DelayBuffer& operator=(const DelayBuffer&) = delete;

  ~DelayBuffer();

  // Inserts a copy of the given audio into the buffer. |position| must be
  // monotonically increasing, and the audio must not overlap any
  // previously-written audio. The length of the |input_bus| may vary, but the
  // channel layout may not. The given |volume| will be used to scale the audio
  // during the copy to the internal buffer.
  void Write(FrameTicks position,
             const media::AudioBus& input_bus,
             double volume);

  // Reads audio from the buffer, starting at the given |from| position, which
  // must not overlap any previously-read audio. |frames_to_read| is the number
  // of frames to read, which may be any amount less than or equal to the size
  // of the |output_bus|. No part of the |output_bus| beyond the first
  // |frames_to_read| will be modified. If there are gaps (i.e., missing pieces)
  // of the recording, zeros will be filled in the output.
  void Read(FrameTicks from, int frames_to_read, media::AudioBus* output_bus);

  // Returns the current buffered range of the recording, ala the usual C++
  // [begin,end)  semantics.
  FrameTicks GetBeginPosition() const;
  FrameTicks GetEndPosition() const;

 private:
  struct InputChunk {
    // The position of the first frame in this chunk.
    FrameTicks position;

    // The storage for the audio frames.
    std::unique_ptr<media::AudioBus> bus;

    // Constructor for an InputChunk with data.
    InputChunk(FrameTicks p, std::unique_ptr<media::AudioBus> b);

    InputChunk(const InputChunk&) = delete;
    InputChunk& operator=(const InputChunk&) = delete;

    // Move constructor/assignment.
    InputChunk(InputChunk&& other);
    InputChunk& operator=(InputChunk&& other);

    ~InputChunk();

    // Returns the position just after the last frame's position.
    FrameTicks GetEndPosition() const;
  };

  // The minimum number of un-read frames that must be kept.
  const int history_size_;

  // A queue storing each chunk of recorded audio. The elements in the queue are
  // always in-order, chronologically increasing by InputChunk::position, and do
  // not overlap.
  base::circular_deque<InputChunk> chunks_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DELAY_BUFFER_H_
