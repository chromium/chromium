// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_pool.h"

namespace blink {

constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(5);

AudioRendererMixerInput::AudioRendererMixerInput(
    AudioRendererMixerPool* mixer_pool,
    const LocalFrameToken& source_frame_token,
    const FrameToken& main_frame_token,
    std::string_view device_id,
    media::AudioLatency::Type latency)
    : mixer_pool_(mixer_pool),
      source_frame_token_(source_frame_token),
      main_frame_token_(main_frame_token),
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
  if (sink_) {
    sink_->Stop();
  }

  // Because GetOutputDeviceInfoAsync() and SwitchOutputDevice() both use
  // base::RetainedRef, it should be impossible to get here with these set.
  DCHECK(!pending_device_info_cb_);
  DCHECK(!pending_switch_cb_);
}

void AudioRendererMixerInput::Initialize(
    const media::AudioParameters& params,
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

  total_fade_in_frames_ =
      static_cast<int>(media::AudioTimestampHelper::TimeToFrames(
          kFadeInDuration, params_.sample_rate()));
}

void AudioRendererMixerInput::Start() {
  DCHECK(!started_);
  DCHECK(!mixer_);
  DCHECK(callback_);  // Initialized.
  DCHECK(sink_);

  // It's important that `sink` has already been authorized to ensure we don't
  // allow sharing between RenderFrames not authorized for sending audio to a
  // given device.
  CHECK(device_info_);
  CHECK_EQ(device_info_->device_status(), media::OUTPUT_DEVICE_STATUS_OK);

  started_ = true;
  mixer_ = mixer_pool_->GetMixer(main_frame_token_, params_, latency_,
                                 *device_info_, std::move(sink_));

  // Note: OnRenderError() may be called immediately after this call returns.
  mixer_->AddErrorCallback(this);
}

void AudioRendererMixerInput::Stop() {
  // Stop() may be called at any time, if Pause() hasn't been called we need to
  // remove our mixer input before shutdown.
  Pause();

  if (mixer_) {
    mixer_->RemoveErrorCallback(this);
    mixer_pool_->ReturnMixer(mixer_.ExtractAsDangling());
    DCHECK(!mixer_);
  }
  callback_ = nullptr;
  started_ = false;
}

void AudioRendererMixerInput::Play() {
  if (playing_ || !mixer_) {
    return;
  }

  // Fading in the first few frames avoids an audible pop.
  remaining_fade_in_frames_ = total_fade_in_frames_;

  mixer_->AddMixerInput(params_, this);
  playing_ = true;
}

void AudioRendererMixerInput::Pause() {
  if (!playing_ || !mixer_) {
    return;
  }

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

media::OutputDeviceInfo AudioRendererMixerInput::GetOutputDeviceInfo() {
  NOTREACHED_IN_MIGRATION();  // The blocking API is intentionally not
                              // supported.
  return media::OutputDeviceInfo();
}

void AudioRendererMixerInput::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  // If we have device information for a current sink or mixer, just return it
  // immediately. Per the AudioRendererSink API contract, this must be posted.
  if (device_info_.has_value() && (sink_ || mixer_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(info_cb), *device_info_));
    return;
  }

  if (switch_output_device_in_progress_) {
    DCHECK(!godia_in_progress_);
    pending_device_info_cb_ = std::move(info_cb);
    return;
  }

  godia_in_progress_ = true;

  // We may have `device_info_`, but a Stop() has been called since if we don't
  // have a `sink_` or a `mixer_`, so request the information again in case it
  // has changed (which may occur due to browser side device changes).
  device_info_.reset();

  // If we don't have a sink yet start the process of getting one.
  sink_ = mixer_pool_->GetSink(source_frame_token_, device_id_);

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
    media::OutputDeviceStatusCB callback) {
  // If a GODIA() call is in progress, defer until it's complete.
  if (godia_in_progress_) {
    DCHECK(!switch_output_device_in_progress_);

    // Abort any previous device switch which may be pending.
    if (pending_switch_cb_) {
      std::move(pending_switch_cb_)
          .Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    }

    pending_device_id_ = device_id;
    pending_switch_cb_ = std::move(callback);
    return;
  }

  // Some pages send "default" instead of the spec compliant empty string for
  // the default device. Short circuit these here to avoid busy work.
  if (device_id == device_id_ ||
      (media::AudioDeviceDescription::IsDefaultDevice(device_id_) &&
       media::AudioDeviceDescription::IsDefaultDevice(device_id))) {
    std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_OK);
    return;
  }

  switch_output_device_in_progress_ = true;

  // Request a new sink using the new device id. This process may fail, so to
  // avoid interrupting working audio, don't set any class variables until we
  // know it's a success.
  auto new_sink = mixer_pool_->GetSink(source_frame_token_, device_id);

  // Retain a ref to this sink to ensure it is not destructed while this occurs.
  // The callback is guaranteed to execute on this thread, so there are no
  // threading issues.
  new_sink->GetOutputDeviceInfoAsync(
      base::BindOnce(&AudioRendererMixerInput::OnDeviceSwitchReady,
                     base::RetainedRef(this), std::move(callback), new_sink));
}

double AudioRendererMixerInput::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed,
    const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "AudioRendererMixerInput::ProvideInput",
              "delay (frames)", frames_delayed);
  const base::TimeDelta delay = media::AudioTimestampHelper::FramesToTime(
      frames_delayed, params_.sample_rate());

  int frames_filled =
      callback_->Render(delay, base::TimeTicks::Now(), glitch_info, audio_bus);

  // AudioConverter expects unfilled frames to be zeroed.
  if (frames_filled < audio_bus->frames()) {
    audio_bus->ZeroFramesPartial(frames_filled,
                                 audio_bus->frames() - frames_filled);
  }

  if (remaining_fade_in_frames_) {
    // On MacOS, `audio_bus` might be 2ms long, and the fade needs to be applied
    // over multiple buffers.
    const int frames = std::min(remaining_fade_in_frames_, audio_bus->frames());

    DCHECK_LE(remaining_fade_in_frames_, total_fade_in_frames_);
    const int start_volume = total_fade_in_frames_ - remaining_fade_in_frames_;
    DCHECK_GE(start_volume, 0);

    // Apply a perfect linear fade-in. Fading-in in steps (e.g. increasing
    // volume by 10% every 1ms over 10ms) introduces high frequency distortions.
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      float* data = audio_bus->channel(ch);

      for (int i = 0; i < frames; ++i) {
        data[i] *= static_cast<float>(start_volume + i) / total_fade_in_frames_;
      }
    }

    remaining_fade_in_frames_ -= frames;

    DCHECK_GE(remaining_fade_in_frames_, 0);
  }

  // We're reading `volume_` from the audio device thread and must avoid racing
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
    media::OutputDeviceInfo device_info) {
  DCHECK(godia_in_progress_);
  godia_in_progress_ = false;

  device_info_ = device_info;
  std::move(info_cb).Run(*device_info_);

  // Complete any pending SwitchOutputDevice() if needed. We don't post this to
  // ensure we don't reorder calls relative to what the page is expecting. I.e.,
  // if we post we could end up with Switch(1) -> Switch(2) ending on Switch(1).
  if (!pending_switch_cb_) {
    return;
  }
  SwitchOutputDevice(std::move(pending_device_id_),
                     std::move(pending_switch_cb_));
}

void AudioRendererMixerInput::OnDeviceSwitchReady(
    media::OutputDeviceStatusCB switch_cb,
    scoped_refptr<media::AudioRendererSink> sink,
    media::OutputDeviceInfo device_info) {
  DCHECK(switch_output_device_in_progress_);
  switch_output_device_in_progress_ = false;

  if (device_info.device_status() != media::OUTPUT_DEVICE_STATUS_OK) {
    sink->Stop();
    std::move(switch_cb).Run(device_info.device_status());

    // Start any pending device info request.
    if (pending_device_info_cb_) {
      GetOutputDeviceInfoAsync(std::move(pending_device_info_cb_));
    }

    return;
  }

  const bool has_mixer = !!mixer_;
  const bool is_playing = playing_;

  // This may occur if Start() hasn't yet been called.
  if (sink_) {
    sink_->Stop();
  }

  sink_ = std::move(sink);
  device_info_ = device_info;
  device_id_ = device_info.device_id();

  auto callback = callback_;
  Stop();
  callback_ = callback;

  if (has_mixer) {
    Start();
    if (is_playing) {
      Play();
    }
  }

  std::move(switch_cb).Run(device_info.device_status());

  // Start any pending device info request.
  if (pending_device_info_cb_) {
    GetOutputDeviceInfoAsync(std::move(pending_device_info_cb_));
  }
}

}  // namespace blink
