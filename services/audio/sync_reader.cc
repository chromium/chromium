// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/sync_reader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/audio/audio_device_thread.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "services/audio/output_glitch_counter.h"

using media::AudioLatency;

namespace audio {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
BASE_FEATURE(kDynamicAudioTimeout,
             "DynamicAudioTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double> kBufferDurationPercent{
    &kDynamicAudioTimeout, "buffer_duration_percent", 0.95};
#endif

SyncReader::SyncReader(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    const media::AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket)
    : SyncReader(std::move(log_callback),
                 params,
                 foreign_socket,
                 std::make_unique<OutputGlitchCounter>(params.latency_tag())) {}

SyncReader::SyncReader(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    const media::AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket,
    std::unique_ptr<OutputGlitchCounter> glitch_counter)
    : log_callback_(std::move(log_callback)),
      latency_tag_(params.latency_tag()),
      mute_audio_for_testing_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMuteAudio)),
      output_bus_buffer_size_(
          media::AudioBus::CalculateMemorySize(params.channels(),
                                               params.frames_per_buffer())),
      read_timeout_glitch_{.duration = params.GetBufferDuration(), .count = 1},
      glitch_counter_(std::move(glitch_counter)) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  maximum_wait_time_ = params.GetBufferDuration() / 2;
  maximum_wait_time_for_mixing_ = maximum_wait_time_;
#else
  if (base::FeatureList::IsEnabled(kDynamicAudioTimeout)) {
    maximum_wait_time_ =
        params.GetBufferDuration() * kBufferDurationPercent.Get();
  } else {
    maximum_wait_time_ = base::Milliseconds(20);
  }
  maximum_wait_time_for_mixing_ = maximum_wait_time_;

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (media::IsChromeWideEchoCancellationEnabled()) {
    double mixing_timeout_percent =
        media::kChromeWideEchoCancellationDynamicMixingTimeout.Get();

    // The default negative value means we should ignore this parameter.
    if (mixing_timeout_percent > 0) {
      maximum_wait_time_for_mixing_ =
          params.GetBufferDuration() * mixing_timeout_percent;
    }
  }
#endif

#endif

  base::CheckedNumeric<size_t> memory_size =
      media::ComputeAudioOutputBufferSizeChecked(params);
  if (!memory_size.IsValid())
    return;

  shared_memory_region_ =
      base::UnsafeSharedMemoryRegion::Create(memory_size.ValueOrDie());
  shared_memory_mapping_ = shared_memory_region_.Map();
  if (shared_memory_region_.IsValid() && shared_memory_mapping_.IsValid() &&
      base::CancelableSyncSocket::CreatePair(&socket_, foreign_socket)) {
    auto* const buffer =
        shared_memory_mapping_.GetMemoryAs<media::AudioOutputBuffer>();
    output_bus_ = media::AudioBus::WrapMemory(params, buffer->audio);
    output_bus_->Zero();
    output_bus_->set_is_bitstream_format(params.IsBitstreamFormat());
  }
}

SyncReader::~SyncReader() {
  OutputGlitchCounter::LogStats log_stats = glitch_counter_->GetLogStats();

  TRACE_EVENT_INSTANT2("audio", "~SyncReader", TRACE_EVENT_SCOPE_THREAD,
                       "Missed callbacks", log_stats.miss_count_,
                       "Total callbacks", log_stats.callback_count_);

  log_callback_.Run(base::StringPrintf(
      "ASR: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      log_stats.miss_count_, log_stats.callback_count_));
}

bool SyncReader::IsValid() const {
  if (output_bus_) {
    DCHECK(shared_memory_region_.IsValid());
    DCHECK(shared_memory_mapping_.IsValid());
    DCHECK_NE(socket_.handle(), base::SyncSocket::kInvalidHandle);
    return true;
  }
  return false;
}

base::UnsafeSharedMemoryRegion SyncReader::TakeSharedMemoryRegion() {
  return std::move(shared_memory_region_);
}

// AudioOutputController::SyncReader implementations.
void SyncReader::RequestMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 const media::AudioGlitchInfo& glitch_info) {
  // We don't send arguments over the socket since sending more than 4
  // bytes might lead to being descheduled. The reading side will zero
  // them when consumed.
  auto* const buffer =
      shared_memory_mapping_.GetMemoryAs<media::AudioOutputBuffer>();
  // Increase the number of skipped frames stored in shared memory.
  buffer->params.delay_us = delay.InMicroseconds();
  buffer->params.delay_timestamp_us =
      (delay_timestamp - base::TimeTicks()).InMicroseconds();
  // Add platform glitches to the accumulated glitch info.
  pending_glitch_info_ += glitch_info;
  buffer->params.glitch_duration_us =
      pending_glitch_info_.duration.InMicroseconds();
  buffer->params.glitch_count = pending_glitch_info_.count;

  // Zero out the entire output buffer to avoid stuttering/repeating-buffers
  // in the anomalous case if the renderer is unable to keep up with real-time.
  output_bus_->Zero();

  uint32_t control_signal = 0;
  if (delay.is_max()) {
    // std::numeric_limits<uint32_t>::max() is a special signal which is
    // returned after the browser stops the output device in response to a
    // renderer side request.
    control_signal = std::numeric_limits<uint32_t>::max();
  }

  size_t sent_bytes = socket_.Send(base::byte_span_from_ref(control_signal));
  if (sent_bytes != sizeof(control_signal)) {
    // Ensure we don't log consecutive errors as this can lead to a large
    // amount of logs.
    if (!had_socket_error_) {
      had_socket_error_ = true;
      static const char* socket_send_failure_message =
          "ASR: No room in socket buffer.";
      PLOG(WARNING) << socket_send_failure_message;
      log_callback_.Run(socket_send_failure_message);
      TRACE_EVENT_INSTANT0("audio", socket_send_failure_message,
                           TRACE_EVENT_SCOPE_THREAD);
    }
  } else {
    had_socket_error_ = false;
    // We have successfully passed on the glitch info, now reset it.
    pending_glitch_info_ = {};
    // The AudioDeviceThread will only increase its own index if the socket
    // write succeeds, so only increase our own index on successful writes in
    // order not to get out of sync.
    ++buffer_index_;
  }
}

bool SyncReader::Read(media::AudioBus* dest, bool is_mixing) {
  bool missed_callback = !WaitUntilDataIsReady(is_mixing);
  glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  if (missed_callback) {
    ++renderer_missed_callback_count_;
    if (renderer_missed_callback_count_ <= 100 &&
        renderer_missed_callback_count_ % 10 == 0) {
      LOG(WARNING) << "SyncReader::Read timed out, audio glitch count="
                   << renderer_missed_callback_count_;
      if (renderer_missed_callback_count_ == 100)
        LOG(WARNING) << "(log cap reached, suppressing further logs)";
    }
    dest->Zero();
    // Add IPC glitch to the accumulated glitch info.
    pending_glitch_info_ += read_timeout_glitch_;
    return false;
  }

  // Zeroed buffers may be discarded immediately when outputing compressed
  // bitstream.
  if (mute_audio_for_testing_ && !output_bus_->is_bitstream_format()) {
    dest->Zero();
    return true;
  }

  if (output_bus_->is_bitstream_format()) {
    // For bitstream formats, we need the real data size and PCM frame count.
    auto* const buffer =
        shared_memory_mapping_.GetMemoryAs<media::AudioOutputBuffer>();
    uint32_t data_size = buffer->params.bitstream_data_size;
    uint32_t bitstream_frames = buffer->params.bitstream_frames;
    // |bitstream_frames| is cast to int below, so it must fit.
    if (data_size > output_bus_buffer_size_ ||
        !base::IsValueInRangeForNumericType<int>(bitstream_frames)) {
      // Received data doesn't fit in the buffer, shouldn't happen.
      dest->Zero();
      return true;
    }
    output_bus_->SetBitstreamDataSize(data_size);
    output_bus_->SetBitstreamFrames(bitstream_frames);
    output_bus_->CopyTo(dest);
    return true;
  }

  // Copy and clip data coming across the shared memory since it's untrusted.
  output_bus_->CopyAndClipTo(dest);
  return true;
}

void SyncReader::Close() {
  socket_.Close();
  output_bus_.reset();
}

bool SyncReader::WaitUntilDataIsReady(bool is_mixing) {
  TRACE_EVENT0("audio", "SyncReader::WaitUntilDataIsReady");
  base::TimeDelta timeout =
      is_mixing ? maximum_wait_time_for_mixing_ : maximum_wait_time_;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeTicks finish_time = start_time + timeout;

  // Check if data is ready and if not, wait a reasonable amount of time for it.
  //
  // Data readiness is achieved via parallel counters, one on the renderer side
  // and one here.  Every time a buffer is requested via UpdatePendingBytes(),
  // |buffer_index_| is incremented.  Subsequently every time the renderer has a
  // buffer ready it increments its counter and sends the counter value over the
  // SyncSocket.  Data is ready when |buffer_index_| matches the counter value
  // received from the renderer.
  //
  // The counter values may temporarily become out of sync if the renderer is
  // unable to deliver audio fast enough.  It's assumed that the renderer will
  // catch up at some point, which means discarding counter values read from the
  // SyncSocket which don't match our current buffer index.
  size_t bytes_received = 0;
  uint32_t renderer_buffer_index = 0;
  while (timeout.InMicroseconds() > 0) {
    bytes_received = socket_.ReceiveWithTimeout(
        base::byte_span_from_ref(renderer_buffer_index), timeout);
    if (bytes_received != sizeof(renderer_buffer_index)) {
      bytes_received = 0;
      break;
    }

    if (renderer_buffer_index == buffer_index_)
      break;

    // Reduce the timeout value as receives succeed, but aren't the right index.
    timeout = finish_time - base::TimeTicks::Now();
  }

  // Receive timed out or another error occurred.  Receive can timeout if the
  // renderer is unable to deliver audio data within the allotted time.
  if (!bytes_received || renderer_buffer_index != buffer_index_) {
    TRACE_EVENT_INSTANT0("audio", "SyncReader::Read timed out",
                         TRACE_EVENT_SCOPE_THREAD);

    base::TimeDelta time_since_start = base::TimeTicks::Now() - start_time;
    base::UmaHistogramCustomTimes("Media.AudioOutputControllerDataNotReady",
                                  time_since_start, base::Milliseconds(1),
                                  base::Milliseconds(1000), 50);
    return false;
  }

  return true;
}

}  // namespace audio
