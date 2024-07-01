// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

using ml::model_loader::mojom::blink::CreateModelLoaderOptionsPtr;
using ml::model_loader::mojom::blink::MLService;

webnn::mojom::blink::CreateContextOptions::Device ConvertBlinkDeviceTypeToMojo(
    const V8MLDeviceType& device_type_blink) {
  switch (device_type_blink.AsEnum()) {
    case V8MLDeviceType::Enum::kCpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kCpu;
    case V8MLDeviceType::Enum::kGpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kGpu;
    case V8MLDeviceType::Enum::kNpu:
      return webnn::mojom::blink::CreateContextOptions::Device::kNpu;
  }
}

webnn::mojom::blink::CreateContextOptions::PowerPreference
ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kAuto:
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
      model_loader_service_(execution_context),
      webnn_context_provider_(execution_context) {}

void ML::CreateModelLoader(ScriptState* script_state,
                           CreateModelLoaderOptionsPtr options,
                           MLService::CreateModelLoaderCallback callback) {
  EnsureModelLoaderServiceConnection(script_state);

  model_loader_service_->CreateModelLoader(std::move(options),
                                           std::move(callback));
}

void ML::Trace(Visitor* visitor) const {
  visitor->Trace(model_loader_service_);
  visitor->Trace(webnn_context_provider_);
  visitor->Trace(pending_resolvers_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLContext> ML::createContext(ScriptState* script_state,
                                           MLContextOptions* options,
                                           ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("ML::createContext");
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
          ConvertBlinkPowerPreferenceToMojo(options->powerPreference()),
          options->numThreads()),
      WTF::BindOnce(
          [](ML* ml, ScriptPromiseResolver<MLContext>* resolver,
             MLContextOptions* options,
             webnn::mojom::blink::CreateContextResultPtr result) {
            ml->pending_resolvers_.erase(resolver);

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
                options->devicePreference(), options->deviceType(),
                options->powerPreference(), options->modelFormat(),
                options->numThreads(), ml, std::move(result->get_success())));
          },
          WrapPersistent(this), WrapPersistent(resolver),
          WrapPersistent(options)));

  return promise;
}

void ML::EnsureModelLoaderServiceConnection(ScriptState* script_state) {
  // The execution context of this navigator is valid here because it has been
  // verified at the beginning of `MLModelLoader::load()` function.
  CHECK(script_state->ContextIsValid());

  // Note that we do not use `ExecutionContext::From(script_state)` because
  // the ScriptState passed in may not be guaranteed to match the execution
  // context associated with this navigator, especially with
  // cross-browsing-context calls.
  if (!model_loader_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        model_loader_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMachineLearning)));
  }
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
  webnn_context_provider_.set_disconnect_handler(WTF::BindOnce(
      &ML::OnWebNNServiceConnectionError, WrapWeakPersistent(this)));
}

}  // namespace blink
