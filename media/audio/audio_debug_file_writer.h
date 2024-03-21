// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

class AudioBusPool;

// Writes audio data to a 16 bit PCM WAVE file used for debugging purposes.
// Functions are virtual for the purpose of test mocking. This class can be
// created and used anywhere, but must be destroyed using the
// OnTaskRunnerDeleter provided by Create(). It starts writing on Create(), and
// stops writing on destruction.
class MEDIA_EXPORT AudioDebugFileWriter {
 public:
  AudioDebugFileWriter(const AudioDebugFileWriter&) = delete;
  AudioDebugFileWriter& operator=(const AudioDebugFileWriter&) = delete;

  virtual ~AudioDebugFileWriter();

  // Write |data| to file.
  virtual void Write(const AudioBus& data);

  using Ptr = std::unique_ptr<AudioDebugFileWriter, base::OnTaskRunnerDeleter>;

  // Number of channels and sample rate are used from |params|, the other
  // parameters are ignored. The number of channels in the data passed to
  // Write() must match |params|. Write() must be called on the sequence that
  // task_runner belongs to.
  static Ptr Create(const AudioParameters& params, base::File file);

 protected:
  // Protected for testing.
  AudioDebugFileWriter(const AudioParameters& params,
                       base::File file,
                       std::unique_ptr<AudioBusPool> audio_bus_pool);

  // Create with a custom AudioBusPool.
  static Ptr Create(const AudioParameters& params,
                    base::File file,
                    std::unique_ptr<AudioBusPool> audio_bus_pool);

  const AudioParameters params_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

 private:
  // Write wave header to file. Called twice: on Create() the size of the wave
  // data is unknown, so the header is written with zero sizes; then on
  // destruction it is re-written with the actual size info accumulated
  // throughout the object lifetime.
  void WriteHeader();

  void DoWrite(std::unique_ptr<AudioBus> data);

  // The file to write to.
  base::File file_;

  // Number of written samples.
  uint64_t samples_ = 0;

  // Intermediate buffer to be written to file. Interleaved 16 bit audio data.
  std::optional<base::HeapArray<int16_t>> interleaved_data_;

  // Stores AudioBuses to be reused.
  const std::unique_ptr<AudioBusPool> audio_bus_pool_;

  // The number of AudioBuses that should be preallocated on creation.
  static constexpr size_t kPreallocatedAudioBuses = 100;

  // The maximum number of AudioBuses we should cache at once.
  static constexpr size_t kMaxCachedAudioBuses = 500;

  base::WeakPtr<AudioDebugFileWriter> weak_this_;
  base::WeakPtrFactory<AudioDebugFileWriter> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
