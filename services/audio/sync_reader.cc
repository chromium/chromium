// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/sync_reader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/command_line.h"
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

using media::AudioLatency;

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum class AudioGlitchResult {
  kNoGlitches = 0,
  kGlitches = 1,
  kMaxValue = kGlitches
};

void LogPerLatencyGlitchUma(AudioLatency::LatencyType latency,
                            int renderer_missed_callback_count,
                            int renderer_callback_count,
                            bool mixing) {
  DCHECK_LE(renderer_missed_callback_count, renderer_callback_count);

  auto LatencyToString = [](AudioLatency::LatencyType latency) {
    switch (latency) {
      case AudioLatency::LATENCY_EXACT_MS:
        return "LatencyExactMs";
      case AudioLatency::LATENCY_INTERACTIVE:
        return "LatencyInteractive";
      case AudioLatency::LATENCY_RTC:
        return "LatencyRtc";
      case AudioLatency::LATENCY_PLAYBACK:
        return "LatencyPlayback";
      default:
        return "LatencyUnknown";
    }
  };

  const std::string suffix = LatencyToString(latency);

  if (!mixing) {
    base::UmaHistogramEnumeration("Media.AudioRendererAudioGlitches2." + suffix,
                                  (renderer_missed_callback_count > 0)
                                      ? AudioGlitchResult::kGlitches
                                      : AudioGlitchResult::kNoGlitches);
  }
  const int kPermilleScaling = 1000;
  // 10%: if we have more that 10% of callbacks having issues, the details are
  // not very interesting any more, so we just log all those cases together to
  // have a better resolution for lower values.
  const int kHistogramRange = kPermilleScaling / 10;

  // 30 s for 10 ms buffers (RTC streams)/ 1 minute for 20 ms buffers (media
  // playback).
  const int kShortStreamMaxCallbackCount = 3000;

  if (renderer_callback_count <= 0)
    return;

  int missed_permille = std::ceil(
      kPermilleScaling * static_cast<double>(renderer_missed_callback_count) /
      renderer_callback_count);

  std::string histogram_name = base::StrCat(
      {"Media.AudioRendererMissedDeadline2.", mixing ? "Mixing." : "",
       (renderer_callback_count < kShortStreamMaxCallbackCount) ? "Short"
                                                                : "Long"});
  base::UmaHistogramCustomCounts(histogram_name,
                                 std::min(missed_permille, kHistogramRange), 0,
                                 kHistogramRange + 1, 100);

  base::UmaHistogramCustomCounts(histogram_name + "." + suffix,
                                 std::min(missed_permille, kHistogramRange), 0,
                                 kHistogramRange + 1, 100);
}

}  // namespace

namespace audio {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)
const base::Feature kDynamicAudioTimeout{"DynamicAudioTimeout",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<double> kBufferDurationPercent{
    &kDynamicAudioTimeout, "buffer_duration_percent", 0.5};
#endif

SyncReader::SyncReader(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    const media::AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket)
    : log_callback_(std::move(log_callback)),
      latency_tag_(params.latency_tag()),
      mute_audio_for_testing_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMuteAudio)),
      output_bus_buffer_size_(
          media::AudioBus::CalculateMemorySize(params.channels(),
                                               params.frames_per_buffer())) {
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
  if (base::FeatureList::IsEnabled(media::kChromeWideEchoCancellation)) {
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
    auto* const buffer = reinterpret_cast<media::AudioOutputBuffer*>(
        shared_memory_mapping_.memory());
    output_bus_ = media::AudioBus::WrapMemory(params, buffer->audio);
    output_bus_->Zero();
    output_bus_->set_is_bitstream_format(params.IsBitstreamFormat());
  }
}

SyncReader::~SyncReader() {
  DCHECK_GE(renderer_callback_count_, mixing_renderer_callback_count_);

  if (!renderer_callback_count_)
    return;

  // Subtract 'trailing' count of callbacks missed just before the destructor
  // call. This happens if the renderer process was killed or e.g. the page
  // refreshed while the output device was open etc.
  // This trims off the end of both the missed and total counts so that we
  // preserve the proportion of counts before the teardown period.
  DCHECK_LE(trailing_renderer_missed_callback_count_,
            renderer_missed_callback_count_);
  DCHECK_LE(trailing_renderer_missed_callback_count_, renderer_callback_count_);

  renderer_missed_callback_count_ -= trailing_renderer_missed_callback_count_;
  renderer_callback_count_ -= trailing_renderer_missed_callback_count_;

  DCHECK_LE(mixing_trailing_renderer_missed_callback_count_,
            mixing_renderer_missed_callback_count_);
  DCHECK_LE(mixing_trailing_renderer_missed_callback_count_,
            mixing_renderer_callback_count_);

  mixing_renderer_missed_callback_count_ -=
      mixing_trailing_renderer_missed_callback_count_;
  mixing_renderer_callback_count_ -=
      mixing_trailing_renderer_missed_callback_count_;

  DCHECK_GE(renderer_callback_count_, mixing_renderer_callback_count_);

  if (!renderer_callback_count_)
    return;

  base::UmaHistogramEnumeration("Media.AudioRendererAudioGlitches",
                                (renderer_missed_callback_count_ > 0)
                                    ? AudioGlitchResult::kGlitches
                                    : AudioGlitchResult::kNoGlitches);
  int percentage_missed =
      100.0 * renderer_missed_callback_count_ / renderer_callback_count_;

  base::UmaHistogramPercentage("Media.AudioRendererMissedDeadline",
                               percentage_missed);

  LogPerLatencyGlitchUma(latency_tag_, renderer_missed_callback_count_,
                         renderer_callback_count_, /*mixing=*/false);

  LogPerLatencyGlitchUma(latency_tag_, mixing_renderer_missed_callback_count_,
                         mixing_renderer_callback_count_, /*mixing=*/true);

  TRACE_EVENT_INSTANT1("audio", "~SyncReader", TRACE_EVENT_SCOPE_THREAD,
                       "Missed callback percentage", percentage_missed);

  log_callback_.Run(base::StringPrintf(
      "ASR: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      renderer_missed_callback_count_, renderer_callback_count_));
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
                                 int prior_frames_skipped) {
  // We don't send arguments over the socket since sending more than 4
  // bytes might lead to being descheduled. The reading side will zero
  // them when consumed.
  auto* const buffer = reinterpret_cast<media::AudioOutputBuffer*>(
      shared_memory_mapping_.memory());
  // Increase the number of skipped frames stored in shared memory.
  buffer->params.frames_skipped += prior_frames_skipped;
  buffer->params.delay_us = delay.InMicroseconds();
  buffer->params.delay_timestamp_us =
      (delay_timestamp - base::TimeTicks()).InMicroseconds();

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

  size_t sent_bytes = socket_.Send(&control_signal, sizeof(control_signal));
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
  }
  ++buffer_index_;
}

void SyncReader::Read(media::AudioBus* dest, bool is_mixing) {
  ++renderer_callback_count_;
  if (is_mixing)
    ++mixing_renderer_callback_count_;

  if (!WaitUntilDataIsReady(is_mixing)) {
    ++trailing_renderer_missed_callback_count_;
    ++renderer_missed_callback_count_;
    if (is_mixing) {
      ++mixing_trailing_renderer_missed_callback_count_;
      ++mixing_renderer_missed_callback_count_;
    }
    if (renderer_missed_callback_count_ <= 100 &&
        renderer_missed_callback_count_ % 10 == 0) {
      LOG(WARNING) << "SyncReader::Read timed out, audio glitch count="
                   << renderer_missed_callback_count_;
      if (renderer_missed_callback_count_ == 100)
        LOG(WARNING) << "(log cap reached, suppressing further logs)";
    }
    dest->Zero();
    return;
  }

  trailing_renderer_missed_callback_count_ = 0;
  mixing_trailing_renderer_missed_callback_count_ = 0;

  // Zeroed buffers may be discarded immediately when outputing compressed
  // bitstream.
  if (mute_audio_for_testing_ && !output_bus_->is_bitstream_format()) {
    dest->Zero();
    return;
  }

  if (output_bus_->is_bitstream_format()) {
    // For bitstream formats, we need the real data size and PCM frame count.
    auto* const buffer = reinterpret_cast<media::AudioOutputBuffer*>(
        shared_memory_mapping_.memory());
    uint32_t data_size = buffer->params.bitstream_data_size;
    uint32_t bitstream_frames = buffer->params.bitstream_frames;
    // |bitstream_frames| is cast to int below, so it must fit.
    if (data_size > output_bus_buffer_size_ ||
        !base::IsValueInRangeForNumericType<int>(bitstream_frames)) {
      // Received data doesn't fit in the buffer, shouldn't happen.
      dest->Zero();
      return;
    }
    output_bus_->SetBitstreamDataSize(data_size);
    output_bus_->SetBitstreamFrames(bitstream_frames);
    output_bus_->CopyTo(dest);
    return;
  }

  // Copy and clip data coming across the shared memory since it's untrusted.
  output_bus_->CopyAndClipTo(dest);
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
        &renderer_buffer_index, sizeof(renderer_buffer_index), timeout);
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
