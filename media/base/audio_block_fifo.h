// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BLOCK_FIFO_H_
#define MEDIA_BASE_AUDIO_BLOCK_FIFO_H_

#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// First-in first-out container for AudioBus elements.
// The FIFO is composed of blocks of AudioBus elements, it accepts interleaved
// data as input and will deinterleave it into the FIFO, and it only allows
// consuming a whole block of AudioBus element.
// This class is thread-unsafe.
class MEDIA_EXPORT AudioBlockFifo {
 public:
  // Creates a new AudioBlockFifo and allocates |blocks| memory, each block
  // of memory can store |channels| of length |frames| data.
  AudioBlockFifo(int channels, int frames, int blocks);

  AudioBlockFifo(const AudioBlockFifo&) = delete;
  AudioBlockFifo& operator=(const AudioBlockFifo&) = delete;

  virtual ~AudioBlockFifo();

  // Pushes interleaved audio data from |source| to the FIFO.
  // The method will deinterleave the data into an audio bus.
  // Push() will crash if the allocated space is insufficient.
  void Push(const void* source, int frames, int bytes_per_sample);

  // Pushes zeroed out frames to the FIFO.
  void PushSilence(int frames);

  // Consumes a block of audio from the FIFO.  Returns an AudioBus which
  // contains the consumed audio data to avoid copying.
  // Consume() will crash if the FIFO does not contain a block of data.
  const AudioBus* Consume();

  // Empties the FIFO without deallocating any memory.
  void Clear();

  // Number of available block of memory ready to be consumed in the FIFO.
  int available_blocks() const { return available_blocks_; }

  // Number of available frames of data in the FIFO.
  int GetAvailableFrames() const;

  // Number of unfilled frames in the whole FIFO.
  int GetUnfilledFrames() const;

  // Dynamically increase |blocks| of memory to the FIFO.
  void IncreaseCapacity(int blocks);

 private:
  // Common implementation for Push() and PushSilence.  if |source| is nullptr,
  // silence will be pushed. To push silence, set source and bytes_per_sample to
  // nullptr and 0 respectively.
  void PushInternal(const void* source, int frames, int bytes_per_sample);

  // The actual FIFO is a vector of audio buses.
  std::vector<std::unique_ptr<AudioBus>> audio_blocks_;

  // Number of channels in AudioBus.
  const int channels_;

  // Maximum number of frames of data one block of memory can contain.
  // This value is set by |frames| in the constructor.
  const int block_frames_;

  // Used to keep track which block of memory to be written.
  int write_block_;

  // Used to keep track which block of memory to be consumed.
  int read_block_;

  // Number of available blocks of memory to be consumed.
  int available_blocks_;

  // Current write position in the current written block.
  int write_pos_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BLOCK_FIFO_H_
