// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include "base/bind.h"
#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_parameters.h"

namespace media {

namespace {

// Currently AudioCapturer supports only one payload buffer with id=0.
constexpr uint32_t kBufferId = 0;

// Number of audio packets that should fit in the capture buffer.
constexpr size_t kBufferPacketCapacity = 10;

}  // namespace

FuchsiaAudioCapturerSource::FuchsiaAudioCapturerSource(
    fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer_handle)
    : capturer_handle_(std::move(capturer_handle)) {
  DCHECK(capturer_handle_);
  DETACH_FROM_THREAD(thread_checker_);
}

FuchsiaAudioCapturerSource::~FuchsiaAudioCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (capture_buffer_) {
    zx_status_t status = zx::vmar::root_self()->unmap(
        reinterpret_cast<uint64_t>(capture_buffer_), capture_buffer_size_);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
  }
}

void FuchsiaAudioCapturerSource::Initialize(const AudioParameters& params,
                                            CaptureCallback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!capture_buffer_);
  DCHECK(!callback_);
  DCHECK(callback);

  params_ = params;
  callback_ = callback;

  if (params_.format() != AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    ReportError("Only AUDIO_PCM_LOW_LATENCY format is supported");
    return;
  }

  // Bind AudioCapturer.
  capturer_.Bind(std::move(capturer_handle_));
  capturer_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioCapturer disconnected";
    ReportError("AudioCapturer disconnected");
  });

  // Bind the event for incoming packets.
  capturer_.events().OnPacketProduced =
      fit::bind_member(this, &FuchsiaAudioCapturerSource::OnPacketCaptured);

  // TODO(crbug.com/1065207): Enable/disable stream processing based on
  // |params.effects()| when support is added to fuchsia.media.AudioCapturer.

  // Configure stream format.
  fuchsia::media::AudioStreamType stream_type;
  stream_type.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  stream_type.channels = params_.channels();
  stream_type.frames_per_second = params_.sample_rate();
  capturer_->SetPcmStreamType(std::move(stream_type));

  // Allocate shared buffer.
  capture_buffer_size_ =
      params_.GetBytesPerBuffer(kSampleFormatF32) * kBufferPacketCapacity;
  capture_buffer_size_ = base::bits::Align(capture_buffer_size_, ZX_PAGE_SIZE);

  zx::vmo buffer_vmo;
  zx_status_t status = zx::vmo::create(capture_buffer_size_, 0, &buffer_vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

  constexpr char kName[] = "cr-audio-capturer";
  status = buffer_vmo.set_property(ZX_PROP_NAME, kName, base::size(kName) - 1);
  ZX_DCHECK(status == ZX_OK, status);

  // Map the buffer.
  uint64_t addr;
  status = zx::vmar::root_self()->map(
      ZX_VM_PERM_READ, /*vmar_offset=*/0, buffer_vmo, /*vmo_offset=*/0,
      capture_buffer_size_, &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    ReportError("Failed to map capture buffer");
    return;
  }
  capture_buffer_ = reinterpret_cast<uint8_t*>(addr);

  // Pass the buffer to the capturer.
  capturer_->AddPayloadBuffer(kBufferId, std::move(buffer_vmo));
}

void FuchsiaAudioCapturerSource::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Errors are reported asynchronously, so Start() may be called after an error
  // has occurred.
  if (!capturer_)
    return;

  DCHECK(!is_active_);
  is_active_ = true;

  if (!is_capturer_started_) {
    is_capturer_started_ = true;
    capturer_->StartAsyncCapture(params_.frames_per_buffer());
  }

  // Post a task to call OnCaptureStarted() asynchronously, as required by
  // AudioCapturerSource interface..
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuchsiaAudioCapturerSource::NotifyCaptureStarted,
                     weak_factory_.GetWeakPtr()));
}

void FuchsiaAudioCapturerSource::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Errors are reported asynchronously, so Stop() may be called after an error
  // has occurred.
  if (!capturer_)
    return;

  // StopAsyncCapture() is an asynchronous operation that completes
  // asynchronously and other methods cannot be called until it's complete.
  // To avoid extra complexity, update internal state without actually stopping
  // the capturer. The downside is that |capturer_| will keep sending packets in
  // the stopped state. This is acceptable because normally AudioCapturerSource
  // instances are not kept in the stopped state for significant amount of time,
  // i.e. usually either destructor or Start() are called immediately after
  // Stop().
  is_active_ = false;
}

void FuchsiaAudioCapturerSource::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::SetAutomaticGainControl(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::NotifyCaptureError(
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  callback_->OnCaptureError(message);
}

void FuchsiaAudioCapturerSource::NotifyCaptureStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Nothing to do if initialization has failed.
  if (!capturer_ || !is_active_)
    return;

  callback_->OnCaptureStarted();
}

void FuchsiaAudioCapturerSource::OnPacketCaptured(
    fuchsia::media::StreamPacket packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  size_t bytes_per_frame = params_.GetBytesPerFrame(kSampleFormatF32);

  if (packet.payload_buffer_id != kBufferId ||
      packet.payload_offset + packet.payload_size > capture_buffer_size_ ||
      packet.payload_size % bytes_per_frame != 0 ||
      packet.payload_size < bytes_per_frame) {
    ReportError("AudioCapturer produced invalid packet");
    return;
  }

  // If the capturer was stopped then just drop the packet.
  if (is_active_) {
    size_t num_frames = packet.payload_size / bytes_per_frame;
    auto audio_bus = AudioBus::Create(params_.channels(), num_frames);
    audio_bus->FromInterleaved<Float32SampleTypeTraits>(
        reinterpret_cast<const float*>(capture_buffer_ + packet.payload_offset),
        num_frames);
    callback_->Capture(audio_bus.get(), base::TimeTicks::FromZxTime(packet.pts),
                       /*volume=*/1.0,
                       /*key_pressed=*/false);
  }

  capturer_->ReleasePacket(std::move(packet));
}

void FuchsiaAudioCapturerSource::ReportError(const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capturer_.Unbind();

  // Post async task to report the error as required by the the
  // AudioCapturerSource interface.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FuchsiaAudioCapturerSource::NotifyCaptureError,
                                weak_factory_.GetWeakPtr(), message));
}

}  // namespace media
