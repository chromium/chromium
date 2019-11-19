// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// Writes audio data to a 16 bit PCM WAVE file used for debugging purposes. All
// operations are non-blocking.
// Functions are virtual for the purpose of test mocking.
class MEDIA_EXPORT AudioDebugFileWriter {
 public:
  // Number of channels and sample rate are used from |params|, the other
  // parameters are ignored. The number of channels in the data passed to
  // Write() must match |params|.
  explicit AudioDebugFileWriter(const AudioParameters& params);

  virtual ~AudioDebugFileWriter();

  // Must be called before calling Write() for the first time after creation or
  // Stop() call. Can be called on any sequence; Write() and Stop() must be
  // called on the same sequence as Start().
  virtual void Start(base::File file);

  // Must be called to finish recording. Each call to Start() requires a call to
  // Stop(). Will be automatically called on destruction.
  virtual void Stop();

  // Write |data| to file.
  virtual void Write(std::unique_ptr<AudioBus> data);

  // Returns true if Write() call scheduled at this point will most likely write
  // data to the file, and false if it most likely will be a no-op. The result
  // may be ambigulous if Start() or Stop() is executed at the moment. Can be
  // called from any sequence.
  virtual bool WillWrite();

 protected:
  const AudioParameters params_;

 private:
  class AudioFileWriter;

  using AudioFileWriterUniquePtr =
      std::unique_ptr<AudioFileWriter, base::OnTaskRunnerDeleter>;

  // The task runner to do file output operations on.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_ =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  AudioFileWriterUniquePtr file_writer_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioDebugFileWriter);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
