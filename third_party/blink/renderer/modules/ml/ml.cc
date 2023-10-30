// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "components/ml/webnn/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_context_mojo.h"
#endif

namespace blink {

namespace {

using ml::model_loader::mojom::blink::CreateModelLoaderOptionsPtr;
using ml::model_loader::mojom::blink::MLService;

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

void ML::CreateWebNNContext(
    webnn::mojom::blink::CreateContextOptionsPtr options,
    webnn::mojom::blink::WebNNContextProvider::CreateWebNNContextCallback
        callback) {
  // Connect WebNN Service if needed.
  EnsureWebNNServiceConnection();

  // Create `WebNNGraph` message pipe with `WebNNContext` mojo interface.
  webnn_context_provider_->CreateWebNNContext(std::move(options),
                                              std::move(callback));
}

bool ML::CreateWebNNContextSync(
    webnn::mojom::blink::CreateContextOptionsPtr options,
    webnn::mojom::blink::CreateContextResultPtr* out_result) {
  CHECK(!IsMainThread());
  // Connect to the WebNN Service if needed.
  EnsureWebNNServiceConnection();
  return webnn_context_provider_->CreateWebNNContext(std::move(options),
                                                     out_result);
}

void ML::Trace(Visitor* visitor) const {
  visitor->Trace(model_loader_service_);
  visitor->Trace(webnn_context_provider_);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise ML::createContext(ScriptState* script_state,
                                MLContextOptions* options,
                                ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  auto promise = resolver->Promise();

#if !BUILDFLAG(IS_CHROMEOS)
  if (options->deviceType() == V8MLDeviceType::Enum::kGpu) {
    if (base::FeatureList::IsEnabled(
            webnn::features::kEnableMachineLearningNeuralNetworkService)) {
      MLContextMojo::ValidateAndCreateAsync(resolver, options, this);
    } else {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Not implemented"));
    }
    return promise;
  }
#endif

  // Notice that currently, we just create the context in the renderer. In the
  // future we may add backend query ability to check whether a context is
  // supportable or not. At that time, this function will be truly asynced.
  //
  // TODO(crbug.com/1273291): Support async context creation for all contexts.
  resolver->Resolve(MLContext::ValidateAndCreateSync(options, this));
  return promise;
}

MLContext* ML::createContextSync(ScriptState* script_state,
                                 MLContextOptions* options,
                                 ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // The runtime enable feature is used to disable the cross process hardware
  // acceleration by default.
  if (options->deviceType() == V8MLDeviceType::Enum::kGpu) {
    if (base::FeatureList::IsEnabled(
            webnn::features::kEnableMachineLearningNeuralNetworkService)) {
      return MLContextMojo::ValidateAndCreateSync(script_state, exception_state,
                                                  options, this);
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Not implemented");
      return nullptr;
    }
  }
#endif

  return MLContext::ValidateAndCreateSync(options, this);
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
            GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)));
  }
}

void ML::EnsureWebNNServiceConnection() {
  if (webnn_context_provider_.is_bound()) {
    return;
  }
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      webnn_context_provider_.BindNewPipeAndPassReceiver(
          GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)));
}

}  // namespace blink
