// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_fifo_sink_adapter.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/fifo_buffer.h"

namespace remoting::protocol {

WebrtcAudioFifoSinkAdapter::WebrtcAudioFifoSinkAdapter(
    std::unique_ptr<FifoBufferWriter> audio_writer,
    FormatChangedCallback format_changed_cb)
    : caller_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      audio_writer_(std::move(audio_writer)),
      format_changed_cb_(std::move(format_changed_cb)) {
  DCHECK(audio_writer_);
  DCHECK(format_changed_cb_);

  notify_format_changed_cb_ = base::BindPostTask(
      caller_task_runner_,
      base::BindRepeating(&WebrtcAudioFifoSinkAdapter::NotifyFormatChanged,
                          weak_factory_.GetWeakPtr()));
}

WebrtcAudioFifoSinkAdapter::~WebrtcAudioFifoSinkAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetTrack(nullptr);
}

void WebrtcAudioFifoSinkAdapter::SetTrack(
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_track_ == track) {
    return;
  }

  if (audio_track_) {
    audio_track_->RemoveSink(this);
  }

  audio_track_ = std::move(track);

  if (audio_track_) {
    audio_track_->AddSink(this);
  }
}

void WebrtcAudioFifoSinkAdapter::OnData(const void* audio_data,
                                        int bits_per_sample,
                                        int sample_rate,
                                        size_t number_of_channels,
                                        size_t number_of_frames) {
  if (bits_per_sample != 16) {
    return;
  }

  AudioSampleInfo new_format{
      .sampling_rate = base::saturated_cast<uint32_t>(sample_rate),
      .channels = base::saturated_cast<uint8_t>(number_of_channels),
  };
  if (!current_format_ || *current_format_ != new_format) {
    current_format_ = new_format;
    uint32_t sequence = ++latest_posted_sequence_;

    notify_format_changed_cb_.Run(*current_format_, sequence);
  }

  if (latest_posted_sequence_ != latest_acknowledged_sequence_) {
    // Drop incoming audio frames until the format change is acknowledged.
    return;
  }

  size_t bytes_per_frame = number_of_channels * (bits_per_sample / 8);
  size_t data_size = number_of_frames * bytes_per_frame;

  // SAFETY: WebRTC's OnData() guarantees that `audio_data` points to a valid
  // buffer containing at least `data_size` bytes of raw PCM audio.
  base::span<const uint8_t> data_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(audio_data), data_size));
  audio_writer_->Write(data_span);
}

void WebrtcAudioFifoSinkAdapter::OnFormatAcknowledged(uint32_t sequence) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Prevent out-of-order asynchronous acknowledgments (e.g., due to Mojo
  // IPC queueing) from retroactively downgrading the latest sequence state.
  // Also safely handles unsigned sequence index wrap-around modularly.
  if (static_cast<int32_t>(sequence - latest_acknowledged_sequence_) > 0) {
    latest_acknowledged_sequence_ = sequence;
  }
}

void WebrtcAudioFifoSinkAdapter::NotifyFormatChanged(
    const AudioSampleInfo& info,
    uint32_t sequence) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  format_changed_cb_.Run(
      info, base::BindOnce(&WebrtcAudioFifoSinkAdapter::OnFormatAcknowledged,
                           weak_factory_.GetWeakPtr(), sequence));
}

}  // namespace remoting::protocol
