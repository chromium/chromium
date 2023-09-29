// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_FIFO_H_
#define MEDIA_BASE_AUDIO_FIFO_H_

#include <memory>

#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// First-in first-out container for AudioBus elements.
// The maximum number of audio frames in the FIFO is set at construction and
// can not be extended dynamically.  The allocated memory is utilized as a
// ring buffer.
// This class is thread-unsafe.
class MEDIA_EXPORT AudioFifo {
 public:
  // Creates a new AudioFifo and allocates |channels| of length |frames|.
  AudioFifo(int channels, int frames);

  AudioFifo(const AudioFifo&) = delete;
  AudioFifo& operator=(const AudioFifo&) = delete;

  virtual ~AudioFifo();

  // Pushes all audio channel data from `source` to the FIFO.
  // Push() will crash if the allocated space is insufficient.
  void Push(const AudioBus* source);

  // Pushes the number of `source_size` of frames in all audio channel data from
  // `source` to the FIFO.
  void Push(const AudioBus* source, int source_size);

  // Consumes |frames_to_consume| audio frames from the FIFO and copies
  // them to |destination| starting at position |start_frame|.
  // Consume() will crash if the FIFO does not contain |frames_to_consume|
  // frames or if there is insufficient space in |destination| to store the
  // frames.
  void Consume(AudioBus* destination, int start_frame, int frames_to_consume);

  // Empties the FIFO without deallocating any memory.
  void Clear();

  // Number of actual audio frames in the FIFO.
  int frames() const { return frames_; }

  int max_frames() const { return max_frames_; }

 private:
  // The actual FIFO is an audio bus implemented as a ring buffer.
  std::unique_ptr<AudioBus> audio_bus_;

  // Maximum number of elements the FIFO can contain.
  // This value is set by |frames| in the constructor.
  const int max_frames_;

  // Number of actual elements in the FIFO.
  int frames_;

  // Current read position.
  int read_pos_;

  // Current write position.
  int write_pos_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_FIFO_H_
