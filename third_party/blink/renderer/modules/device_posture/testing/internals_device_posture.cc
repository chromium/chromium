// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_posture/testing/internals_device_posture.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider_automation.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

blink::mojom::DevicePostureType ToMojoDevicePostureType(
    V8DevicePostureType::Enum posture) {
  switch (posture) {
    case blink::V8DevicePostureType::Enum::kContinuous:
      return blink::mojom::DevicePostureType::kContinuous;
    case blink::V8DevicePostureType::Enum::kFolded:
      return blink::mojom::DevicePostureType::kFolded;
  }
}

}  // namespace

ScriptPromise<IDLUndefined> InternalsDevicePosture::setDevicePostureOverride(
    ScriptState* script_state,
    Internals&,
    V8DevicePostureType posture) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::DevicePostureProviderAutomation>
      device_posture_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      device_posture_provider.BindNewPipeAndPassReceiver());
  device_posture_provider->SetPostureOverride(
      ToMojoDevicePostureType(posture.AsEnum()));

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> InternalsDevicePosture::clearDevicePostureOverride(
    ScriptState* script_state,
    Internals&) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  mojo::Remote<test::mojom::blink::DevicePostureProviderAutomation>
      device_posture_provider;
  window->GetBrowserInterfaceBroker().GetInterface(
      device_posture_provider.BindNewPipeAndPassReceiver());
  device_posture_provider->ClearPostureOverride();

  return ToResolvedUndefinedPromise(script_state);
}

}  // namespace blink
