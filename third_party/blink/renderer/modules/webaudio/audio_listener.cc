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

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/panner_handler.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"

namespace blink {

namespace {

constexpr double kDefaultPositionXValue = 0.0;
constexpr double kDefaultPositionYValue = 0.0;
constexpr double kDefaultPositionZValue = 0.0;
constexpr double kDefaultForwardXValue = 0.0;
constexpr double kDefaultForwardYValue = 0.0;
constexpr double kDefaultForwardZValue = -1.0;
constexpr double kDefaultUpXValue = 0.0;
constexpr double kDefaultUpYValue = 1.0;
constexpr double kDefaultUpZValue = 0.0;

}  // namespace

AudioListener::AudioListener(BaseAudioContext& context)
    : InspectorHelperMixin(context.GraphTracer(), context.Uuid()),
      position_x_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionX,
          kDefaultPositionXValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      position_y_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionY,
          kDefaultPositionYValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      position_z_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerPositionZ,
          kDefaultPositionZValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      forward_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardX,
                             kDefaultForwardXValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      forward_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardY,
                             kDefaultForwardYValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      forward_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerForwardZ,
                             kDefaultForwardZValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpX,
                             kDefaultUpXValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpY,
                             kDefaultUpYValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      up_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeAudioListenerUpZ,
                             kDefaultUpZValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      position_x_values_(
          context.GetDeferredTaskHandler().RenderQuantumFrames()),
      position_y_values_(
          context.GetDeferredTaskHandler().RenderQuantumFrames()),
      position_z_values_(
          context.GetDeferredTaskHandler().RenderQuantumFrames()),
      forward_x_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()),
      forward_y_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()),
      forward_z_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()),
      up_x_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()),
      up_y_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()),
      up_z_values_(context.GetDeferredTaskHandler().RenderQuantumFrames()) {
  // Initialize the cached values with the current values.  Thus, we don't need
  // to notify any panners because we haved moved.
  last_position_ = GetPosition();
  last_forward_ = GetOrientation();
  last_up_ = GetUpVector();
}

void AudioListener::Trace(Visitor* visitor) const {
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

void AudioListener::AddPannerHandler(PannerHandler& panner_handler) {
  DCHECK(IsMainThread());

  panner_handlers_.insert(&panner_handler);
}

void AudioListener::RemovePannerHandler(PannerHandler& panner_handler) {
  DCHECK(IsMainThread());

  DCHECK(panner_handlers_.Contains(&panner_handler));
  panner_handlers_.erase(&panner_handler);
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

bool AudioListener::IsAudioRate() const {
  return positionX()->Handler().IsAudioRate() ||
         positionY()->Handler().IsAudioRate() ||
         positionZ()->Handler().IsAudioRate() ||
         forwardX()->Handler().IsAudioRate() ||
         forwardY()->Handler().IsAudioRate() ||
         forwardZ()->Handler().IsAudioRate() ||
         upX()->Handler().IsAudioRate() ||
         upY()->Handler().IsAudioRate() ||
         upZ()->Handler().IsAudioRate();
}

void AudioListener::UpdateValuesIfNeeded(uint32_t frames_to_process) {
  double current_time =
      positionX()->Handler().DestinationHandler().CurrentTime();
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
    upX()->Handler().CalculateSampleAccurateValues(
        up_x_values_.Data(), frames_to_process);
    upY()->Handler().CalculateSampleAccurateValues(
        up_y_values_.Data(), frames_to_process);
    upZ()->Handler().CalculateSampleAccurateValues(
        up_z_values_.Data(), frames_to_process);
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

void AudioListener::CreateAndLoadHRTFDatabaseLoader(float sample_rate) {
  DCHECK(IsMainThread());

  if (!hrtf_database_loader_) {
    hrtf_database_loader_ =
        HRTFDatabaseLoader::CreateAndLoadAsynchronouslyIfNecessary(sample_rate);
  }
}

void AudioListener::WaitForHRTFDatabaseLoaderThreadCompletion() {
  if (hrtf_database_loader_) {
    hrtf_database_loader_->WaitForLoaderThreadCompletion();
  }
}

void AudioListener::MarkPannersAsDirty(unsigned type) {
  DCHECK(IsMainThread());

  for (PannerHandler* panner_handler : panner_handlers_) {
    panner_handler->MarkPannerAsDirty(type);
  }
}

void AudioListener::SetPosition(const gfx::Point3F& position,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  const base::AutoLock listener_locker(listener_lock_);

  const double now = position_x_->Context()->currentTime();

  position_x_->setValueAtTime(position.x(), now, exceptionState);
  position_y_->setValueAtTime(position.y(), now, exceptionState);
  position_z_->setValueAtTime(position.z(), now, exceptionState);

  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty |
                     PannerHandler::kDistanceConeGainDirty);
}

void AudioListener::SetOrientation(const gfx::Vector3dF& orientation,
                                   ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  const base::AutoLock listener_locker(listener_lock_);

  const double now = forward_x_->Context()->currentTime();

  forward_x_->setValueAtTime(orientation.x(), now, exceptionState);
  forward_y_->setValueAtTime(orientation.y(), now, exceptionState);
  forward_z_->setValueAtTime(orientation.z(), now, exceptionState);

  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

void AudioListener::SetUpVector(const gfx::Vector3dF& up_vector,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  const base::AutoLock listener_locker(listener_lock_);

  const double now = up_x_->Context()->currentTime();

  up_x_->setValueAtTime(up_vector.x(), now, exceptionState);
  up_y_->setValueAtTime(up_vector.y(), now, exceptionState);
  up_z_->setValueAtTime(up_vector.z(), now, exceptionState);

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
