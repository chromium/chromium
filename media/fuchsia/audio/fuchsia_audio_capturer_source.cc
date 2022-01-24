// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include "base/bind.h"
#include "base/bits.h"
#include "base/cxx17_backports.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/location.h"
#include "base/task/task_runner.h"
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
    fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer_handle,
    scoped_refptr<base::SingleThreadTaskRunner> capturer_task_runner)
    : capturer_handle_(std::move(capturer_handle)),
      capturer_task_runner_(capturer_task_runner) {
  DCHECK(capturer_handle_);
}

FuchsiaAudioCapturerSource::~FuchsiaAudioCapturerSource() {
  DCHECK(!callback_)
      << "Stop() must be called before FuchsiaAudioCapturerSource is released.";

  if (capture_buffer_) {
    zx_status_t status = zx::vmar::root_self()->unmap(
        reinterpret_cast<uint64_t>(capture_buffer_), capture_buffer_size_);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
  }
}

void FuchsiaAudioCapturerSource::Initialize(const AudioParameters& params,
                                            CaptureCallback* callback) {
  DCHECK(!capture_buffer_);
  DCHECK(!callback_);
  DCHECK(callback);

  main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  params_ = params;
  callback_ = callback;

  if (params_.format() != AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    ReportError("Only AUDIO_PCM_LOW_LATENCY format is supported");
    return;
  }
  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FuchsiaAudioCapturerSource::InitializeOnCapturerThread,
                     this));
}

void FuchsiaAudioCapturerSource::Start() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_);

  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FuchsiaAudioCapturerSource::StartOnCapturerThread, this));
}

void FuchsiaAudioCapturerSource::Stop() {
  // Nothing to do if Initialize() hasn't been called.
  if (!main_task_runner_)
    return;

  DCHECK(main_task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock lock(callback_lock_);

    if (!callback_)
      return;

    callback_ = nullptr;
  }

  capturer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FuchsiaAudioCapturerSource::StopOnCapturerThread, this));
}

void FuchsiaAudioCapturerSource::SetVolume(double volume) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::SetAutomaticGainControl(bool enable) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void FuchsiaAudioCapturerSource::InitializeOnCapturerThread() {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());

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
  capture_buffer_size_ =
      base::bits::AlignUp(capture_buffer_size_, ZX_PAGE_SIZE);

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

void FuchsiaAudioCapturerSource::StartOnCapturerThread() {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());

  // Errors are reported asynchronously, so Start() may be called after an error
  // has occurred.
  if (!capturer_)
    return;

  if (!is_capturer_started_) {
    is_capturer_started_ = true;
    capturer_->StartAsyncCapture(params_.frames_per_buffer());
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FuchsiaAudioCapturerSource::NotifyCaptureStarted, this));
}

void FuchsiaAudioCapturerSource::StopOnCapturerThread() {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());
  capturer_.Unbind();
}

void FuchsiaAudioCapturerSource::NotifyCaptureError(
    const std::string& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Nothing to do if Stop() was called.
  if (!callback_)
    return;

  // `Stop()` cannot be called on other threads, so `callback_lock_` doesn't
  // need to be held.
  callback_->OnCaptureError(AudioCapturerSource::ErrorCode::kUnknown, message);
}

void FuchsiaAudioCapturerSource::NotifyCaptureStarted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Nothing to do if Stop() was called.
  if (!callback_)
    return;

  // `Stop()` cannot be called on other threads, so `callback_lock_` doesn't
  // need to be held.
  callback_->OnCaptureStarted();
}

void FuchsiaAudioCapturerSource::OnPacketCaptured(
    fuchsia::media::StreamPacket packet) {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());

  size_t bytes_per_frame = params_.GetBytesPerFrame(kSampleFormatF32);

  if (packet.payload_buffer_id != kBufferId ||
      packet.payload_offset + packet.payload_size > capture_buffer_size_ ||
      packet.payload_size % bytes_per_frame != 0 ||
      packet.payload_size < bytes_per_frame) {
    ReportError("AudioCapturer produced invalid packet");
    return;
  }

  // Keep the lock when calling `Capture()` to ensure that we don't call the
  // callback after `Stop()`. If `Stop()` is called on the main thread while the
  // lock is held it will wait until we release the lock below. This is
  // acceptable because `CaptureCallback::Capture()` is expected to return
  // quickly.
  base::AutoLock lock(callback_lock_);

  // If `Stop()` was called then we can drop the capturer - it won't be used
  // again.
  if (!callback_)
    capturer_.Unbind();

  size_t num_frames = packet.payload_size / bytes_per_frame;
  auto audio_bus = AudioBus::Create(params_.channels(), num_frames);
  audio_bus->FromInterleaved<Float32SampleTypeTraits>(
      reinterpret_cast<const float*>(capture_buffer_ + packet.payload_offset),
      num_frames);
  callback_->Capture(audio_bus.get(), base::TimeTicks::FromZxTime(packet.pts),
                     /*volume=*/1.0,
                     /*key_pressed=*/false);

  capturer_->ReleasePacket(std::move(packet));
}

void FuchsiaAudioCapturerSource::ReportError(const std::string& message) {
  DCHECK(capturer_task_runner_->BelongsToCurrentThread());

  capturer_.Unbind();

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FuchsiaAudioCapturerSource::NotifyCaptureError,
                                this, message));
}

}  // namespace media
