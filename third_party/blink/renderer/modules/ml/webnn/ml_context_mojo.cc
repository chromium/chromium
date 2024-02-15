// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_context_mojo.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error_mojo.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

using webnn::mojom::blink::PowerPreference;

PowerPreference ConvertBlinkPowerPreferenceToMojo(
    const V8MLPowerPreference& power_preference_blink) {
  switch (power_preference_blink.AsEnum()) {
    case V8MLPowerPreference::Enum::kAuto:
      return PowerPreference::kDefault;
    case V8MLPowerPreference::Enum::kLowPower:
      return PowerPreference::kLowPower;
    case V8MLPowerPreference::Enum::kHighPerformance:
      return PowerPreference::kHighPerformance;
  }
}

}  // namespace

// static
void MLContextMojo::ValidateAndCreate(ScriptPromiseResolver* resolver,
                                      MLContextOptions* options,
                                      ML* ml) {
  ScopedMLTrace scoped_trace("MLContextMojo::ValidateAndCreate");
  CHECK_EQ(options->deviceType(), V8MLDeviceType::Enum::kGpu);
  // TODO(crbug.com/1273291): Remove unsupported options (ex. model_format)
  // once the context gets implemented for non-mojo too.
  auto* context = MakeGarbageCollected<MLContextMojo>(
      options->devicePreference(), options->deviceType(),
      options->powerPreference(), options->modelFormat(), options->numThreads(),
      ml);
  context->Create(std::move(scoped_trace), resolver, options);
}

void MLContextMojo::CreateImpl(ScopedMLTrace scoped_trace,
                               ScriptPromiseResolver* resolver,
                               MLContextOptions* options) {
  auto options_mojo = webnn::mojom::blink::CreateContextOptions::New();
  options_mojo->power_preference =
      ConvertBlinkPowerPreferenceToMojo(options->powerPreference());
  GetML()->CreateWebNNContext(
      std::move(options_mojo),
      WTF::BindOnce(&MLContextMojo::OnCreateWebNNContext, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver)));
}

MLContextMojo::MLContextMojo(const V8MLDevicePreference device_preference,
                             const V8MLDeviceType device_type,
                             const V8MLPowerPreference power_preference,
                             const V8MLModelFormat model_format,
                             const unsigned int num_threads,
                             ML* ml)
    : MLContext(device_preference,
                device_type,
                power_preference,
                model_format,
                num_threads,
                ml),
      remote_context_(ml->GetExecutionContext()) {}

MLContextMojo::~MLContextMojo() = default;

void MLContextMojo::Trace(Visitor* visitor) const {
  visitor->Trace(remote_context_);
  MLContext::Trace(visitor);
}

void MLContextMojo::CreateWebNNGraph(
    webnn::mojom::blink::GraphInfoPtr graph_info,
    webnn::mojom::blink::WebNNContext::CreateGraphCallback callback) {
  CHECK(remote_context_.is_bound());

  // Use `WebNNContext` to create `WebNNGraph` message pipe.
  remote_context_->CreateGraph(std::move(graph_info),
                               WTF::BindOnce(std::move(callback)));
}

void MLContextMojo::OnCreateWebNNContext(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver* resolver,
    blink_mojom::CreateContextResultPtr result) {
  if (result->is_error()) {
    const auto& create_context_error = result->get_error();
    resolver->Reject(MakeGarbageCollected<DOMException>(
        ConvertWebNNErrorCodeToDOMExceptionCode(create_context_error->code),
        create_context_error->message));
    return;
  }

  auto* script_state = resolver->GetScriptState();
  auto* execution_context = ExecutionContext::From(script_state);
  // Bind the end point of `WebNNContext` mojo interface in the blink side.
  remote_context_.Bind(
      std::move(result->get_context_remote()),
      execution_context->GetTaskRunner(TaskType::kInternalDefault));

  resolver->Resolve(this);
}

}  // namespace blink
