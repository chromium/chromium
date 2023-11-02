// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_inspector_agent.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_proxy_inspector_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

SensorInspectorAgent::SensorInspectorAgent(LocalDOMWindow* window)
    : provider_(SensorProviderProxy::From(window)) {}

void SensorInspectorAgent::Trace(Visitor* visitor) const {
  visitor->Trace(provider_);
}

namespace {

void FillQuaternion(double alpha,
                    double beta,
                    double gamma,
                    device::SensorReadingQuat* reading) {
  double half_x_angle = Deg2rad(beta) * 0.5;
  double half_y_angle = Deg2rad(gamma) * 0.5;
  double half_z_angle = Deg2rad(alpha) * 0.5;

  double cos_z = cos(half_z_angle);
  double sin_z = sin(half_z_angle);
  double cos_y = cos(half_y_angle);
  double sin_y = sin(half_y_angle);
  double cos_x = cos(half_x_angle);
  double sin_x = sin(half_x_angle);

  reading->x = sin_x * cos_y * cos_z - cos_x * sin_y * sin_z;
  reading->y = cos_x * sin_y * cos_z + sin_x * cos_y * sin_z;
  reading->z = cos_x * cos_y * sin_z + sin_x * sin_y * cos_z;
  reading->w = cos_x * cos_y * cos_z - sin_x * sin_y * sin_z;
}

void PopulateOrientationReading(double alpha,
                                double beta,
                                double gamma,
                                device::SensorReading* reading) {
  FillQuaternion(alpha, beta, gamma, &reading->orientation_quat);
  reading->orientation_quat.timestamp =
      base::TimeTicks::Now().since_origin().InSecondsF();
}

const char kInspectorConsoleMessage[] =
    "A reload is required so that the existing AbsoluteOrientationSensor and "
    "RelativeOrientationSensor objects on this page use the overridden "
    "values that have been provided. Close the inspector and reload again "
    "to return to the normal behavior.";

}  // namespace

void SensorInspectorAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  LocalDOMWindow* current_window = provider_->GetSupplementable();
  LocalDOMWindow* new_window = frame->DomWindow();
  if (current_window != new_window) {
    // We need to manually reset |provider_| to drop the strong reference it
    // has to an old window that would otherwise be prevented from being
    // deleted.
    bool inspector_mode = provider_->inspector_mode();
    provider_ = SensorProviderProxy::From(new_window);
    provider_->set_inspector_mode(inspector_mode);
  }
}

void SensorInspectorAgent::SetOrientationSensorOverride(double alpha,
                                                        double beta,
                                                        double gamma) {
  if (!provider_->inspector_mode()) {
    if (LocalDOMWindow* window = provider_->GetSupplementable()) {
      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kInfo, kInspectorConsoleMessage);
      window->AddConsoleMessage(console_message);
    }
    provider_->set_inspector_mode(true);
  }

  using device::mojom::blink::SensorType;
  SensorProxy* absolute =
      provider_->GetSensorProxy(SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  SensorProxy* relative =
      provider_->GetSensorProxy(SensorType::RELATIVE_ORIENTATION_QUATERNION);

  if (!absolute && !relative)
    return;

  device::SensorReading reading;
  PopulateOrientationReading(alpha, beta, gamma, &reading);

  if (absolute)
    absolute->SetReadingForInspector(reading);
  if (relative)
    relative->SetReadingForInspector(reading);
}

void SensorInspectorAgent::Disable() {
  provider_->set_inspector_mode(false);
}

}  // namespace blink
