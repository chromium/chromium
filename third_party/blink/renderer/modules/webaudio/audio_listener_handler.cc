// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_listener_handler.h"

#include "third_party/blink/renderer/modules/webaudio/panner_handler.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"

namespace blink {

scoped_refptr<AudioListenerHandler> AudioListenerHandler::Create(
    AudioParamHandler& position_x_handler,
    AudioParamHandler& position_y_handler,
    AudioParamHandler& position_z_handler,
    AudioParamHandler& forward_x_handler,
    AudioParamHandler& forward_y_handler,
    AudioParamHandler& forward_z_handler,
    AudioParamHandler& up_x_handler,
    AudioParamHandler& up_y_handler,
    AudioParamHandler& up_z_handler,
    unsigned int render_quantum_frames) {
  return base::AdoptRef(new AudioListenerHandler(
      position_x_handler, position_y_handler, position_z_handler,
      forward_x_handler, forward_y_handler, forward_z_handler,
      up_x_handler, up_y_handler, up_z_handler, render_quantum_frames));
}

AudioListenerHandler::AudioListenerHandler(
    AudioParamHandler& position_x_handler,
    AudioParamHandler& position_y_handler,
    AudioParamHandler& position_z_handler,
    AudioParamHandler& forward_x_handler,
    AudioParamHandler& forward_y_handler,
    AudioParamHandler& forward_z_handler,
    AudioParamHandler& up_x_handler,
    AudioParamHandler& up_y_handler,
    AudioParamHandler& up_z_handler,
    unsigned int render_quantum_frames)
    : position_x_handler_(&position_x_handler),
      position_y_handler_(&position_y_handler),
      position_z_handler_(&position_z_handler),
      forward_x_handler_(&forward_x_handler),
      forward_y_handler_(&forward_y_handler),
      forward_z_handler_(&forward_z_handler),
      up_x_handler_(&up_x_handler),
      up_y_handler_(&up_y_handler),
      up_z_handler_(&up_z_handler),
      position_x_values_(render_quantum_frames),
      position_y_values_(render_quantum_frames),
      position_z_values_(render_quantum_frames),
      forward_x_values_(render_quantum_frames),
      forward_y_values_(render_quantum_frames),
      forward_z_values_(render_quantum_frames),
      up_x_values_(render_quantum_frames),
      up_y_values_(render_quantum_frames),
      up_z_values_(render_quantum_frames) {
  // Initialize the cached values with the current values.  Thus, we don't need
  // to notify any panners because we haved moved.
  last_position_ = GetPosition();
  last_forward_ = GetOrientation();
  last_up_ = GetUpVector();
}

AudioListenerHandler::~AudioListenerHandler() {
  position_x_handler_ = nullptr;
  position_y_handler_ = nullptr;
  position_z_handler_ = nullptr;
  forward_x_handler_ = nullptr;
  forward_y_handler_ = nullptr;
  forward_z_handler_ = nullptr;
  up_x_handler_ = nullptr;
  up_y_handler_ = nullptr;
  up_z_handler_ = nullptr;
  hrtf_database_loader_ = nullptr;
  panner_handlers_.clear();
}

const float* AudioListenerHandler::GetPositionXValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_x_values_.Data();
}

const float* AudioListenerHandler::GetPositionYValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_y_values_.Data();
}

const float* AudioListenerHandler::GetPositionZValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_z_values_.Data();
}

const float* AudioListenerHandler::GetForwardXValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_x_values_.Data();
}

const float* AudioListenerHandler::GetForwardYValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_y_values_.Data();
}

const float* AudioListenerHandler::GetForwardZValues(
    uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_z_values_.Data();
}

const float* AudioListenerHandler::GetUpXValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_x_values_.Data();
}

const float* AudioListenerHandler::GetUpYValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_y_values_.Data();
}

const float* AudioListenerHandler::GetUpZValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_z_values_.Data();
}

bool AudioListenerHandler::HasSampleAccurateValues() const {
  return position_x_handler_->HasSampleAccurateValues() ||
         position_y_handler_->HasSampleAccurateValues() ||
         position_z_handler_->HasSampleAccurateValues() ||
         forward_x_handler_->HasSampleAccurateValues() ||
         forward_y_handler_->HasSampleAccurateValues() ||
         forward_z_handler_->HasSampleAccurateValues() ||
         up_x_handler_->HasSampleAccurateValues() ||
         up_y_handler_->HasSampleAccurateValues() ||
         up_z_handler_->HasSampleAccurateValues();
}

bool AudioListenerHandler::IsAudioRate() const {
  return position_x_handler_->IsAudioRate() ||
         position_y_handler_->IsAudioRate() ||
         position_z_handler_->IsAudioRate() ||
         forward_x_handler_->IsAudioRate() ||
         forward_y_handler_->IsAudioRate() ||
         forward_z_handler_->IsAudioRate() ||
         up_x_handler_->IsAudioRate() ||
         up_y_handler_->IsAudioRate() ||
         up_z_handler_->IsAudioRate();
}

void AudioListenerHandler::AddPannerHandler(PannerHandler& panner_handler) {
  DCHECK(IsMainThread());

  panner_handlers_.insert(&panner_handler);
}

void AudioListenerHandler::RemovePannerHandler(PannerHandler& panner_handler) {
  DCHECK(IsMainThread());

  DCHECK(panner_handlers_.Contains(&panner_handler));
  panner_handlers_.erase(&panner_handler);
}

void AudioListenerHandler::MarkPannersAsDirty(unsigned panning_change_type) {
  DCHECK(IsMainThread());

  for (PannerHandler* panner_handler : panner_handlers_) {
    panner_handler->MarkPannerAsDirty(panning_change_type);
  }
}

void AudioListenerHandler::UpdateState() {
  DCHECK(!IsMainThread());

  const base::AutoTryLock try_locker(listener_lock_);
  if (try_locker.is_acquired()) {
    const gfx::Point3F current_position = GetPosition();
    const gfx::Vector3dF current_forward = GetOrientation();
    const gfx::Vector3dF current_up = GetUpVector();

    is_listener_dirty_ = current_position != last_position_ ||
                         current_forward != last_forward_ ||
                         current_up != last_up_;

    if (is_listener_dirty_) {
      last_position_ = current_position;
      last_forward_ = current_forward;
      last_up_ = current_up;
    }
  } else {
    // The main thread must be updating the position, the forward, or the up
    // vector; assume the listener is dirty.  At worst, we'll do a little more
    // work than necessary for one render quantum.
    is_listener_dirty_ = true;
  }
}

void AudioListenerHandler::CreateAndLoadHRTFDatabaseLoader(float sample_rate) {
  DCHECK(IsMainThread());

  if (hrtf_database_loader_) {
    return;
  }

  hrtf_database_loader_ =
      HRTFDatabaseLoader::CreateAndLoadAsynchronouslyIfNecessary(sample_rate);
}

void AudioListenerHandler::WaitForHRTFDatabaseLoaderThreadCompletion() {
  // This can be called from both main and audio threads.

  if (!hrtf_database_loader_) {
    return;
  }

  hrtf_database_loader_->WaitForLoaderThreadCompletion();
}

HRTFDatabaseLoader* AudioListenerHandler::HrtfDatabaseLoader() {
  DCHECK(IsMainThread());

  return hrtf_database_loader_.get();
}

void AudioListenerHandler::UpdateValuesIfNeeded(uint32_t frames_to_process) {
  double current_time = position_x_handler_->DestinationHandler().CurrentTime();

  if (last_update_time_ != current_time) {
    // The time has passed. Update all of the automation values.
    last_update_time_ = current_time;

    DCHECK_LE(frames_to_process, position_x_values_.size());
    DCHECK_LE(frames_to_process, position_y_values_.size());
    DCHECK_LE(frames_to_process, position_z_values_.size());
    DCHECK_LE(frames_to_process, forward_x_values_.size());
    DCHECK_LE(frames_to_process, forward_y_values_.size());
    DCHECK_LE(frames_to_process, forward_z_values_.size());
    DCHECK_LE(frames_to_process, up_x_values_.size());
    DCHECK_LE(frames_to_process, up_y_values_.size());
    DCHECK_LE(frames_to_process, up_z_values_.size());

    position_x_handler_->CalculateSampleAccurateValues(
        position_x_values_.Data(), frames_to_process);
    position_y_handler_->CalculateSampleAccurateValues(
        position_y_values_.Data(), frames_to_process);
    position_z_handler_->CalculateSampleAccurateValues(
        position_z_values_.Data(), frames_to_process);
    forward_x_handler_->CalculateSampleAccurateValues(
        forward_x_values_.Data(), frames_to_process);
    forward_y_handler_->CalculateSampleAccurateValues(
        forward_y_values_.Data(), frames_to_process);
    forward_z_handler_->CalculateSampleAccurateValues(
        forward_z_values_.Data(), frames_to_process);
    up_x_handler_->CalculateSampleAccurateValues(
        up_x_values_.Data(), frames_to_process);
    up_y_handler_->CalculateSampleAccurateValues(
        up_y_values_.Data(), frames_to_process);
    up_z_handler_->CalculateSampleAccurateValues(
        up_z_values_.Data(), frames_to_process);
  }
}

}  // namespace blink
