// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/fuchsia/audio_input_stream_fuchsia.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/vmo.h>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/fuchsia/audio_manager_fuchsia.h"

namespace media {

namespace {

// Currently AudioCapturer supports only one payload buffer with id=0.
constexpr uint32_t kBufferId = 0;

// Number of audio packets that should fit in the capture buffer.
constexpr size_t kBufferPacketCapacity = 10;

}  // namespace

AudioInputStreamFuchsia::AudioInputStreamFuchsia(
    AudioManagerFuchsia* manager,
    const AudioParameters& parameters,
    std::string device_id)
    : manager_(manager),
      parameters_(parameters),
      device_id_(std::move(device_id)) {
  DCHECK(device_id_.empty() ||
         device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId ||
         device_id_ == AudioDeviceDescription::kDefaultDeviceId)
      << "AudioInput from " << device_id_ << " not implemented!";
  DCHECK(parameters_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY ||
         parameters_.format() == AudioParameters::AUDIO_PCM_LINEAR);
}

AudioInputStreamFuchsia::~AudioInputStreamFuchsia() = default;

AudioInputStream::OpenOutcome AudioInputStreamFuchsia::Open() {
  // Open() can be called only once.
  DCHECK(!capturer_);

  auto factory = base::ComponentContextForProcess()
                     ->svc()
                     ->Connect<fuchsia::media::Audio>();
  bool is_loopback =
      device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId;
  factory->CreateAudioCapturer(capturer_.NewRequest(), is_loopback);
  capturer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioCapturer disconnected";
    ReportError();
  });

  // Bind the event for incoming packets.
  capturer_.events().OnPacketProduced =
      fit::bind_member(this, &AudioInputStreamFuchsia::OnPacketProduced);

  // Configure stream format.
  fuchsia::media::AudioStreamType stream_type;
  stream_type.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  stream_type.channels = parameters_.channels();
  stream_type.frames_per_second = parameters_.sample_rate();
  capturer_->SetPcmStreamType(std::move(stream_type));

  // Allocate shared buffer.
  size_t capture_buffer_size =
      parameters_.GetBytesPerBuffer(kSampleFormatF32) * kBufferPacketCapacity;
  capture_buffer_size = base::bits::AlignUp(capture_buffer_size, ZX_PAGE_SIZE);

  zx::vmo buffer_vmo;
  zx_status_t status = zx::vmo::create(capture_buffer_size, 0, &buffer_vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

  constexpr char kName[] = "cr-audio-capturer";
  status = buffer_vmo.set_property(ZX_PROP_NAME, kName, std::size(kName) - 1);
  ZX_DCHECK(status == ZX_OK, status);

  bool mapped =
      capture_buffer_.Initialize(std::move(buffer_vmo), /*writable=*/false,
                                 /*offset=*/0, /*size=*/capture_buffer_size,
                                 fuchsia::sysmem2::CoherencyDomain::CPU);

  if (!mapped)
    return OpenOutcome::kFailed;

  // Pass the buffer to the capturer.
  capturer_->AddPayloadBuffer(kBufferId,
                              capture_buffer_.Duplicate(/*writable=*/true));

  return OpenOutcome::kSuccess;
}

void AudioInputStreamFuchsia::Start(AudioInputCallback* callback) {
  if (!capturer_) {
    callback->OnError();
    return;
  }

  callback_ = callback;

  if (!is_capturer_started_) {
    is_capturer_started_ = true;
    capturer_->StartAsyncCapture(parameters_.frames_per_buffer());
  }
}

void AudioInputStreamFuchsia::Stop() {
  // Normally Close() is called immediately after Stop(), so there is no need to
  // stop the capturer. Just release the |callback_| to ensure it's not called
  // again.
  callback_ = nullptr;
}

void AudioInputStreamFuchsia::Close() {
  Stop();
  if (manager_)
    manager_->ReleaseInputStream(this);
}

double AudioInputStreamFuchsia::GetMaxVolume() {
  return 1.0;
}

void AudioInputStreamFuchsia::SetVolume(double volume) {
  NOTIMPLEMENTED();
}

double AudioInputStreamFuchsia::GetVolume() {
  return 1.0;
}

bool AudioInputStreamFuchsia::SetAutomaticGainControl(bool enabled) {
  NOTIMPLEMENTED();
  return false;
}

bool AudioInputStreamFuchsia::GetAutomaticGainControl() {
  return false;
}

bool AudioInputStreamFuchsia::IsMuted() {
  return false;
}

void AudioInputStreamFuchsia::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTIMPLEMENTED();
}

void AudioInputStreamFuchsia::OnPacketProduced(
    fuchsia::media::StreamPacket packet) {
  size_t bytes_per_frame = parameters_.GetBytesPerFrame(kSampleFormatF32);

  if (packet.payload_buffer_id != kBufferId ||
      packet.payload_offset + packet.payload_size > capture_buffer_.size() ||
      packet.payload_size % bytes_per_frame != 0 ||
      packet.payload_size < bytes_per_frame) {
    LOG(ERROR) << "Received invalid packet from AudioCapturer.";
    ReportError();
    return;
  }

  if (callback_) {
    int num_frames = packet.payload_size / bytes_per_frame;
    if (!audio_bus_ || num_frames != audio_bus_->frames())
      audio_bus_ = AudioBus::Create(parameters_.channels(), num_frames);
    audio_bus_->FromInterleaved<Float32SampleTypeTraits>(
        reinterpret_cast<const float*>(capture_buffer_.GetMemory().data() +
                                       packet.payload_offset),
        num_frames);
    callback_->OnData(audio_bus_.get(), base::TimeTicks::FromZxTime(packet.pts),
                      /*volume=*/1.0, {});
  }

  capturer_->ReleasePacket(std::move(packet));
}

void AudioInputStreamFuchsia::ReportError() {
  capturer_.Unbind();
  if (callback_)
    callback_->OnError();
}

}  // namespace media
