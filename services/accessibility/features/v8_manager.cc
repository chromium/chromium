// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_manager.h"

#include <utility>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/features/automation_internal_bindings.h"
#include "services/accessibility/features/mojo/mojo.h"
#include "services/accessibility/features/tts_interface_binder.h"
#include "services/accessibility/features/v8_bindings_utils.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace ax {

namespace {

// The index into the global template's internal fields that
// stores a pointer to this V8Environment. This allows any class with
// a v8::Context to access this V8Environment.
static const int kV8ContextWrapperIndex = 0;

}  // namespace

// static
base::SequenceBound<V8Environment> V8Environment::Create(
    base::WeakPtr<V8Manager> manager) {
  // Create task runner for running V8. The Isolate should only ever be accessed
  // on this thread.
  auto v8_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  // Get a reference to the current SequencedTaskRunner for posting tasks back
  // to the constructor and current thread.
  auto main_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::SequenceBound<V8Environment> result(
      std::move(v8_runner), std::move(main_runner), std::move(manager));
  return result;
}

// static
V8Environment* V8Environment::GetFromContext(v8::Local<v8::Context> context) {
  v8::Local<v8::Object> global_proxy = context->Global();
  CHECK_LT(kV8ContextWrapperIndex, global_proxy->InternalFieldCount());
  V8Environment* v8_env = reinterpret_cast<V8Environment*>(
      global_proxy->GetAlignedPointerFromInternalField(kV8ContextWrapperIndex));
  CHECK(v8_env);
  return v8_env;
}

V8Environment::V8Environment(
    scoped_refptr<base::SequencedTaskRunner> main_runner,
    base::WeakPtr<V8Manager> manager)
    : main_runner_(std::move(main_runner)), manager_(std::move(manager)) {
  CreateIsolate();
}

V8Environment::~V8Environment() {
  if (!isolate_holder_)
    return;

  NotifyIsolateWillDestroy();

  isolate_holder_->isolate()->TerminateExecution();
  context_holder_.reset();
  isolate_holder_.reset();
}

void V8Environment::InstallAutomation(
    mojo::PendingAssociatedReceiver<mojom::Automation> automation,
    mojo::PendingRemote<mojom::AutomationClient> automation_client) {
  automation_bindings_ = std::make_unique<AutomationInternalBindings>(
      this, std::move(automation), std::move(automation_client));
}

void V8Environment::ExecuteScript(const std::string& script,
                                  base::OnceCallback<void()> on_complete) {
  bool result = BindingsIsolateHolder::ExecuteScriptInContext(script);
  DCHECK(result);
  // FIXME: For callers, it might be nice to bounce back to the calling thread
  // rather than exposing V8Environment's internal implementation thread.
  std::move(on_complete).Run();
}

v8::Isolate* V8Environment::GetIsolate() const {
  return isolate_holder_ ? isolate_holder_->isolate() : nullptr;
}

v8::Local<v8::Context> V8Environment::GetContext() const {
  return context_holder_->context();
}

void V8Environment::BindInterface(
    const std::string& interface_name,
    mojo::GenericPendingReceiver pending_receiver) {
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::BindInterface, manager_,
                                interface_name, std::move(pending_receiver)));
}

void V8Environment::CreateIsolate() {
  std::unique_ptr<v8::Isolate::CreateParams> params =
      gin::IsolateHolder::getDefaultIsolateParams();
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility, std::move(params));
}

void V8Environment::AddV8Bindings() {
  DCHECK(isolate_holder_) << "V8 has not been started, cannot bind.";

  v8::Isolate* isolate = isolate_holder_->isolate();

  // Enter isolate scope.
  v8::Isolate::Scope isolate_scope(isolate);

  // Creates and enters stack-allocated handle scope.
  // All the Local handles (Local<>) in this function will belong to this
  // HandleScope and will be garbage collected when it goes out of scope in this
  // C++ function.
  v8::HandleScope handle_scope(isolate);

  // Create a template for the global object where we set the
  // built-in global functions.
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(kV8ContextWrapperIndex + 1);

  // Create a template for the global "chrome" object.
  // We add this because APIs are found in the chrome namespace in JS,
  // like chrome.automation, etc.
  v8::Local<v8::ObjectTemplate> chrome_template =
      v8::ObjectTemplate::New(isolate);
  global_template->Set(isolate, "chrome", chrome_template);

  // Add automation bindings if needed.
  if (automation_bindings_) {
    v8::Local<v8::ObjectTemplate> automation_template =
        v8::ObjectTemplate::New(isolate);
    automation_bindings_->AddAutomationRoutesToTemplate(&automation_template);
    chrome_template->Set(isolate, "automation", automation_template);
    v8::Local<v8::ObjectTemplate> automation_internal_template =
        v8::ObjectTemplate::New(isolate);
    automation_bindings_->AddAutomationInternalRoutesToTemplate(
        &automation_internal_template);
    chrome_template->Set(isolate, "automationInternal",
                         automation_internal_template);
  }

  // Adds atpconsole.log/warn/error.
  // TODO(crbug.com/1355633): Deprecate and use console.log/warn/error instead.
  BindingsUtils::AddAtpConsoleTemplate(isolate, global_template);

  // Add TextEncoder and TextDecoder, which are used by Mojo to pass strings,
  // as well as some Accessibility features.
  BindingsUtils::AddCallHandlerToTemplate(
      isolate, global_template, "TextEncoder",
      BindingsUtils::CreateTextEncoderCallback);
  BindingsUtils::AddCallHandlerToTemplate(
      isolate, global_template, "TextDecoder",
      BindingsUtils::CreateTextDecoderCallback);

  // TODO(crbug.com/1355633): Add other API bindings to the global template.

  // Add the global template to the current context.
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, /*extensions=*/nullptr, global_template);
  context_holder_ = std::make_unique<gin::ContextHolder>(isolate);
  context_holder_->SetContext(context);

  // Make a pointer to `this` in the current context.
  // This allows Mojo to use the context to find `this` and bind interfaces
  // on it.
  v8::Local<v8::Object> global_proxy = context->Global();
  DCHECK_EQ(kV8ContextWrapperIndex + 1, global_proxy->InternalFieldCount());

  global_proxy->SetAlignedPointerInInternalField(kV8ContextWrapperIndex, this);

  // Create a template for the "Mojo" object in the context scope.
  v8::Context::Scope context_scope(context);
  gin::Handle<Mojo> mojo = Mojo::Create(context);
  global_proxy->Set(context, gin::StringToV8(isolate, "Mojo"), mojo.ToV8())
      .Check();

  // TODO(crbug.com/1355633): At this point we could load in API Javascript
  // using ExecuteScript.
}

V8Manager::V8Manager() {
  // Note: this can't be in the initializer list because WeakPtrFactory fields
  // must be declared after all other fields.
  v8_env_ = V8Environment::Create(weak_factory_.GetWeakPtr());
}

V8Manager::~V8Manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void V8Manager::ConfigureAutomation(
    mojo::PendingAssociatedReceiver<mojom::Automation> automation,
    mojo::PendingRemote<mojom::AutomationClient> automation_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::InstallAutomation)
      .WithArgs(std::move(automation), std::move(automation_client));
}

void V8Manager::ConfigureTts(
    mojom::AccessibilityServiceClient* ax_service_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/262637071): load the TTS JS shim into V8 using v8_env_.AsyncCall.
  interface_binders_.push_back(
      std::make_unique<TtsInterfaceBinder>(ax_service_client));
}

void V8Manager::FinishContextSetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::AddV8Bindings);
}

void V8Manager::AddInterfaceForTest(
    std::unique_ptr<InterfaceBinder> interface_binder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interface_binders_.push_back(std::move(interface_binder));
}

void V8Manager::BindInterface(const std::string& interface_name,
                              mojo::GenericPendingReceiver pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b:262637071): Add mappings for bindings for other C++/JS mojom APIs.
  // TODO(b:262637071): We may need to use associated remotes/receivers to avoid
  // messages coming in an unpredicted order compared to the Extensions system.
  for (auto& binder : interface_binders_) {
    if (binder->MatchesInterface(interface_name)) {
      binder->BindReceiver(std::move(pending_receiver));
      return;
    }
  }
  LOG(ERROR) << "Couldn't find Receiver for interface " << interface_name
             << ". Ensure it's installed in the isolate with "
             << "V8Manager::AddInterface and available for binding in "
             << "V8Manager::BindInterface.";
}

void V8Manager::RunScriptForTest(const std::string& script,
                                 base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  v8_env_.AsyncCall(&V8Environment::ExecuteScript)
      .WithArgs(script, std::move(on_complete));
}

}  // namespace ax
