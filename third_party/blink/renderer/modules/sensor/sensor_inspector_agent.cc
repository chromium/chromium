// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_inspector_agent.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_proxy_inspector_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

SensorInspectorAgent::SensorInspectorAgent(Document* document)
    : provider_(SensorProviderProxy::From(document)) {}

void SensorInspectorAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(provider_);
}

namespace {

void FillQuaternion(double alpha,
                    double beta,
                    double gamma,
                    device::SensorReadingQuat* reading) {
  double half_x_angle = deg2rad(beta) * 0.5;
  double half_y_angle = deg2rad(gamma) * 0.5;
  double half_z_angle = deg2rad(alpha) * 0.5;

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
  Document* current_document = provider_->GetSupplementable();
  Document* new_document = frame->GetDocument();
  if (current_document != new_document) {
    // We need to manually reset |provider_| to drop the strong reference it
    // has to an old document that would otherwise be prevented from being
    // deleted.
    bool inspector_mode = provider_->inspector_mode();
    provider_ = SensorProviderProxy::From(new_document);
    provider_->set_inspector_mode(inspector_mode);
  }
}

void SensorInspectorAgent::SetOrientationSensorOverride(double alpha,
                                                        double beta,
                                                        double gamma) {
  if (!provider_->inspector_mode()) {
    Document* document = provider_->GetSupplementable();
    if (document) {
      ConsoleMessage* console_message = ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kInfo, kInspectorConsoleMessage);
      document->AddConsoleMessage(console_message);
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
