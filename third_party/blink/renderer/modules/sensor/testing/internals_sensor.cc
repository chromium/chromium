// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/testing/internals_sensor.h"

#include <utility>

#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "services/device/public/mojom/sensor_provider.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider_automation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_virtual_sensor_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_virtual_sensor_information.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_virtual_sensor_reading.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_virtual_sensor_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

device::mojom::blink::SensorType ToMojoSensorType(
    V8VirtualSensorType::Enum type) {
  switch (type) {
    case V8VirtualSensorType::Enum::kAbsoluteOrientation:
      return device::mojom::blink::SensorType::ABSOLUTE_ORIENTATION_QUATERNION;
    case V8VirtualSensorType::Enum::kAccelerometer:
      return device::mojom::blink::SensorType::ACCELEROMETER;
    case V8VirtualSensorType::Enum::kAmbientLight:
      return device::mojom::blink::SensorType::AMBIENT_LIGHT;
    case V8VirtualSensorType::Enum::kGravity:
      return device::mojom::blink::SensorType::GRAVITY;
    case V8VirtualSensorType::Enum::kGyroscope:
      return device::mojom::blink::SensorType::GYROSCOPE;
    case V8VirtualSensorType::Enum::kLinearAcceleration:
      return device::mojom::blink::SensorType::LINEAR_ACCELERATION;
    case V8VirtualSensorType::Enum::kMagnetometer:
      return device::mojom::blink::SensorType::MAGNETOMETER;
    case V8VirtualSensorType::Enum::kProximity:
      return device::mojom::blink::SensorType::PROXIMITY;
    case V8VirtualSensorType::Enum::kRelativeOrientation:
      return device::mojom::blink::SensorType::RELATIVE_ORIENTATION_QUATERNION;
  }
}

device::mojom::blink::VirtualSensorMetadataPtr ToMojoSensorMetadata(
    CreateVirtualSensorOptions* options) {
  if (!options) {
    return device::mojom::blink::VirtualSensorMetadata::New();
  }

  auto metadata = device::mojom::blink::VirtualSensorMetadata::New();
  metadata->available = options->connected();
  if (options->hasMinSamplingFrequency()) {
    metadata->minimum_frequency = options->minSamplingFrequency().value();
  }
  if (options->hasMaxSamplingFrequency()) {
    metadata->maximum_frequency = options->maxSamplingFrequency().value();
  }
  return metadata;
}

base::expected<device::mojom::blink::SensorReadingRawPtr, String>
ToMojoRawReading(V8VirtualSensorType::Enum type,
                 VirtualSensorReading* reading) {
  if (!reading) {
    return device::mojom::blink::SensorReadingRaw::New();
  }

  // TODO(crbug.com/1492436): with the right Blink Mojo traits, we could use
  // device::SensorReading instead of device::mojom::blink::SensorReadingRaw.
  auto raw_reading = device::mojom::blink::SensorReadingRaw::New();
  raw_reading->timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  raw_reading->values.Fill(0.0, 4);
  switch (type) {
    case V8VirtualSensorType::Enum::kAbsoluteOrientation:
    case V8VirtualSensorType::Enum::kRelativeOrientation: {
      if (!reading->hasQuaternion()) {
        return base::unexpected("Invalid quaternion reading format");
      }
      constexpr size_t kQuaternionSize = 4;
      Vector<double> quaternion = reading->getQuaternionOr(Vector<double>());
      if (quaternion.size() != kQuaternionSize) {
        return base::unexpected(
            "'quaternion' does not have the right number of elements");
      }
      raw_reading->values.swap(quaternion);
      break;
    }
    case V8VirtualSensorType::Enum::kAmbientLight:
      if (!reading->hasIlluminance()) {
        return base::unexpected("Invalid illuminance reading format");
      }
      raw_reading->values[0] = reading->getIlluminanceOr(0);
      break;
    case V8VirtualSensorType::Enum::kAccelerometer:
    case V8VirtualSensorType::Enum::kGravity:
    case V8VirtualSensorType::Enum::kGyroscope:
    case V8VirtualSensorType::Enum::kLinearAcceleration:
    case V8VirtualSensorType::Enum::kMagnetometer:
    case V8VirtualSensorType::Enum::kProximity:
      if (!reading->hasX() || !reading->hasY() || !reading->hasZ()) {
        return base::unexpected("Invalid xyz reading format");
      }
      raw_reading->values[0] = reading->getXOr(0);
      raw_reading->values[1] = reading->getYOr(0);
      raw_reading->values[2] = reading->getZOr(0);
      break;
  }
  return raw_reading;
}

}  // namespace

// static
ScriptPromise InternalsSensor::createVirtualSensor(
    ScriptState* script_state,
    Internals&,
    V8VirtualSensorType type,
    CreateVirtualSensorOptions* options) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>
      virtual_sensor_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      virtual_sensor_provider.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* raw_virtual_sensor_provider = virtual_sensor_provider.get();
  raw_virtual_sensor_provider->CreateVirtualSensor(
      ToMojoSensorType(type.AsEnum()), ToMojoSensorMetadata(options),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>,
             device::mojom::blink::CreateVirtualSensorResult result) {
            switch (result) {
              case device::mojom::blink::CreateVirtualSensorResult::kSuccess:
                resolver->Resolve();
                break;
              case device::mojom::blink::CreateVirtualSensorResult::
                  kSensorTypeAlreadyOverridden:
                resolver->Reject("This sensor type has already been created");
                break;
            }
          },
          WrapPersistent(resolver), std::move(virtual_sensor_provider)));
  return promise;
}

// static
ScriptPromise InternalsSensor::updateVirtualSensor(
    ScriptState* script_state,
    Internals&,
    V8VirtualSensorType type,
    VirtualSensorReading* reading) {
  auto mojo_reading = ToMojoRawReading(type.AsEnum(), reading);
  if (!mojo_reading.has_value()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowDOMException::CreateOrEmpty(script_state->GetIsolate(),
                                           DOMExceptionCode::kInvalidStateError,
                                           mojo_reading.error()));
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>
      virtual_sensor_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      virtual_sensor_provider.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* raw_virtual_sensor_provider = virtual_sensor_provider.get();
  raw_virtual_sensor_provider->UpdateVirtualSensor(
      ToMojoSensorType(type.AsEnum()), std::move(mojo_reading.value()),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>,
             device::mojom::blink::UpdateVirtualSensorResult result) {
            switch (result) {
              case device::mojom::blink::UpdateVirtualSensorResult::kSuccess: {
                resolver->Resolve();
                break;
              }
              case device::mojom::blink::UpdateVirtualSensorResult::
                  kSensorTypeNotOverridden:
                resolver->Reject(
                    "A virtual sensor with this type has not been created");
                break;
            }
          },
          WrapPersistent(resolver), std::move(virtual_sensor_provider)));
  return promise;
}

// static
ScriptPromise InternalsSensor::removeVirtualSensor(ScriptState* script_state,
                                                   Internals&,
                                                   V8VirtualSensorType type) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>
      virtual_sensor_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      virtual_sensor_provider.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* raw_virtual_sensor_provider = virtual_sensor_provider.get();
  raw_virtual_sensor_provider->RemoveVirtualSensor(
      ToMojoSensorType(type.AsEnum()),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>) {
            resolver->Resolve();
          },
          WrapPersistent(resolver), std::move(virtual_sensor_provider)));
  return promise;
}

// static
ScriptPromise InternalsSensor::getVirtualSensorInformation(
    ScriptState* script_state,
    Internals&,
    V8VirtualSensorType type) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>
      virtual_sensor_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      virtual_sensor_provider.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* raw_virtual_sensor_provider = virtual_sensor_provider.get();
  raw_virtual_sensor_provider->GetVirtualSensorInformation(
      ToMojoSensorType(type.AsEnum()),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<test::mojom::blink::WebSensorProviderAutomation>,
             device::mojom::blink::GetVirtualSensorInformationResultPtr
                 result) {
            if (result->is_error()) {
              switch (result->get_error()) {
                case device::mojom::blink::GetVirtualSensorInformationError::
                    kSensorTypeNotOverridden:
                  resolver->Reject(
                      "A virtual sensor with this type has not been created");
                  return;
              }
            }
            CHECK(result->is_info());
            auto* sensor_info = VirtualSensorInformation::Create();
            sensor_info->setRequestedSamplingFrequency(
                result->get_info()->sampling_frequency);
            resolver->Resolve(sensor_info);
          },
          WrapPersistent(resolver), std::move(virtual_sensor_provider)));
  return promise;
}

}  // namespace blink
