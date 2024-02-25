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
      deferred_task_handler_(&context.GetDeferredTaskHandler()),
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
      forward_x_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerForwardX,
          kDefaultForwardXValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      forward_y_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerForwardY,
          kDefaultForwardYValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      forward_z_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerForwardZ,
          kDefaultForwardZValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      up_x_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerUpX,
          kDefaultUpXValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      up_y_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerUpY,
          kDefaultUpYValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)),
      up_z_(AudioParam::Create(context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioListenerUpZ,
          kDefaultUpZValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(AudioListenerHandler::Create(
      position_x_->Handler(), position_y_->Handler(), position_z_->Handler(),
      forward_x_->Handler(), forward_y_->Handler(), forward_z_->Handler(),
      up_x_->Handler(), up_y_->Handler(), up_z_->Handler(),
      deferred_task_handler_->RenderQuantumFrames()));
}

AudioListener::~AudioListener() {
  // The graph lock is required to destroy the handler because the
  // AudioParamHandlers in `handler_` assumes the lock in its destruction.
  {
    DeferredTaskHandler::GraphAutoLocker locker(*deferred_task_handler_);
    handler_ = nullptr;
  }
}

void AudioListener::setOrientation(float x, float y, float z,
                                   float up_x, float up_y, float up_z,
                                   ExceptionState& exceptionState) {
  SetOrientation(gfx::Vector3dF(x, y, z), exceptionState);
  SetUpVector(gfx::Vector3dF(up_x, up_y, up_z), exceptionState);
}

void AudioListener::setPosition(float x, float y, float z,
                                ExceptionState& exceptionState) {
  SetPosition(gfx::Point3F(x, y, z), exceptionState);
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

void AudioListener::SetHandler(scoped_refptr<AudioListenerHandler> handler) {
  handler_ = std::move(handler);
}


void AudioListener::SetPosition(const gfx::Point3F& position,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  const double now = position_x_->Context()->currentTime();
  position_x_->setValueAtTime(position.x(), now, exceptionState);
  position_y_->setValueAtTime(position.y(), now, exceptionState);
  position_z_->setValueAtTime(position.z(), now, exceptionState);

  const base::AutoLock listener_locker(Handler().Lock());
  Handler().MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty |
                               PannerHandler::kDistanceConeGainDirty);
}

void AudioListener::SetOrientation(const gfx::Vector3dF& orientation,
                                   ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  const double now = forward_x_->Context()->currentTime();
  forward_x_->setValueAtTime(orientation.x(), now, exceptionState);
  forward_y_->setValueAtTime(orientation.y(), now, exceptionState);
  forward_z_->setValueAtTime(orientation.z(), now, exceptionState);

  const base::AutoLock listener_locker(Handler().Lock());
  Handler().MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

void AudioListener::SetUpVector(const gfx::Vector3dF& up_vector,
                                ExceptionState& exceptionState) {
  DCHECK(IsMainThread());

  const double now = up_x_->Context()->currentTime();
  up_x_->setValueAtTime(up_vector.x(), now, exceptionState);
  up_y_->setValueAtTime(up_vector.y(), now, exceptionState);
  up_z_->setValueAtTime(up_vector.z(), now, exceptionState);

  const base::AutoLock listener_locker(Handler().Lock());
  Handler().MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

}  // namespace blink
