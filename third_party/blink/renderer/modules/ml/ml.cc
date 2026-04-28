// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink-forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

#if BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
#include "services/webnn/public/cpp/in_process_context_provider.h"
#endif  // BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)

namespace blink {

namespace {

webnn::mojom::blink::Device ConvertBlinkDeviceTypeToMojo(
    const V8MLDeviceType& device_type_blink) {
  switch (device_type_blink.AsEnum()) {
    case V8MLDeviceType::Enum::kCpu:
      return webnn::mojom::blink::Device::kCpu;
    case V8MLDeviceType::Enum::kGpu:
      return webnn::mojom::blink::Device::kGpu;
    case V8MLDeviceType::Enum::kNpu:
      return webnn::mojom::blink::Device::kNpu;
  }
}

webnn::mojom::blink::CreateContextOptions::PowerPreference
ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kDefault:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kDefault;
    case V8MLPowerPreference::Enum::kLowPower:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kLowPower;
    case V8MLPowerPreference::Enum::kHighPerformance:
      return webnn::mojom::blink::CreateContextOptions::PowerPreference::
          kHighPerformance;
  }
}

}  // namespace

ML::ML(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
#if BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
      tflite_context_provider_(execution_context),
#endif  // BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
      webnn_context_provider_(execution_context) {
}

void ML::Trace(Visitor* visitor) const {
  visitor->Trace(webnn_context_provider_);
#if BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
  visitor->Trace(tflite_context_provider_);
  visitor->Trace(tflite_pending_resolvers_);
#endif  // BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
  visitor->Trace(pending_resolvers_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLContext> ML::createContext(ScriptState* script_state,
                                           MLContextOptions* options,
                                           ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("ML::createContext(MLContextOptions)");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLContext>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // Ensure `resolver` is rejected if the `CreateWebNNContext()` callback isn't
  // run due to a WebNN service connection error.
  pending_resolvers_.insert(resolver);

  EnsureWebNNServiceConnection();

  webnn_context_provider_->CreateWebNNContext(
      webnn::mojom::blink::CreateContextOptions::New(
          ConvertBlinkDeviceTypeToMojo(options->deviceType()),
          ConvertBlinkPowerPreferenceToMojo(options->powerPreference())),
      BindOnce(
          [](ML* ml, ScriptPromiseResolver<MLContext>* resolver,
             MLContextOptions* options, webnn::ScopedTrace scoped_trace,
             webnn::mojom::blink::CreateContextResultPtr result) {
            ml->pending_resolvers_.erase(resolver);

            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }

            if (result->is_error()) {
#if BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
              // Fallback to in-process TFLite CPU backend when the GPU
              // process WebNN backend fails.
              ml->CreateInProcessTFLiteContext(resolver, options,
                                               std::move(scoped_trace));
              return;
#else
              const webnn::mojom::blink::Error& create_context_error =
                  *result->get_error();
              resolver->RejectWithDOMException(
                  WebNNErrorCodeToDOMExceptionCode(create_context_error.code),
                  create_context_error.message);
              return;
#endif  // BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
            }

            resolver->Resolve(MakeGarbageCollected<MLContext>(
                context, options->deviceType(), options->powerPreference(),
                std::move(result->get_success())));
          },
          WrapPersistent(this), WrapPersistent(resolver),
          WrapPersistent(options), std::move(scoped_trace)));

  return promise;
}

void ML::OnWebNNServiceConnectionError() {
  webnn_context_provider_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                     "WebNN service connection error.");
  }
  pending_resolvers_.clear();
}

void ML::EnsureWebNNServiceConnection() {
  if (webnn_context_provider_.is_bound()) {
    return;
  }
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      webnn_context_provider_.BindNewPipeAndPassReceiver(
          GetExecutionContext()->GetTaskRunner(TaskType::kMachineLearning)));
  // Bind should always succeed because ml.idl is gated on the same feature flag
  // as `WebNNContextProvider`.
  CHECK(webnn_context_provider_.is_bound());
  webnn_context_provider_.set_disconnect_handler(
      BindOnce(&ML::OnWebNNServiceConnectionError, WrapWeakPersistent(this)));
}

#if BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)
void ML::CreateInProcessTFLiteContext(
    ScriptPromiseResolver<MLContext>* resolver,
    MLContextOptions* options,
    webnn::ScopedTrace scoped_trace) {
  EnsureInProcessTFLiteConnection();

  // Track this resolver in the TFLite-specific set so that only a TFLite
  // disconnect (not a GPU-process disconnect) can reject it.
  tflite_pending_resolvers_.insert(resolver);

  // The tflite_context_provider_ remote uses the blink Mojo variant
  // (connected via cross-variant pipe to the non-blink receiver), so
  // we can use the same callback pattern as the GPU process path.
  tflite_context_provider_->CreateWebNNContext(
      webnn::mojom::blink::CreateContextOptions::New(
          ConvertBlinkDeviceTypeToMojo(options->deviceType()),
          ConvertBlinkPowerPreferenceToMojo(options->powerPreference())),
      BindOnce(
          [](ML* ml, ScriptPromiseResolver<MLContext>* resolver,
             MLContextOptions* options, webnn::ScopedTrace scoped_trace,
             webnn::mojom::blink::CreateContextResultPtr result) {
            ml->tflite_pending_resolvers_.erase(resolver);

            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }

            if (result->is_error()) {
              const webnn::mojom::blink::Error& create_context_error =
                  *result->get_error();
              resolver->RejectWithDOMException(
                  WebNNErrorCodeToDOMExceptionCode(create_context_error.code),
                  create_context_error.message);
              return;
            }

            resolver->Resolve(MakeGarbageCollected<MLContext>(
                context, options->deviceType(), options->powerPreference(),
                std::move(result->get_success())));
          },
          WrapPersistent(this), WrapPersistent(resolver),
          WrapPersistent(options), std::move(scoped_trace)));
}

void ML::EnsureInProcessTFLiteConnection() {
  if (tflite_context_provider_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMachineLearning);

  // Create the in-process TFLite context provider via the thin factory.
  // The factory returns a raw pipe handle for a WebNNContextProvider remote.
  // We wrap it into a blink-variant PendingRemote — this works because blink
  // and non-blink Mojo variants use the same wire format.
  mojo::ScopedMessagePipeHandle context_provider_pipe =
      webnn::tflite::CreateInProcessContextProvider(task_runner);
  tflite_context_provider_.Bind(
      mojo::PendingRemote<webnn::mojom::blink::WebNNContextProvider>(
          std::move(context_provider_pipe), 0u),
      task_runner);
  CHECK(tflite_context_provider_.is_bound());
  tflite_context_provider_.set_disconnect_handler(
      BindOnce(&ML::OnTFLiteServiceConnectionError, WrapWeakPersistent(this)));
}

void ML::OnTFLiteServiceConnectionError() {
  tflite_context_provider_.reset();
  for (const auto& resolver : tflite_pending_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kUnknownError,
        "In-process TFLite service connection error.");
  }
  tflite_pending_resolvers_.clear();
}
#endif  // BUILDFLAG(WEBNN_TFLITE_IN_RENDERER)

}  // namespace blink
