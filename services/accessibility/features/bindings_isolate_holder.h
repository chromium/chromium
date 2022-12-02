// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_BINDINGS_ISOLATE_HOLDER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_BINDINGS_ISOLATE_HOLDER_H_

#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-local-handle.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace ax {

// Virtual class that allows bindings to access the context and
// isolate for V8, and can execute Javascript scripts in the V8 context.
// This may be overridden for testing API bindings.
class BindingsIsolateHolder {
 public:
  // Initializes V8 for the service. May be called from any thread.
  static void InitializeV8();

  // Gets the current isolate.
  virtual v8::Isolate* GetIsolate() const = 0;

  // Gets the current context.
  virtual v8::Local<v8::Context> GetContext() const = 0;

  // Compiles and executes the given string as a JS script in the
  // BindingsIsolateHolder's isolate and context.
  // Returns true if the script was executed, and false if there
  // was an error.
  bool ExecuteScriptInContext(const std::string& script);

  // Called when an exception is encountered when compiling or
  // executing a script during ExecuteScriptInContext. The default
  // implementation logs the error to the console, but this may
  // be overridden.
  virtual void HandleError(const std::string& message);

 private:
  // Converts a V8 exception to a human-readable string including
  // line number, if relevant, within the script.
  std::string ExceptionToString(const v8::TryCatch& try_catch);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_BINDINGS_ISOLATE_HOLDER_H_
