// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_inspector_agent.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"

namespace blink {

namespace {

constexpr char kInspectorConsoleMessage[] =
    "A reload is required so that the existing AbsoluteOrientationSensor "
    "and RelativeOrientationSensor objects on this page use the overridden "
    "values that have been provided. To return to the normal behavior, you can "
    "either close the inspector or disable the orientation override, and then "
    "reload.";

}  // namespace

DeviceOrientationInspectorAgent::~DeviceOrientationInspectorAgent() = default;

DeviceOrientationInspectorAgent::DeviceOrientationInspectorAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      enabled_(&agent_state_, /*default_value=*/false) {}

void DeviceOrientationInspectorAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

DeviceOrientationController& DeviceOrientationInspectorAgent::Controller() {
  return DeviceOrientationController::From(
      *inspected_frames_->Root()->DomWindow());
}

protocol::Response
DeviceOrientationInspectorAgent::setDeviceOrientationOverride(double alpha,
                                                              double beta,
                                                              double gamma) {
  if (!enabled_.Get()) {
    Controller().RestartPumpIfNeeded();

    // If the device orientation override is switching to being enabled, warn
    // about the effect it has on existing AbsoluteOrientationSensor and
    // RelativeOrientationSensor instances.
    inspected_frames_->Root()->DomWindow()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kInfo,
            kInspectorConsoleMessage));
  }
  enabled_.Set(true);
  return protocol::Response::Success();
}

protocol::Response
DeviceOrientationInspectorAgent::clearDeviceOrientationOverride() {
  return disable();
}

protocol::Response DeviceOrientationInspectorAgent::disable() {
  agent_state_.ClearAllFields();
  if (!inspected_frames_->Root()->DomWindow()->IsContextDestroyed()) {
    Controller().RestartPumpIfNeeded();
  }
  return protocol::Response::Success();
}

void DeviceOrientationInspectorAgent::Restore() {
  if (!enabled_.Get()) {
    return;
  }
  Controller().RestartPumpIfNeeded();
}

}  // namespace blink
