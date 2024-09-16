// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/testing/internals_compute_pressure.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom-blink.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager_automation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_virtual_pressure_source_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

device::mojom::blink::VirtualPressureSourceMetadataPtr ToMojoPressureMetadata(
    CreateVirtualPressureSourceOptions* options) {
  if (!options) {
    return device::mojom::blink::VirtualPressureSourceMetadata::New();
  }

  auto metadata = device::mojom::blink::VirtualPressureSourceMetadata::New();
  metadata->available = options->supported();
  return metadata;
}

device::mojom::blink::PressureSource ToMojoPressureSource(
    V8PressureSource::Enum source) {
  switch (source) {
    case blink::V8PressureSource::Enum::kCpu:
      return device::mojom::blink::PressureSource::kCpu;
  }
}

device::mojom::blink::PressureState ToMojoPressureState(
    V8PressureState::Enum state) {
  switch (state) {
    case blink::V8PressureState::Enum::kNominal:
      return device::mojom::blink::PressureState::kNominal;
    case blink::V8PressureState::Enum::kFair:
      return device::mojom::blink::PressureState::kFair;
    case blink::V8PressureState::Enum::kSerious:
      return device::mojom::blink::PressureState::kSerious;
    case blink::V8PressureState::Enum::kCritical:
      return device::mojom::blink::PressureState::kCritical;
  }
}

ExecutionContext* GetExecutionContext(ScriptState* script_state) {
  // Although this API is available for workers as well, the
  // InternalsComputePressure calls are always made on a Window object via
  // testdriver.js.
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CHECK(window);
  return window;
}

}  // namespace

// static
ScriptPromise<IDLUndefined>
InternalsComputePressure::createVirtualPressureSource(
    ScriptState* script_state,
    Internals&,
    V8PressureSource source,
    CreateVirtualPressureSourceOptions* options) {
  auto* execution_context = GetExecutionContext(script_state);
  mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>
      web_pressure_manager_automation;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      web_pressure_manager_automation.BindNewPipeAndPassReceiver());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  auto* raw_pressure_manager_automation = web_pressure_manager_automation.get();
  raw_pressure_manager_automation->CreateVirtualPressureSource(
      ToMojoPressureSource(source.AsEnum()), ToMojoPressureMetadata(options),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>,
             test::mojom::blink::CreateVirtualPressureSourceResult result) {
            switch (result) {
              case test::mojom::blink::CreateVirtualPressureSourceResult::
                  kSuccess:
                resolver->Resolve();
                break;
              case test::mojom::blink::CreateVirtualPressureSourceResult::
                  kSourceTypeAlreadyOverridden:
                resolver->Reject(
                    "This pressure source type has already been created");
                break;
            }
            resolver->Resolve();
          },
          WrapPersistent(resolver),
          std::move(web_pressure_manager_automation)));
  return promise;
}

// static
ScriptPromise<IDLUndefined>
InternalsComputePressure::removeVirtualPressureSource(ScriptState* script_state,
                                                      Internals&,
                                                      V8PressureSource source) {
  auto* execution_context = GetExecutionContext(script_state);
  mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>
      web_pressure_manager_automation;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      web_pressure_manager_automation.BindNewPipeAndPassReceiver());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  auto* raw_pressure_manager_automation = web_pressure_manager_automation.get();
  raw_pressure_manager_automation->RemoveVirtualPressureSource(
      ToMojoPressureSource(source.AsEnum()),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>) {
            resolver->Resolve();
          },
          WrapPersistent(resolver),
          std::move(web_pressure_manager_automation)));
  return promise;
}

// static
ScriptPromise<IDLUndefined>
InternalsComputePressure::updateVirtualPressureSource(ScriptState* script_state,
                                                      Internals&,
                                                      V8PressureSource source,
                                                      V8PressureState state) {
  auto* execution_context = GetExecutionContext(script_state);
  mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>
      web_pressure_manager_automation;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      web_pressure_manager_automation.BindNewPipeAndPassReceiver());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  auto* raw_pressure_manager_automation = web_pressure_manager_automation.get();
  raw_pressure_manager_automation->UpdateVirtualPressureSourceState(
      ToMojoPressureSource(source.AsEnum()),
      ToMojoPressureState(state.AsEnum()),
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::WebPressureManagerAutomation>,
             test::mojom::UpdateVirtualPressureSourceStateResult result) {
            switch (result) {
              case test::mojom::blink::UpdateVirtualPressureSourceStateResult::
                  kSuccess: {
                resolver->Resolve();
                break;
              }
              case test::mojom::blink::UpdateVirtualPressureSourceStateResult::
                  kSourceTypeNotOverridden:
                resolver->Reject(
                    "A virtual pressure source with this type has not been "
                    "created");
                break;
            }
          },
          WrapPersistent(resolver),
          std::move(web_pressure_manager_automation)));
  return promise;
}

}  // namespace blink
