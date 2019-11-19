// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer_input.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_pool.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

// TODO(dalecurtis): Merge with AudioDeviceDescription::IsDefaultDevice() once
// that file has been moved to media/base.
bool IsDefaultDevice(const std::string& device_id) {
  return device_id.empty() || device_id == "default";
}

AudioRendererMixerInput::AudioRendererMixerInput(
    AudioRendererMixerPool* mixer_pool,
    int owner_id,
    const std::string& device_id,
    AudioLatency::LatencyType latency)
    : mixer_pool_(mixer_pool),
      owner_id_(owner_id),
      device_id_(device_id),
      latency_(latency) {
  DCHECK(mixer_pool_);
}

AudioRendererMixerInput::~AudioRendererMixerInput() {
  // Note: This may not happen on the thread the sink was used. E.g., this may
  // end up destroyed on the render thread despite being used on the media
  // thread.

  DCHECK(!started_);
  DCHECK(!mixer_);
  if (sink_)
    sink_->Stop();

  // Because GetOutputDeviceInfoAsync() and SwitchOutputDevice() both use
  // base::RetainedRef, it should be impossible to get here with these set.
  DCHECK(!pending_device_info_cb_);
  DCHECK(!pending_switch_cb_);
}

void AudioRendererMixerInput::Initialize(
    const AudioParameters& params,
    AudioRendererSink::RenderCallback* callback) {
  DCHECK(!started_);
  DCHECK(!mixer_);
  DCHECK(callback);

  // Current usage ensures we always call GetOutputDeviceInfoAsync() and wait
  // for the result before calling this method. We could add support for doing
  // otherwise here, but it's not needed for now, so for simplicity just DCHECK.
  DCHECK(sink_);
  DCHECK(device_info_);

  params_ = params;
  callback_ = callback;
}

void AudioRendererMixerInput::Start() {
  DCHECK(!started_);
  DCHECK(!mixer_);
  DCHECK(callback_);  // Initialized.

  DCHECK(sink_);
  DCHECK(device_info_);
  DCHECK_EQ(device_info_->device_status(), OUTPUT_DEVICE_STATUS_OK);

  started_ = true;
  mixer_ = mixer_pool_->GetMixer(owner_id_, params_, latency_, *device_info_,
                                 std::move(sink_));

  // Note: OnRenderError() may be called immediately after this call returns.
  mixer_->AddErrorCallback(this);
}

void AudioRendererMixerInput::Stop() {
  // Stop() may be called at any time, if Pause() hasn't been called we need to
  // remove our mixer input before shutdown.
  Pause();

  if (mixer_) {
    mixer_->RemoveErrorCallback(this);
    mixer_pool_->ReturnMixer(mixer_);
    mixer_ = nullptr;
  }

  started_ = false;
}

void AudioRendererMixerInput::Play() {
  if (playing_ || !mixer_)
    return;

  mixer_->AddMixerInput(params_, this);
  playing_ = true;
}

void AudioRendererMixerInput::Pause() {
  if (!playing_ || !mixer_)
    return;

  mixer_->RemoveMixerInput(params_, this);
  playing_ = false;
}

// Flush is not supported with mixed sinks due to how delayed pausing works in
// the mixer.
void AudioRendererMixerInput::Flush() {}

bool AudioRendererMixerInput::SetVolume(double volume) {
  base::AutoLock auto_lock(volume_lock_);
  volume_ = volume;
  return true;
}

OutputDeviceInfo AudioRendererMixerInput::GetOutputDeviceInfo() {
  NOTREACHED();  // The blocking API is intentionally not supported.
  return OutputDeviceInfo();
}

void AudioRendererMixerInput::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  // If we have device information for a current sink or mixer, just return it
  // immediately. Per the AudioRendererSink API contract, this must be posted.
  if (device_info_.has_value() && (sink_ || mixer_)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(info_cb), *device_info_));
    return;
  }

  if (switch_output_device_in_progress_) {
    DCHECK(!godia_in_progress_);
    pending_device_info_cb_ = std::move(info_cb);
    return;
  }

  godia_in_progress_ = true;

  // We may have |device_info_|, but a Stop() has been called since if we don't
  // have a |sink_| or a |mixer_|, so request the information again in case it
  // has changed (which may occur due to browser side device changes).
  device_info_.reset();

  // If we don't have a sink yet start the process of getting one.
  sink_ = mixer_pool_->GetSink(owner_id_, device_id_);

  // Retain a ref to this sink to ensure it is not destructed while this occurs.
  // The callback is guaranteed to execute on this thread, so there are no
  // threading issues.
  sink_->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInput::OnDeviceInfoReceived,
                     base::RetainedRef(this), std::move(info_cb)));
}

bool AudioRendererMixerInput::IsOptimizedForHardwareParameters() {
  return true;
}

bool AudioRendererMixerInput::CurrentThreadIsRenderingThread() {
  return mixer_->CurrentThreadIsRenderingThread();
}

void AudioRendererMixerInput::SwitchOutputDevice(
    const std::string& device_id,
    OutputDeviceStatusCB callback) {
  // If a GODIA() call is in progress, defer until it's complete.
  if (godia_in_progress_) {
    DCHECK(!switch_output_device_in_progress_);

    // Abort any previous device switch which may be pending.
    if (pending_switch_cb_)
      std::move(pending_switch_cb_).Run(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);

    pending_device_id_ = device_id;
    pending_switch_cb_ = std::move(callback);
    return;
  }

  // Some pages send "default" instead of the spec compliant empty string for
  // the default device. Short circuit these here to avoid busy work.
  if (device_id == device_id_ ||
      (IsDefaultDevice(device_id_) && IsDefaultDevice(device_id))) {
    std::move(callback).Run(OUTPUT_DEVICE_STATUS_OK);
    return;
  }

  switch_output_device_in_progress_ = true;

  // Request a new sink using the new device id. This process may fail, so to
  // avoid interrupting working audio, don't set any class variables until we
  // know it's a success.
  auto new_sink = mixer_pool_->GetSink(owner_id_, device_id);

  // Retain a ref to this sink to ensure it is not destructed while this occurs.
  // The callback is guaranteed to execute on this thread, so there are no
  // threading issues.
  new_sink->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInput::OnDeviceSwitchReady,
                     base::RetainedRef(this), std::move(callback), new_sink));
}

double AudioRendererMixerInput::ProvideInput(AudioBus* audio_bus,
                                             uint32_t frames_delayed) {
  TRACE_EVENT0("audio", "AudioRendererMixerInput::ProvideInput");
  const base::TimeDelta delay =
      AudioTimestampHelper::FramesToTime(frames_delayed, params_.sample_rate());

  int frames_filled =
      callback_->Render(delay, base::TimeTicks::Now(), 0, audio_bus);

  // AudioConverter expects unfilled frames to be zeroed.
  if (frames_filled < audio_bus->frames()) {
    audio_bus->ZeroFramesPartial(frames_filled,
                                 audio_bus->frames() - frames_filled);
  }

  // We're reading |volume_| from the audio device thread and must avoid racing
  // with the main/media thread calls to SetVolume(). See thread safety comment
  // in the header file.
  {
    base::AutoLock auto_lock(volume_lock_);
    return frames_filled > 0 ? volume_ : 0;
  }
}

void AudioRendererMixerInput::OnRenderError() {
  callback_->OnRenderError();
}

void AudioRendererMixerInput::OnDeviceInfoReceived(
    OutputDeviceInfoCB info_cb,
    OutputDeviceInfo device_info) {
  DCHECK(godia_in_progress_);
  godia_in_progress_ = false;

  device_info_ = device_info;
  std::move(info_cb).Run(*device_info_);

  // Complete any pending SwitchOutputDevice() if needed. We don't post this to
  // ensure we don't reorder calls relative to what the page is expecting. I.e.,
  // if we post we could end up with Switch(1) -> Switch(2) ending on Switch(1).
  if (!pending_switch_cb_)
    return;
  SwitchOutputDevice(std::move(pending_device_id_),
                     std::move(pending_switch_cb_));
}

void AudioRendererMixerInput::OnDeviceSwitchReady(
    OutputDeviceStatusCB switch_cb,
    scoped_refptr<AudioRendererSink> sink,
    OutputDeviceInfo device_info) {
  DCHECK(switch_output_device_in_progress_);
  switch_output_device_in_progress_ = false;

  if (device_info.device_status() != OUTPUT_DEVICE_STATUS_OK) {
    sink->Stop();
    std::move(switch_cb).Run(device_info.device_status());

    // Start any pending device info request.
    if (pending_device_info_cb_)
      GetOutputDeviceInfoAsync(std::move(pending_device_info_cb_));

    return;
  }

  const bool has_mixer = !!mixer_;
  const bool is_playing = playing_;

  // This may occur if Start() hasn't yet been called.
  if (sink_)
    sink_->Stop();

  sink_ = std::move(sink);
  device_info_ = device_info;
  device_id_ = device_info.device_id();

  Stop();
  if (has_mixer) {
    Start();
    if (is_playing)
      Play();
  }

  std::move(switch_cb).Run(device_info.device_status());

  // Start any pending device info request.
  if (pending_device_info_cb_)
    GetOutputDeviceInfoAsync(std::move(pending_device_info_cb_));
}

}  // namespace media
