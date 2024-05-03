// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/shell_runner.h"

#include <memory>

#include "gin/converter.h"
#include "gin/per_context_data.h"
#include "gin/public/context_holder.h"
#include "gin/try_catch.h"
#include "v8/include/v8-script.h"

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace gin {

ShellRunnerDelegate::ShellRunnerDelegate() = default;

ShellRunnerDelegate::~ShellRunnerDelegate() = default;

v8::Local<ObjectTemplate> ShellRunnerDelegate::GetGlobalTemplate(
    ShellRunner* runner,
    v8::Isolate* isolate) {
  return v8::Local<ObjectTemplate>();
}

void ShellRunnerDelegate::DidCreateContext(ShellRunner* runner) {
}

void ShellRunnerDelegate::WillRunScript(ShellRunner* runner) {
}

void ShellRunnerDelegate::DidRunScript(ShellRunner* runner) {
}

void ShellRunnerDelegate::UnhandledException(ShellRunner* runner,
                                               TryCatch& try_catch) {
  CHECK(false) << try_catch.GetStackTrace();
}

ShellRunner::ShellRunner(ShellRunnerDelegate* delegate, Isolate* isolate)
    : delegate_(delegate) {
  v8::Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      Context::New(isolate, NULL, delegate_->GetGlobalTemplate(this, isolate));

  context_holder_ = std::make_unique<ContextHolder>(isolate);
  context_holder_->SetContext(context);
  PerContextData::From(context)->set_runner(this);

  v8::Context::Scope scope(context);
  delegate_->DidCreateContext(this);
}

ShellRunner::~ShellRunner() = default;

v8::MaybeLocal<v8::Value> ShellRunner::Run(const std::string& source,
                                           const std::string& resource_name) {
  v8::Isolate* isolate = GetContextHolder()->isolate();
  TryCatch try_catch(isolate);
  v8::ScriptOrigin origin(StringToV8(isolate, resource_name));
  auto maybe_script = Script::Compile(GetContextHolder()->context(),
                                      StringToV8(isolate, source), &origin);
  v8::Local<Script> script;
  if (!maybe_script.ToLocal(&script)) {
    delegate_->UnhandledException(this, try_catch);
    return v8::MaybeLocal<v8::Value>();
  }

  return Run(script);
}

ContextHolder* ShellRunner::GetContextHolder() {
  return context_holder_.get();
}

v8::MaybeLocal<v8::Value> ShellRunner::Run(v8::Local<Script> script) {
  TryCatch try_catch(GetContextHolder()->isolate());
  delegate_->WillRunScript(this);

  auto maybe = script->Run(GetContextHolder()->context());

  delegate_->DidRunScript(this);
  if (maybe.IsEmpty()) {
    delegate_->UnhandledException(this, try_catch);
  }
  return maybe;
}

}  // namespace gin
