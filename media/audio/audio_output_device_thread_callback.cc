// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device_thread_callback.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_stats_reporter.h"
#include "media/base/audio_glitch_info.h"

namespace media {

AudioOutputDeviceThreadCallback::AudioOutputDeviceThreadCallback(
    const media::AudioParameters& audio_parameters,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    media::AudioRendererSink::RenderCallback* render_callback)
    : media::AudioDeviceThread::Callback(
          audio_parameters,
          ComputeAudioOutputBufferSize(audio_parameters),
          /*segment count*/ 1),
      shared_memory_region_(std::move(shared_memory_region)),
      render_callback_(render_callback),
      create_time_(base::TimeTicks::Now()),
      stats_reporter_(audio_parameters,
                      AudioDeviceStatsReporter::Type::kOutput) {
  // CHECK that the shared memory is large enough. The memory allocated must be
  // at least as large as expected.
  CHECK(memory_length_ <= shared_memory_region_.GetSize());
}

AudioOutputDeviceThreadCallback::~AudioOutputDeviceThreadCallback() {
  UmaHistogramLongTimes("Media.Audio.Render.OutputStreamDuration2",
                        base::TimeTicks::Now() - create_time_);
}

void AudioOutputDeviceThreadCallback::MapSharedMemory() {
  CHECK_EQ(total_segments_, 1u);
  shared_memory_mapping_ = shared_memory_region_.MapAt(0, memory_length_);
  CHECK(shared_memory_mapping_.IsValid());

  media::AudioOutputBuffer* buffer =
      reinterpret_cast<media::AudioOutputBuffer*>(
          shared_memory_mapping_.memory());
  output_bus_ = media::AudioBus::WrapMemory(audio_parameters_, buffer->audio);
  output_bus_->set_is_bitstream_format(audio_parameters_.IsBitstreamFormat());
}

// Called whenever we receive notifications about pending data.
void AudioOutputDeviceThreadCallback::Process(uint32_t control_signal) {
  callback_num_++;

  // Read and reset the glitch info.
  media::AudioOutputBuffer* buffer =
      reinterpret_cast<media::AudioOutputBuffer*>(
          shared_memory_mapping_.memory());
  media::AudioGlitchInfo glitch_info{
      .duration = base::Microseconds(buffer->params.glitch_duration_us),
      .count = buffer->params.glitch_count};
  buffer->params.glitch_duration_us = {};
  buffer->params.glitch_count = 0;

  base::TimeDelta delay = base::Microseconds(buffer->params.delay_us);

  base::TimeTicks delay_timestamp =
      base::TimeTicks() + base::Microseconds(buffer->params.delay_timestamp_us);

  TRACE_EVENT("audio", "AudioOutputDevice::FireRenderCallback", "callback_num",
              callback_num_, "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
              "playout_delay (ms)", delay.InMillisecondsF());
  glitch_info.MaybeAddTraceEvent();

  DVLOG(4) << __func__ << " delay:" << delay << " delay_timestamp:" << delay;

  // When playback starts, we get an immediate callback to Process to make sure
  // that we have some data, we'll get another one after the device is awake and
  // ingesting data, which is what we want to track with this trace.
  if (callback_num_ == 2) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "StartingPlayback",
                                    TRACE_ID_LOCAL(this));
    if (first_play_start_time_) {
      UmaHistogramTimes("Media.Audio.Render.OutputDeviceStartTime2",
                        base::TimeTicks::Now() - *first_play_start_time_);
    }
  }

  // Update the audio-delay measurement, inform about the number of skipped
  // frames, and ask client to render audio.  Since |output_bus_| is wrapping
  // the shared memory the Render() call is writing directly into the shared
  // memory.
  render_callback_->Render(delay, delay_timestamp, glitch_info,
                           output_bus_.get());
  stats_reporter_.ReportCallback(delay, glitch_info);

  if (audio_parameters_.IsBitstreamFormat()) {
    buffer->params.bitstream_data_size = output_bus_->GetBitstreamDataSize();
    buffer->params.bitstream_frames = output_bus_->GetBitstreamFrames();
  }

}

void AudioOutputDeviceThreadCallback::OnSocketError() {
  render_callback_->OnRenderError();
}

bool AudioOutputDeviceThreadCallback::CurrentThreadIsAudioDeviceThread() {
  return thread_checker_.CalledOnValidThread();
}

void AudioOutputDeviceThreadCallback::InitializePlayStartTime() {
  if (first_play_start_time_)
    return;

  DCHECK(!callback_num_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "StartingPlayback",
                                    TRACE_ID_LOCAL(this));
  first_play_start_time_ = base::TimeTicks::Now();
}

}  // namespace media
