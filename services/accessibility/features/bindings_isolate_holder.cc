// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/bindings_isolate_holder.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "gin/v8_initializer.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace ax {

// static
void BindingsIsolateHolder::InitializeV8() {
  // Only initialize V8 for the Accessibility Service once.
  if (gin::IsolateHolder::Initialized())
    return;

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
}

BindingsIsolateHolder::BindingsIsolateHolder() = default;

BindingsIsolateHolder::~BindingsIsolateHolder() = default;

void BindingsIsolateHolder::AddObserver(IsolateObserver* observer) {
  observers_.AddObserver(observer);
}

void BindingsIsolateHolder::RemoveObserver(IsolateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BindingsIsolateHolder::NotifyIsolateWillDestroy() {
  for (IsolateObserver& obs : observers_) {
    obs.OnIsolateWillDestroy();
  }
}

bool BindingsIsolateHolder::ExecuteScriptInContext(const std::string& script) {
  // Enter isolate scope.
  v8::Isolate::Scope isolate_scope(GetIsolate());

  // Creates and enters stack-allocated handle scope.
  // All the Local handles (Local<>) in this function will belong to this
  // HandleScope and will be garbage collected when it goes out of scope in this
  // C++ function.
  v8::HandleScope handle_scope(GetIsolate());

  // Enter the context for compiling and running the script.
  v8::Context::Scope context_scope(GetContext());
  {
    const char* code_c = script.c_str();
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(GetIsolate(), code_c).ToLocalChecked();

    v8::TryCatch trycatch(GetIsolate());

    // Compile the source code, checking for errors.
    v8::Local<v8::Script> compiled;
    if (!v8::Script::Compile(GetContext(), source).ToLocal(&compiled)) {
      DCHECK(trycatch.HasCaught());
      HandleError(ExceptionToString(trycatch));
      return false;
    }

    // Run the script, checking for errors.
    v8::MaybeLocal<v8::Value> maybe_result = compiled->Run(GetContext());
    if (maybe_result.IsEmpty()) {
      DCHECK(trycatch.HasCaught());
      HandleError(ExceptionToString(trycatch));
      return false;
    }

    return true;
  }
}

void BindingsIsolateHolder::HandleError(const std::string& message) {
  LOG(ERROR) << message;
}

// From chrome/test/base/v8_unit_test.cc.
std::string BindingsIsolateHolder::ExceptionToString(
    const v8::TryCatch& try_catch) {
  std::string str;
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::String::Utf8Value exception(isolate, try_catch.Exception());
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    str.append(base::StringPrintf("%s\n", *exception));
  } else {
    v8::String::Utf8Value filename(isolate,
                                   message->GetScriptOrigin().ResourceName());
    int linenum = message->GetLineNumber(context).ToChecked();
    int colnum = message->GetStartColumn(context).ToChecked();
    str.append(base::StringPrintf("%s:%i:%i %s\n", *filename, linenum, colnum,
                                  *exception));
    v8::String::Utf8Value sourceline(
        isolate, message->GetSourceLine(context).ToLocalChecked());
    str.append(base::StringPrintf("%s\n", *sourceline));
  }
  return str;
}

}  // namespace ax
