// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_EXCEPTION_HANDLER_H_
#define EXTENSIONS_RENDERER_BINDINGS_EXCEPTION_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8.h"

namespace extensions {

// A class to handle uncaught exceptions encountered in the bindings system
// while running untrusted code, such as exceptions thrown during callback
// execution or event handling.
class ExceptionHandler {
 public:
  ExceptionHandler(const binding::AddConsoleError& add_console_error);
  ~ExceptionHandler();

  // Handles an exception in the given |context|. |message| is a message to
  // prefix the error message with, e.g. "Exception in response to foo".
  // The |try_catch| is the TryCatch that caught the exception.
  void HandleException(v8::Local<v8::Context> context,
                       const std::string& message,
                       v8::TryCatch* try_catch);
  // Same as above, but accepts a v8::Value for the exception rather than
  // retrieving it from a TryCatch.
  void HandleException(v8::Local<v8::Context> context,
                       const std::string& full_message,
                       v8::Local<v8::Value> exception_value);

  // Sets a custom handler for the given context, which will be notified of
  // exceptions thrown. This is only allowed to be called while the context is
  // valid.
  void SetHandlerForContext(v8::Local<v8::Context> context,
                            v8::Local<v8::Function> handler);

 private:
  // Returns the custom handler for the given |context|, or an empty handle if
  // no custom handle exists.
  v8::Local<v8::Function> GetCustomHandler(v8::Local<v8::Context> context);

  binding::AddConsoleError add_console_error_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_EXCEPTION_HANDLER_H_
