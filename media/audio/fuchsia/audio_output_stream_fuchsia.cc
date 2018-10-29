// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_output_stream_fuchsia.h"

#include <zircon/syscalls.h>

#include "base/fuchsia/component_context.h"
#include "media/audio/fuchsia/audio_manager_fuchsia.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

// Current AudioRenderer implementation allows only one buffer with id=0.
// TODO(sergeyu): Replace with an incrementing buffer id once AddPayloadBuffer()
// and RemovePayloadBuffer() are implemented properly in AudioRenderer.
const uint32_t kBufferId = 0;

AudioOutputStreamFuchsia::AudioOutputStreamFuchsia(
    AudioManagerFuchsia* manager,
    const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      audio_bus_(AudioBus::Create(parameters)) {}

AudioOutputStreamFuchsia::~AudioOutputStreamFuchsia() {
  // Close() must be called first.
  DCHECK(!audio_renderer_);
}

bool AudioOutputStreamFuchsia::Open() {
  DCHECK(!audio_renderer_);

  // Connect |audio_renderer_| to the audio service.
  fuchsia::media::AudioPtr audio_server =
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToService<fuchsia::media::Audio>();
  audio_server->CreateAudioRenderer(audio_renderer_.NewRequest());
  audio_renderer_.set_error_handler(
      fit::bind_member(this, &AudioOutputStreamFuchsia::OnRendererError));

  // Inform the |audio_renderer_| of the format required by the caller.
  fuchsia::media::AudioStreamType format;
  format.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  format.channels = parameters_.channels();
  format.frames_per_second = parameters_.sample_rate();
  audio_renderer_->SetPcmStreamType(std::move(format));

  // Use number of samples to specify media position.
  audio_renderer_->SetPtsUnits(parameters_.sample_rate(), 1);

  // Setup OnMinLeadTimeChanged event listener. This event is used to get
  // |min_lead_time_|, which indicates how far ahead audio samples need to be
  // sent to the renderer.
  audio_renderer_.events().OnMinLeadTimeChanged =
      fit::bind_member(this, &AudioOutputStreamFuchsia::OnMinLeadTimeChanged);
  audio_renderer_->EnableMinLeadTimeEvents(true);

  // The renderer may fail initialization asynchronously, which is handled in
  // OnRendererError().
  return true;
}

void AudioOutputStreamFuchsia::Start(AudioSourceCallback* callback) {
  DCHECK(!callback_);
  DCHECK(reference_time_.is_null());
  DCHECK(!timer_.IsRunning());
  callback_ = callback;

  PumpSamples();
}

void AudioOutputStreamFuchsia::Stop() {
  callback_ = nullptr;
  reference_time_ = base::TimeTicks();
  audio_renderer_->PauseNoReply();
  audio_renderer_->DiscardAllPacketsNoReply();
  timer_.Stop();
}

void AudioOutputStreamFuchsia::SetVolume(double volume) {
  DCHECK(0.0 <= volume && volume <= 1.0) << volume;
  volume_ = volume;
}

void AudioOutputStreamFuchsia::GetVolume(double* volume) {
  *volume = volume_;
}

void AudioOutputStreamFuchsia::Close() {
  Stop();
  audio_renderer_.Unbind();

  // Signal to the manager that we're closed and can be removed. This should be
  // the last call in the function as it deletes |this|.
  manager_->ReleaseOutputStream(this);
}

base::TimeTicks AudioOutputStreamFuchsia::GetCurrentStreamTime() {
  DCHECK(!reference_time_.is_null());
  return reference_time_ +
         AudioTimestampHelper::FramesToTime(stream_position_samples_,
                                            parameters_.sample_rate());
}

size_t AudioOutputStreamFuchsia::GetMinBufferSize() {
  // Ensure that |payload_buffer_| fits enough packets to cover min_lead_time_
  // plus one extra packet.
  int min_packets = (AudioTimestampHelper::TimeToFrames(
                         min_lead_time_, parameters_.sample_rate()) +
                     parameters_.frames_per_buffer() - 1) /
                        parameters_.frames_per_buffer() +
                    1;

  return parameters_.GetBytesPerBuffer(kSampleFormatF32) * min_packets;
}

bool AudioOutputStreamFuchsia::InitializePayloadBuffer() {
  size_t buffer_size = GetMinBufferSize();
  if (!payload_buffer_.CreateAndMapAnonymous(buffer_size)) {
    LOG(WARNING) << "Failed to allocate VMO of size " << buffer_size;
    return false;
  }

  payload_buffer_pos_ = 0;
  audio_renderer_->AddPayloadBuffer(
      kBufferId, zx::vmo(payload_buffer_.handle().Duplicate().GetHandle()));

  return true;
}

void AudioOutputStreamFuchsia::OnMinLeadTimeChanged(int64_t min_lead_time) {
  min_lead_time_ = base::TimeDelta::FromNanoseconds(min_lead_time);

  // When min_lead_time_ increases we may need to reallocate |payload_buffer_|.
  // Code below just unmaps the current buffer. The new buffer will be allocated
  // lated in PumpSamples(). This is necessary because VMO allocation may fail
  // and it's not possible to report that error here - OnMinLeadTimeChanged()
  // may be invoked before Start().
  if (payload_buffer_.mapped_size() > 0 &&
      GetMinBufferSize() > payload_buffer_.mapped_size()) {
    payload_buffer_.Unmap();
  }
}

void AudioOutputStreamFuchsia::OnRendererError() {
  LOG(WARNING) << "AudioRenderer has failed.";
  ReportError();
}

void AudioOutputStreamFuchsia::ReportError() {
  reference_time_ = base::TimeTicks();
  timer_.Stop();
  if (callback_)
    callback_->OnError();
}

void AudioOutputStreamFuchsia::PumpSamples() {
  DCHECK(audio_renderer_);

  // Allocate payload buffer if necessary.
  if (!payload_buffer_.mapped_size() && !InitializePayloadBuffer()) {
    ReportError();
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta delay;
  if (reference_time_.is_null()) {
    delay = min_lead_time_;
  } else {
    auto stream_time = GetCurrentStreamTime();

    // Adjust stream position if we missed timer deadline.
    if (now + min_lead_time_ > stream_time) {
      stream_position_samples_ += AudioTimestampHelper::TimeToFrames(
          now + min_lead_time_ - stream_time, parameters_.sample_rate());
    }

    delay = stream_time - now;
  }

  // Start playback if the stream was previously stopped.
  if (reference_time_.is_null()) {
    stream_position_samples_ = 0;
    reference_time_ = now + min_lead_time_;
    audio_renderer_->PlayNoReply(reference_time_.ToZxTime(),
                                 stream_position_samples_);
  }

  // Request more samples from |callback_|.
  int frames_filled = callback_->OnMoreData(delay, now, 0, audio_bus_.get());
  DCHECK_EQ(frames_filled, audio_bus_->frames());

  audio_bus_->Scale(volume_);

  // Save samples to the |payload_buffer_|.
  size_t packet_size = parameters_.GetBytesPerBuffer(kSampleFormatF32);
  DCHECK_LE(payload_buffer_pos_ + packet_size, payload_buffer_.mapped_size());
  audio_bus_->ToInterleaved<media::Float32SampleTypeTraits>(
      audio_bus_->frames(),
      reinterpret_cast<float*>(static_cast<uint8_t*>(payload_buffer_.memory()) +
                               payload_buffer_pos_));

  // Send a new packet.
  fuchsia::media::StreamPacket packet;
  packet.pts = stream_position_samples_;
  packet.payload_buffer_id = kBufferId;
  packet.payload_offset = payload_buffer_pos_;
  packet.payload_size = packet_size;
  packet.flags = 0;
  audio_renderer_->SendPacketNoReply(std::move(packet));

  stream_position_samples_ += frames_filled;
  payload_buffer_pos_ =
      (payload_buffer_pos_ + packet_size) % payload_buffer_.mapped_size();

  SchedulePumpSamples(now);
}

void AudioOutputStreamFuchsia::SchedulePumpSamples(base::TimeTicks now) {
  base::TimeTicks next_pump_time = GetCurrentStreamTime() - min_lead_time_ -
                                   parameters_.GetBufferDuration() / 2;
  timer_.Start(FROM_HERE, next_pump_time - now,
               base::Bind(&AudioOutputStreamFuchsia::PumpSamples,
                          base::Unretained(this)));
}

}  // namespace media
