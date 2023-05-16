// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_manager.h"

#include "base/memory/ref_counted_delete_on_sequence.h"
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
// stores a pointer to this V8Manager. This allows any class with
// a v8::Context to access this V8Manager.
static const int kV8ContextWrapperIndex = 0;

}  // namespace

// static
scoped_refptr<V8Manager> V8Manager::Create() {
  // Create task runner for running V8. The Isolate should only ever be accessed
  // on this thread.
  auto v8_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  // Get a reference to the current SequencedTaskRunner for posting tasks back
  // to the constructor and current thread.
  auto main_runner = base::SequencedTaskRunner::GetCurrentDefault();
  scoped_refptr<V8Manager> result(new V8Manager(v8_runner, main_runner));
  v8_runner->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::ConstructIsolateOnThread, result));
  return result;
}

// static
V8Manager* V8Manager::GetFromContext(v8::Local<v8::Context> context) {
  v8::Local<v8::Object> global_proxy = context->Global();
  CHECK_LT(kV8ContextWrapperIndex, global_proxy->InternalFieldCount());
  V8Manager* v8_manager = reinterpret_cast<V8Manager*>(
      global_proxy->GetAlignedPointerFromInternalField(kV8ContextWrapperIndex));
  CHECK(v8_manager);
  return v8_manager;
}

V8Manager::V8Manager(scoped_refptr<base::SingleThreadTaskRunner> v8_runner,
                     scoped_refptr<base::SequencedTaskRunner> main_runner)
    : base::RefCountedDeleteOnSequence<V8Manager>(v8_runner),
      v8_runner_(v8_runner),
      main_runner_(main_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

V8Manager::~V8Manager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!isolate_holder_)
    return;

  NotifyIsolateWillDestroy();

  isolate_holder_->isolate()->TerminateExecution();
  context_holder_.reset();
  isolate_holder_.reset();
}

void V8Manager::InstallAutomation(
    base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(main_runner_ == base::SequencedTaskRunner::GetCurrentDefault());
  v8_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::BindAutomationOnThread,
                                weak_ptr_factory_.GetWeakPtr(), at_controller));
}

void V8Manager::InstallTts(
    base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(main_runner_ == base::SequencedTaskRunner::GetCurrentDefault());
  v8_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::BindTtsOnThread,
                                weak_ptr_factory_.GetWeakPtr(), at_controller));
}

void V8Manager::AddV8Bindings() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(main_runner_ == base::SequencedTaskRunner::GetCurrentDefault());
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&V8Manager::AddV8BindingsOnThread,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void V8Manager::ExecuteScript(const std::string& script,
                              base::OnceCallback<void()> on_complete) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(main_runner_ == base::SequencedTaskRunner::GetCurrentDefault());
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&V8Manager::ExecuteScriptOnThread,
                                      weak_ptr_factory_.GetWeakPtr(), script,
                                      std::move(on_complete)));
}

v8::Isolate* V8Manager::GetIsolate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return isolate_holder_ ? isolate_holder_->isolate() : nullptr;
}

v8::Local<v8::Context> V8Manager::GetContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_holder_->context();
}

void V8Manager::BindInterface(const std::string& interface_name,
                              mojo::GenericPendingReceiver pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b:262637071): Add mappings for bindings for other C++/JS mojom APIs.
  // TODO(b:262637071): We may need to use associated remotes/receivers to avoid
  // messages coming in an unpredicted order compared to the Extensions system.
  if (test_mojo_interface_ &&
      test_mojo_interface_->MatchesInterface(interface_name)) {
    test_mojo_interface_->BindReceiver(std::move(pending_receiver));
    return;
  }
  if (tts_interface_binder_ &&
      tts_interface_binder_->MatchesInterface(interface_name)) {
    tts_interface_binder_->BindReceiver(std::move(pending_receiver));
    return;
  }
  LOG(ERROR) << "Couldn't find Receiver for interface " << interface_name
             << ". Ensure it's installed in the isolate with "
             << "V8Manager::Install* and available for binding in "
             << "V8Manager::BindInterface.";
}

void V8Manager::SetTestMojoInterface(
    std::unique_ptr<InterfaceBinder> test_interface) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(main_runner_ == base::SequencedTaskRunner::GetCurrentDefault());
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&V8Manager::SetTestMojoInterfaceOnThread,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(test_interface)));
}

void V8Manager::ConstructIsolateOnThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (isolate_holder_ && context_holder_)
    return;

  std::unique_ptr<v8::Isolate::CreateParams> params =
      gin::IsolateHolder::getDefaultIsolateParams();
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      v8_runner_, gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility, std::move(params));
}

void V8Manager::AddV8BindingsOnThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void V8Manager::BindAutomationOnThread(
    base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Construct the AutomationInternalBindings and its routes.
  automation_bindings_ = std::make_unique<AutomationInternalBindings>(
      weak_ptr_factory_.GetWeakPtr(), at_controller, main_runner_);
}

void V8Manager::BindTtsOnThread(
    base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tts_interface_binder_ =
      std::make_unique<TtsInterfaceBinder>(at_controller, main_runner_);
}

void V8Manager::SetTestMojoInterfaceOnThread(
    std::unique_ptr<InterfaceBinder> test_interface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  test_mojo_interface_ = std::move(test_interface);
}

void V8Manager::ExecuteScriptOnThread(const std::string& script,
                                      base::OnceCallback<void()> on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool result = BindingsIsolateHolder::ExecuteScriptInContext(script);
  DCHECK(result);
  std::move(on_complete).Run();
}

}  // namespace ax
