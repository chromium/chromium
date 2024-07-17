// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_v8_features.h"

#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

v8::Isolate::Priority ToIsolatePriority(base::Process::Priority priority) {
  switch (priority) {
    case base::Process::Priority::kBestEffort:
      return v8::Isolate::Priority::kBestEffort;
    case base::Process::Priority::kUserVisible:
      return v8::Isolate::Priority::kUserVisible;
    case base::Process::Priority::kUserBlocking:
      return v8::Isolate::Priority::kUserBlocking;
  }
}

}  // namespace

// static
void WebV8Features::EnableMojoJS(v8::Local<v8::Context> context, bool enable) {
  if (enable) {
    // If the code is trying to enable mojo JS but mojo JS is not allowed for
    // the process, as determined by the protected memory bool value, then it
    // indicates the code ended up here as a result of a malicious attack. As a
    // result we want to crash the process.
    // (crbug.com/976506)
    ContextFeatureSettings::CrashIfMojoJSNotAllowed();
  }
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnableMojoJS(enable);
}

// static
void WebV8Features::EnableMojoJSAndUseBroker(
    v8::Local<v8::Context> context,
    CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>
        broker_remote) {
  // This code depends on |ContextFeatureSettings::CrashIfMojoJSNotAllowed|
  // through |EnableMojoJS|. If the code is trying to enable mojo JS but mojo JS
  // is not allowed for the process, as determined by the protected memory bool
  // value, then it indicates the code ended up here as a result of a malicious
  // attack. As a result we want to crash the process. (crbug.com/976506)
  EnableMojoJS(context, /*enable*/ true);
  blink::ExecutionContext::From(context)->SetMojoJSInterfaceBroker(
      std::move(broker_remote));
}

// static
void WebV8Features::EnableMojoJSFileSystemAccessHelper(
    v8::Local<v8::Context> context,
    bool enable) {
  if (enable) {
    // If the code is trying to enable mojo JS but mojo JS is not allowed for
    // the process, as determined by the protected memory bool value, then it
    // indicates the code ended up here as a result of a malicious attack. As a
    // result we want to crash the process.
    // (crbug.com/976506)
    ContextFeatureSettings::CrashIfMojoJSNotAllowed();
  }
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  DCHECK(script_state->World().IsMainWorld());

  auto* context_feature_settings = ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists);

  if (!context_feature_settings->isMojoJSEnabled())
    return;

  context_feature_settings->EnableMojoJSFileSystemAccessHelper(enable);
}

// static
void WebV8Features::InitializeMojoJSAllowedProtectedMemory() {
  ContextFeatureSettings::InitializeMojoJSAllowedProtectedMemory();
}

// static
void WebV8Features::AllowMojoJSForProcess() {
  ContextFeatureSettings::AllowMojoJSForProcess();
}

// static
bool WebV8Features::IsMojoJSEnabledForTesting(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings* settings = ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kDontCreateIfNotExists);
  return settings && settings->isMojoJSEnabled();
}

// static
void WebV8Features::EnableMojoJSWithoutSecurityChecksForTesting(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnableMojoJS(true);
}

// static
void WebV8Features::SetIsolatePriority(base::Process::Priority priority) {
  auto isolate_priority = ToIsolatePriority(priority);
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](v8::Isolate::Priority priority, v8::Isolate* isolate) {
            isolate->SetPriority(priority);
          },
          isolate_priority));
  WorkerBackingThread::SetWorkerThreadIsolatesPriority(isolate_priority);
}

}  // namespace blink
