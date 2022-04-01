// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/processing_audio_fifo.h"

#include "media/base/audio_bus.h"

namespace audio {

struct ProcessingAudioFifo::CaptureData {
  std::unique_ptr<media::AudioBus> audio_bus;
  base::TimeTicks capture_time;
  double volume;
  bool key_pressed;
};

ProcessingAudioFifo::ProcessingAudioFifo(
    const media::AudioParameters& input_params,
    int fifo_size,
    ProcessAudioCallback processing_callback)
    : fifo_size_(fifo_size),
      fifo_(fifo_size_),
      input_params_(input_params),
      audio_processing_thread_("AudioProcessingThread"),
      processing_callback_(std::move(processing_callback)),
      new_data_captured_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {
  DCHECK(processing_callback_);

  // Pre-allocate FIFO memory.
  DCHECK_EQ(fifo_.size(), static_cast<size_t>(fifo_size_));
  for (int i = 0; i < fifo_size_; ++i)
    fifo_[i].audio_bus = media::AudioBus::Create(input_params_);
}

ProcessingAudioFifo::~ProcessingAudioFifo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_checker_);

  StopProcessingLoop();
}

void ProcessingAudioFifo::StopProcessingLoop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_checker_);

  fifo_stopping_.Set();
  new_data_captured_.Signal();
  audio_processing_thread_.Stop();
}

ProcessingAudioFifo::CaptureData* ProcessingAudioFifo::GetDataAtIndex(int idx) {
  return &fifo_[idx % fifo_size_];
}

void ProcessingAudioFifo::Start() {
  StartInternal(&new_data_captured_);
}

void ProcessingAudioFifo::StartForTesting(
    base::WaitableEvent* fake_new_data_captured) {
  StartInternal(fake_new_data_captured);
}

void ProcessingAudioFifo::StartInternal(
    base::WaitableEvent* new_data_captured) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_checker_);

  // Start should only be called once.
  DCHECK(!audio_processing_thread_.IsRunning());

  base::Thread::Options options;
  options.priority = base::ThreadPriority::REALTIME_AUDIO;

  audio_processing_thread_.StartWithOptions(std::move(options));

  audio_processing_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessingAudioFifo::ProcessAudioLoop,
                                base::Unretained(this),
                                base::Unretained(new_data_captured)));
}

void ProcessingAudioFifo::PushData(const media::AudioBus* audio_bus,
                                   base::TimeTicks capture_time,
                                   double volume,
                                   bool key_pressed) {
  DCHECK_EQ(audio_bus->frames(), input_params_.frames_per_buffer());

  CaptureData* data;
  {
    base::AutoLock locker(fifo_index_lock_);

    DCHECK_GE(write_count_, read_count_);

    if (write_count_ - read_count_ == fifo_size_) {
      // TODO(crbug.com/1296149): Log dropped frames.
      return;
    }

    data = GetDataAtIndex(write_count_);
  }

  // Write to the FIFO (lock-free).
  data->capture_time = capture_time;
  data->volume = volume;
  data->key_pressed = key_pressed;
  audio_bus->CopyTo(data->audio_bus.get());

  {
    base::AutoLock locker(fifo_index_lock_);
    // The processing/reading thread can process |data| now.
    ++write_count_;
  }

  new_data_captured_.Signal();
}

void ProcessingAudioFifo::ProcessAudioLoop(
    base::WaitableEvent* new_data_captured) {
  while (!fifo_stopping_.IsSet()) {
    // Wait for new data.
    new_data_captured->Wait();

    // Keep processing data until we shut down, or we have processed all
    // available data.
    while (!fifo_stopping_.IsSet()) {
      CaptureData* data;
      {
        base::AutoLock locker(fifo_index_lock_);

        DCHECK_GE(write_count_, read_count_);

        // No data to read.
        if (read_count_ == write_count_)
          break;

        data = GetDataAtIndex(read_count_);
      }

      // Read from the FIFO, and process the data (lock-free).
      processing_callback_.Run(*data->audio_bus, data->capture_time,
                               data->volume, data->key_pressed);

      {
        base::AutoLock locker(fifo_index_lock_);
        // The capture/writing thread can safely overwrite |data| now.
        ++read_count_;
      }
    }
  }
}

}  // namespace audio
