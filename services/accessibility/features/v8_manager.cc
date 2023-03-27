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
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/features/automation_internal_bindings.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-template.h"

namespace ax {

namespace {

// Methods for debugging.
// TODO(crbug.com/1355633): Use blink::mojom::DevToolsAgent interface to attach
// to Chrome devtools.
static std::string PrintArgs(gin::Arguments* args) {
  std::string statement;
  while (!args->PeekNext().IsEmpty()) {
    v8::String::Utf8Value value(args->isolate(), args->PeekNext());
    statement += base::StringPrintf("%s ", *value);
    args->Skip();
  }
  return statement;
}

// Provides temporary functionality for atpconsole.log.
static void ConsoleLog(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Info: " << PrintArgs(args);
}

// Provides temporary functionality for atpconsole.warn.
static void ConsoleWarn(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Error: " << PrintArgs(args);
}

// Provides temporary functionality for atpconsole.error.
static void ConsoleError(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Error: " << PrintArgs(args);
}

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

  isolate_holder_->isolate()->TerminateExecution();
  context_holder_.reset();
  isolate_holder_.reset();
}

void V8Manager::InstallAutomation(
    base::WeakPtr<AssistiveTechnologyControllerImpl> at_controller) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V8Manager::BindAutomationOnThread,
                                weak_ptr_factory_.GetWeakPtr(), at_controller));
}

void V8Manager::AddV8Bindings() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&V8Manager::AddV8BindingsOnThread,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void V8Manager::ExecuteScript(const std::string& script,
                              base::OnceCallback<void()> on_complete) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
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

  // Enter isolate scope.
  v8::Isolate::Scope isolate_scope(isolate_holder_->isolate());

  // Creates and enters stack-allocated handle scope.
  // All the Local handles (Local<>) in this function will belong to this
  // HandleScope and will be garbage collected when it goes out of scope in this
  // C++ function.
  v8::HandleScope handle_scope(isolate_holder_->isolate());

  // Create a template for the global object where we set the
  // built-in global functions.
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate_holder_->isolate());

  // Create a template for the global "chrome" object.
  v8::Local<v8::ObjectTemplate> chrome_template =
      v8::ObjectTemplate::New(isolate_holder_->isolate());
  global_template->Set(isolate_holder_->isolate(), "chrome", chrome_template);

  // Add automation bindings if needed.
  if (automation_bindings_) {
    v8::Local<v8::ObjectTemplate> automation_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());
    automation_bindings_->AddAutomationRoutesToTemplate(&automation_template);
    chrome_template->Set(isolate_holder_->isolate(), "automation",
                         automation_template);
    v8::Local<v8::ObjectTemplate> automation_internal_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());
    automation_bindings_->AddAutomationInternalRoutesToTemplate(
        &automation_internal_template);
    chrome_template->Set(isolate_holder_->isolate(), "automationInternal",
                         automation_internal_template);
  }
  // TODO(crbug.com/1355633): Add other API bindings to the global template.

  // Use static bindings for console functions for initial development.
  // Note that "console" seems to be protected in v8 so we have to make
  // our own, "atpconsole".
  // TODO(crbug.com/1355633): Use blink::mojom::DevToolsAgent interface to
  // attach to Chrome devtools and remove these temporary bindings.
  v8::Local<v8::ObjectTemplate> console_template =
      v8::ObjectTemplate::New(isolate_holder_->isolate());
  console_template->Set(
      isolate_holder_->isolate(), "log",
      gin::CreateFunctionTemplate(isolate_holder_->isolate(),
                                  base::BindRepeating(&ConsoleLog)));
  console_template->Set(
      isolate_holder_->isolate(), "warn",
      gin::CreateFunctionTemplate(isolate_holder_->isolate(),
                                  base::BindRepeating(&ConsoleWarn)));
  console_template->Set(
      isolate_holder_->isolate(), "error",
      gin::CreateFunctionTemplate(isolate_holder_->isolate(),
                                  base::BindRepeating(&ConsoleError)));
  global_template->Set(isolate_holder_->isolate(), "atpconsole",
                       console_template);

  // Add the global template to the current context.
  v8::Local<v8::Context> context = v8::Context::New(
      isolate_holder_->isolate(), /*extensions=*/nullptr, global_template);
  context_holder_ =
      std::make_unique<gin::ContextHolder>(isolate_holder_->isolate());
  context_holder_->SetContext(context);

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

void V8Manager::ExecuteScriptOnThread(const std::string& script,
                                      base::OnceCallback<void()> on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool result = BindingsIsolateHolder::ExecuteScriptInContext(script);
  DCHECK(result);
  std::move(on_complete).Run();
}

}  // namespace ax
