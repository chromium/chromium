// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/processing_audio_fifo.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace audio {

class ProcessingAudioFifo::StatsReporter {
 public:
  // Log once every 10s, assuming 10ms buffers.
  constexpr static int kCallbacksPerLogPeriod = 1000;

  // Capped at 10% of callbacks.
  constexpr static int kMaxLoggedOverrunCount = kCallbacksPerLogPeriod / 10;

  constexpr static int kMaxFifoSize = 200;

  StatsReporter(int fifo_size, ProcessingAudioFifo::LogCallback log_callback)
      : fifo_size_(fifo_size), log_callback_(std::move(log_callback)) {}

  ~StatsReporter() {
    log_callback_.Run(base::StringPrintf(
        "AIC::~ProcessingFifo() => (total_callbacks=%d, total_overruns=%d)",
        total_callback_count_, total_overrun_count_));
  }

  void LogPush(int fifo_space_available) {
    if (!fifo_space_available)
      ++total_overrun_count_;

    int fifo_space_used = fifo_size_ - fifo_space_available;
    if (max_fifo_space_used_during_log_period_ < fifo_space_used)
      max_fifo_space_used_during_log_period_ = fifo_space_used;

    ++total_callback_count_;

    if (total_callback_count_ % kCallbacksPerLogPeriod)
      return;

    base::UmaHistogramCustomCounts(
        "Media.Audio.Capture.ProcessingAudioFifo.MaxUsage",
        max_fifo_space_used_during_log_period_,
        /*min*/ 1,
        /*max*/ kMaxFifoSize + 1,
        /*buckets*/ 50);

    base::UmaHistogramCounts100(
        "Media.Audio.Capture.ProcessingAudioFifo.Overruns",
        total_overrun_count_ - last_logged_overrun_count_);
    max_fifo_space_used_during_log_period_ = 0;
    last_logged_overrun_count_ = total_overrun_count_;
  }

 private:
  const int fifo_size_;
  const ProcessingAudioFifo::LogCallback log_callback_;
  int total_callback_count_ = 0;
  int total_overrun_count_ = 0;
  int max_fifo_space_used_during_log_period_ = 0;
  int last_logged_overrun_count_ = 0;
};

struct ProcessingAudioFifo::CaptureData {
  std::unique_ptr<media::AudioBus> audio_bus;
  base::TimeTicks capture_time;
  double volume;
  bool key_pressed;
  media::AudioGlitchInfo audio_glitch_info;
};

ProcessingAudioFifo::ProcessingAudioFifo(
    const media::AudioParameters& input_params,
    int fifo_size,
    ProcessAudioCallback processing_callback,
    LogCallback log_callback)
    : fifo_size_(fifo_size),
      fifo_(fifo_size_),
      input_params_(input_params),
      audio_processing_thread_("AudioProcessingThread",
                               input_params_.GetBufferDuration()),
      processing_callback_(std::move(processing_callback)),
      new_data_captured_(base::WaitableEvent::ResetPolicy::AUTOMATIC),
      stats_reporter_(
          std::make_unique<StatsReporter>(fifo_size_,
                                          std::move(log_callback))) {
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

void ProcessingAudioFifo::AttachOnProcessedCallbackForTesting(
    base::RepeatingClosure on_processed_callback) {
  // This should only be called before Start().
  DCHECK(!audio_processing_thread_.IsRunning());

  processing_callback_ =
      processing_callback_.Then(std::move(on_processed_callback));
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
  StartInternal(&new_data_captured_,
                base::Thread::Options(base::ThreadType::kRealtimeAudio));
}

void ProcessingAudioFifo::StartForTesting(
    base::WaitableEvent* fake_new_data_captured) {
  // Only use kDefault thread type instead of kRealtimeAudio because Linux has
  // flakiness issue when setting realtime priority.
  StartInternal(fake_new_data_captured,
                base::Thread::Options(base::ThreadType::kDefault));
}

void ProcessingAudioFifo::StartInternal(base::WaitableEvent* new_data_captured,
                                        base::Thread::Options options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_checker_);

  // Start should only be called once.
  DCHECK(!audio_processing_thread_.IsRunning());

  audio_processing_thread_.StartWithOptions(std::move(options));

  audio_processing_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessingAudioFifo::ProcessAudioLoop,
                                base::Unretained(this),
                                base::Unretained(new_data_captured)));
}

void ProcessingAudioFifo::PushData(
    const media::AudioBus* audio_bus,
    base::TimeTicks capture_time,
    double volume,
    bool key_pressed,
    const media::AudioGlitchInfo& audio_glitch_info) {
  DCHECK_EQ(audio_bus->frames(), input_params_.frames_per_buffer());
  glitch_info_accumulator_.Add(audio_glitch_info);

  CaptureData* data = nullptr;
  int fifo_space = fifo_size_;
  {
    base::AutoLock locker(fifo_index_lock_);

    DCHECK_GE(write_count_, read_count_);
    const int unread_buffers = write_count_ - read_count_;
    fifo_space -= unread_buffers;

    if (fifo_space)
      data = GetDataAtIndex(write_count_);
  }

  stats_reporter_->LogPush(fifo_space);

  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("audio"),
                    "ProcessingAudioFifo space available", this, fifo_space);

  if (!data) {
    TRACE_EVENT_INSTANT0("audio", "ProcessingAudioFifo::Overrun",
                         TRACE_EVENT_SCOPE_THREAD);
    glitch_info_accumulator_.Add(media::AudioGlitchInfo{
        .duration = input_params_.GetBufferDuration(), .count = 1});
    return;  // Overrun.
  }

  // Write to the FIFO (lock-free).
  data->capture_time = capture_time;
  data->volume = volume;
  data->key_pressed = key_pressed;
  data->audio_glitch_info = glitch_info_accumulator_.GetAndReset();
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
                               data->volume, data->key_pressed,
                               data->audio_glitch_info);

      {
        base::AutoLock locker(fifo_index_lock_);
        // The capture/writing thread can safely overwrite |data| now.
        ++read_count_;
      }
    }
  }
}

}  // namespace audio
