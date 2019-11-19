/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"

#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/panner_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"

namespace blink {

AudioListener::AudioListener(BaseAudioContext& context)
    : InspectorHelperMixin(context.GraphTracer(), context.Uuid()),
      position_x_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionX,
          0.0,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      position_y_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionY,
          0.0,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      position_z_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionZ,
          0.0,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      forward_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardX,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      forward_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardY,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      forward_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardZ,
                             -1.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpX,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpY,
                             1.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpZ,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      last_update_time_(-1),
      is_listener_dirty_(false),
      position_x_values_(audio_utilities::kRenderQuantumFrames),
      position_y_values_(audio_utilities::kRenderQuantumFrames),
      position_z_values_(audio_utilities::kRenderQuantumFrames),
      forward_x_values_(audio_utilities::kRenderQuantumFrames),
      forward_y_values_(audio_utilities::kRenderQuantumFrames),
      forward_z_values_(audio_utilities::kRenderQuantumFrames),
      up_x_values_(audio_utilities::kRenderQuantumFrames),
      up_y_values_(audio_utilities::kRenderQuantumFrames),
      up_z_values_(audio_utilities::kRenderQuantumFrames) {
  // Initialize the cached values with the current values.  Thus, we don't need
  // to notify any panners because we haved moved.
  last_position_ = GetPosition();
  last_forward_ = Orientation();
  last_up_ = UpVector();
}

AudioListener::~AudioListener() = default;

void AudioListener::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_x_);
  visitor->Trace(position_y_);
  visitor->Trace(position_z_);

  visitor->Trace(forward_x_);
  visitor->Trace(forward_y_);
  visitor->Trace(forward_z_);

  visitor->Trace(up_x_);
  visitor->Trace(up_y_);
  visitor->Trace(up_z_);

  InspectorHelperMixin::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void AudioListener::AddPanner(PannerHandler& panner) {
  DCHECK(IsMainThread());
  panners_.insert(&panner);
}

void AudioListener::RemovePanner(PannerHandler& panner) {
  DCHECK(IsMainThread());
  DCHECK(panners_.Contains(&panner));
  panners_.erase(&panner);
}

bool AudioListener::HasSampleAccurateValues() const {
  return positionX()->Handler().HasSampleAccurateValues() ||
         positionY()->Handler().HasSampleAccurateValues() ||
         positionZ()->Handler().HasSampleAccurateValues() ||
         forwardX()->Handler().HasSampleAccurateValues() ||
         forwardY()->Handler().HasSampleAccurateValues() ||
         forwardZ()->Handler().HasSampleAccurateValues() ||
         upX()->Handler().HasSampleAccurateValues() ||
         upY()->Handler().HasSampleAccurateValues() ||
         upZ()->Handler().HasSampleAccurateValues();
}

void AudioListener::UpdateValuesIfNeeded(uint32_t frames_to_process) {
  double current_time =
      positionX()->Handler().DestinationHandler().CurrentTime();
  if (last_update_time_ != current_time) {
    // Time has changed. Update all of the automation values now.
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

    positionX()->Handler().CalculateSampleAccurateValues(
        position_x_values_.Data(), frames_to_process);
    positionY()->Handler().CalculateSampleAccurateValues(
        position_y_values_.Data(), frames_to_process);
    positionZ()->Handler().CalculateSampleAccurateValues(
        position_z_values_.Data(), frames_to_process);

    forwardX()->Handler().CalculateSampleAccurateValues(
        forward_x_values_.Data(), frames_to_process);
    forwardY()->Handler().CalculateSampleAccurateValues(
        forward_y_values_.Data(), frames_to_process);
    forwardZ()->Handler().CalculateSampleAccurateValues(
        forward_z_values_.Data(), frames_to_process);

    upX()->Handler().CalculateSampleAccurateValues(up_x_values_.Data(),
                                                   frames_to_process);
    upY()->Handler().CalculateSampleAccurateValues(up_y_values_.Data(),
                                                   frames_to_process);
    upZ()->Handler().CalculateSampleAccurateValues(up_z_values_.Data(),
                                                   frames_to_process);
  }
}

const float* AudioListener::GetPositionXValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_x_values_.Data();
}

const float* AudioListener::GetPositionYValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_y_values_.Data();
}

const float* AudioListener::GetPositionZValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_z_values_.Data();
}

const float* AudioListener::GetForwardXValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_x_values_.Data();
}

const float* AudioListener::GetForwardYValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_y_values_.Data();
}

const float* AudioListener::GetForwardZValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_z_values_.Data();
}

const float* AudioListener::GetUpXValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_x_values_.Data();
}

const float* AudioListener::GetUpYValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_y_values_.Data();
}

const float* AudioListener::GetUpZValues(uint32_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_z_values_.Data();
}

void AudioListener::UpdateState() {
  // This must be called from the audio thread in pre or post render phase of
  // the graph processing.  (AudioListener doesn't have access to the context
  // to check for the audio thread.)
  DCHECK(!IsMainThread());

  MutexTryLocker try_locker(listener_lock_);
  if (try_locker.Locked()) {
    FloatPoint3D current_position = GetPosition();
    FloatPoint3D current_forward = Orientation();
    FloatPoint3D current_up = UpVector();

    is_listener_dirty_ = current_position != last_position_ ||
                         current_forward != last_forward_ ||
                         current_up != last_up_;

    if (is_listener_dirty_) {
      last_position_ = current_position;
      last_forward_ = current_forward;
      last_up_ = current_up;
    }
  } else {
    // Main thread must be updating the position, forward, or up vector;
    // just assume the listener is dirty.  At worst, we'll do a little more
    // work than necessary for one rendering quantum.
    is_listener_dirty_ = true;
  }
}

void AudioListener::CreateAndLoadHRTFDatabaseLoader(float sample_rate) {
  DCHECK(IsMainThread());

  if (!hrtf_database_loader_)
    hrtf_database_loader_ =
        HRTFDatabaseLoader::CreateAndLoadAsynchronouslyIfNecessary(sample_rate);
}

bool AudioListener::IsHRTFDatabaseLoaded() {
  return hrtf_database_loader_ && hrtf_database_loader_->IsLoaded();
}

void AudioListener::WaitForHRTFDatabaseLoaderThreadCompletion() {
  if (hrtf_database_loader_)
    hrtf_database_loader_->WaitForLoaderThreadCompletion();
}

void AudioListener::MarkPannersAsDirty(unsigned type) {
  DCHECK(IsMainThread());
  for (PannerHandler* panner : panners_)
    panner->MarkPannerAsDirty(type);
}

void AudioListener::setPosition(const FloatPoint3D& position,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);

  double now = position_x_->Context()->currentTime();

  position_x_->setValueAtTime(position.X(), now, exceptionState);
  position_y_->setValueAtTime(position.Y(), now, exceptionState);
  position_z_->setValueAtTime(position.Z(), now, exceptionState);

  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty |
                     PannerHandler::kDistanceConeGainDirty);
}

void AudioListener::setOrientation(const FloatPoint3D& orientation,
                                   ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);

  double now = forward_x_->Context()->currentTime();

  forward_x_->setValueAtTime(orientation.X(), now, exceptionState);
  forward_y_->setValueAtTime(orientation.Y(), now, exceptionState);
  forward_z_->setValueAtTime(orientation.Z(), now, exceptionState);

  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

void AudioListener::SetUpVector(const FloatPoint3D& up_vector,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);

  double now = up_x_->Context()->currentTime();

  up_x_->setValueAtTime(up_vector.X(), now, exceptionState);
  up_y_->setValueAtTime(up_vector.Y(), now, exceptionState);
  up_z_->setValueAtTime(up_vector.Z(), now, exceptionState);

  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

void AudioListener::ReportDidCreate() {
  GraphTracer().DidCreateAudioListener(this);
  GraphTracer().DidCreateAudioParam(position_x_);
  GraphTracer().DidCreateAudioParam(position_y_);
  GraphTracer().DidCreateAudioParam(position_z_);
  GraphTracer().DidCreateAudioParam(forward_x_);
  GraphTracer().DidCreateAudioParam(forward_y_);
  GraphTracer().DidCreateAudioParam(forward_z_);
  GraphTracer().DidCreateAudioParam(up_x_);
  GraphTracer().DidCreateAudioParam(up_y_);
  GraphTracer().DidCreateAudioParam(up_z_);
}

void AudioListener::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(position_x_);
  GraphTracer().WillDestroyAudioParam(position_y_);
  GraphTracer().WillDestroyAudioParam(position_z_);
  GraphTracer().WillDestroyAudioParam(forward_x_);
  GraphTracer().WillDestroyAudioParam(forward_y_);
  GraphTracer().WillDestroyAudioParam(forward_z_);
  GraphTracer().WillDestroyAudioParam(up_x_);
  GraphTracer().WillDestroyAudioParam(up_y_);
  GraphTracer().WillDestroyAudioParam(up_z_);
  GraphTracer().WillDestroyAudioListener(this);
}

}  // namespace blink
