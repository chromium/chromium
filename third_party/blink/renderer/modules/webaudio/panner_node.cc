/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/panner_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_panner_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/panner_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr double kDefaultPositionXValue = 0.0;
constexpr double kDefaultPositionYValue = 0.0;
constexpr double kDefaultPositionZValue = 0.0;
constexpr double kDefaultOrientationXValue = 1.0;
constexpr double kDefaultOrientationYValue = 0.0;
constexpr double kDefaultOrientationZValue = 0.0;

}  // namespace

PannerNode::PannerNode(BaseAudioContext& context)
    : AudioNode(context),
      position_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerPositionX,
                             kDefaultPositionXValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      position_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerPositionY,
                             kDefaultPositionYValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      position_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerPositionZ,
                             kDefaultPositionZValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_x_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerOrientationX,
                             kDefaultOrientationXValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_y_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerOrientationY,
                             kDefaultOrientationYValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_z_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypePannerOrientationZ,
                             kDefaultOrientationZValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      listener_(context.listener()) {
  SetHandler(PannerHandler::Create(
      *this, context.sampleRate(), position_x_->Handler(),
      position_y_->Handler(), position_z_->Handler(), orientation_x_->Handler(),
      orientation_y_->Handler(), orientation_z_->Handler()));
}

PannerNode* PannerNode::Create(BaseAudioContext& context,
                               ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<PannerNode>(context);
}

PannerNode* PannerNode::Create(BaseAudioContext* context,
                               const PannerOptions* options,
                               ExceptionState& exception_state) {
  PannerNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->setPanningModel(options->panningModel());
  node->setDistanceModel(options->distanceModel());

  node->positionX()->setValue(options->positionX());
  node->positionY()->setValue(options->positionY());
  node->positionZ()->setValue(options->positionZ());

  node->orientationX()->setValue(options->orientationX());
  node->orientationY()->setValue(options->orientationY());
  node->orientationZ()->setValue(options->orientationZ());

  node->setRefDistance(options->refDistance(), exception_state);
  node->setMaxDistance(options->maxDistance(), exception_state);
  node->setRolloffFactor(options->rolloffFactor(), exception_state);
  node->setConeInnerAngle(options->coneInnerAngle());
  node->setConeOuterAngle(options->coneOuterAngle());
  node->setConeOuterGain(options->coneOuterGain(), exception_state);

  return node;
}

PannerHandler& PannerNode::GetPannerHandler() const {
  return static_cast<PannerHandler&>(Handler());
}

V8PanningModelType PannerNode::panningModel() const {
  return V8PanningModelType(GetPannerHandler().PanningModel());
}

void PannerNode::setPanningModel(const V8PanningModelType& model) {
  GetPannerHandler().SetPanningModel(model.AsEnum());
}

void PannerNode::setPosition(float x,
                             float y,
                             float z,
                             ExceptionState& exceptionState) {
  GetPannerHandler().SetPosition(x, y, z, exceptionState);
}

void PannerNode::setOrientation(float x,
                                float y,
                                float z,
                                ExceptionState& exceptionState) {
  GetPannerHandler().SetOrientation(x, y, z, exceptionState);
}

V8DistanceModelType PannerNode::distanceModel() const {
  return V8DistanceModelType(GetPannerHandler().DistanceModel());
}

void PannerNode::setDistanceModel(const V8DistanceModelType& model) {
  GetPannerHandler().SetDistanceModel(model.AsEnum());
}

double PannerNode::refDistance() const {
  return GetPannerHandler().RefDistance();
}

void PannerNode::setRefDistance(double distance,
                                ExceptionState& exception_state) {
  if (distance < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("refDistance",
                                                            distance, 0));
    return;
  }

  GetPannerHandler().SetRefDistance(distance);
}

double PannerNode::maxDistance() const {
  return GetPannerHandler().MaxDistance();
}

void PannerNode::setMaxDistance(double distance,
                                ExceptionState& exception_state) {
  if (distance <= 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("maxDistance",
                                                            distance, 0));
    return;
  }

  GetPannerHandler().SetMaxDistance(distance);
}

double PannerNode::rolloffFactor() const {
  return GetPannerHandler().RolloffFactor();
}

void PannerNode::setRolloffFactor(double factor,
                                  ExceptionState& exception_state) {
  if (factor < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("rolloffFactor",
                                                            factor, 0));
    return;
  }

  GetPannerHandler().SetRolloffFactor(factor);
}

double PannerNode::coneInnerAngle() const {
  return GetPannerHandler().ConeInnerAngle();
}

void PannerNode::setConeInnerAngle(double angle) {
  GetPannerHandler().SetConeInnerAngle(angle);
}

double PannerNode::coneOuterAngle() const {
  return GetPannerHandler().ConeOuterAngle();
}

void PannerNode::setConeOuterAngle(double angle) {
  GetPannerHandler().SetConeOuterAngle(angle);
}

double PannerNode::coneOuterGain() const {
  return GetPannerHandler().ConeOuterGain();
}

void PannerNode::setConeOuterGain(double gain,
                                  ExceptionState& exception_state) {
  if (gain < 0 || gain > 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        ExceptionMessages::IndexOutsideRange<double>(
            "coneOuterGain", gain, 0, ExceptionMessages::kInclusiveBound, 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  GetPannerHandler().SetConeOuterGain(gain);
}

void PannerNode::Trace(Visitor* visitor) const {
  visitor->Trace(position_x_);
  visitor->Trace(position_y_);
  visitor->Trace(position_z_);
  visitor->Trace(orientation_x_);
  visitor->Trace(orientation_y_);
  visitor->Trace(orientation_z_);
  visitor->Trace(listener_);
  AudioNode::Trace(visitor);
}

void PannerNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(position_x_);
  GraphTracer().DidCreateAudioParam(position_y_);
  GraphTracer().DidCreateAudioParam(position_z_);
  GraphTracer().DidCreateAudioParam(orientation_x_);
  GraphTracer().DidCreateAudioParam(orientation_y_);
  GraphTracer().DidCreateAudioParam(orientation_z_);
}

void PannerNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(position_x_);
  GraphTracer().WillDestroyAudioParam(position_y_);
  GraphTracer().WillDestroyAudioParam(position_z_);
  GraphTracer().WillDestroyAudioParam(orientation_x_);
  GraphTracer().WillDestroyAudioParam(orientation_y_);
  GraphTracer().WillDestroyAudioParam(orientation_z_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
