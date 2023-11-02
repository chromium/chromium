// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/js_runner.h"

#include "base/supports_user_data.h"
#include "gin/per_context_data.h"

namespace extensions {
namespace {

constexpr char kExtensionJSRunnerPerContextKey[] = "extension_js_runner";

struct JSRunnerPerContextData : public base::SupportsUserData::Data {
  explicit JSRunnerPerContextData(std::unique_ptr<JSRunner> js_runner)
      : js_runner(std::move(js_runner)) {}
  ~JSRunnerPerContextData() override {}

  std::unique_ptr<JSRunner> js_runner;
};

JSRunner* g_instance_for_testing = nullptr;

}  // namespace

// static
JSRunner* JSRunner::Get(v8::Local<v8::Context> context) {
  if (g_instance_for_testing)
    return g_instance_for_testing;

  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  if (!per_context_data)
    return nullptr;
  auto* data = static_cast<JSRunnerPerContextData*>(
      per_context_data->GetUserData(kExtensionJSRunnerPerContextKey));
  if (!data)
    return nullptr;

  return data->js_runner.get();
}

void JSRunner::SetInstanceForContext(v8::Local<v8::Context> context,
                                     std::unique_ptr<JSRunner> js_runner) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  // We should never try to set an instance for a context that's being torn
  // down.
  CHECK(per_context_data);
  // We should never have an existing instance for this context.
  DCHECK(!per_context_data->GetUserData(kExtensionJSRunnerPerContextKey));

  per_context_data->SetUserData(
      kExtensionJSRunnerPerContextKey,
      std::make_unique<JSRunnerPerContextData>(std::move(js_runner)));
}

void JSRunner::ClearInstanceForContext(v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  // We should never try to clear an instance for a context that's being torn
  // down.
  CHECK(per_context_data);
  // We should always have an existing instance for this context.
  DCHECK(per_context_data->GetUserData(kExtensionJSRunnerPerContextKey));

  per_context_data->SetUserData(kExtensionJSRunnerPerContextKey, nullptr);
}

void JSRunner::SetInstanceForTesting(JSRunner* js_runner) {
  g_instance_for_testing = js_runner;
}

JSRunner* JSRunner::GetInstanceForTesting() {
  return g_instance_for_testing;
}

void JSRunner::RunJSFunction(v8::Local<v8::Function> function,
                             v8::Local<v8::Context> context,
                             int argc,
                             v8::Local<v8::Value> argv[]) {
  RunJSFunction(function, context, argc, argv, ResultCallback());
}

}  // namespace extensions
