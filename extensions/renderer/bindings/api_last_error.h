// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_LAST_ERROR_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_LAST_ERROR_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "v8/include/v8.h"

namespace extensions {

// Handles creating and clearing a 'lastError' property to hold the last error
// set by an extension API function.
class APILastError {
 public:
  // Returns the object the 'lastError' property should be exposed on for the
  // given context. Also allows for the population of a |secondary_parent|; if
  // populated, this object will also have a lastError property, but it will be
  // a simple object without getters/setters. This is to accommodate the
  // legacy chrome.extension.lastError property.
  // Note: |secondary_parent| may be null.
  using GetParent = base::RepeatingCallback<v8::Local<v8::Object>(
      v8::Local<v8::Context>,
      v8::Local<v8::Object>* secondary_parent)>;

  APILastError(GetParent get_parent,
               binding::AddConsoleError add_console_error);
  APILastError(APILastError&& other);

  APILastError(const APILastError&) = delete;
  APILastError& operator=(const APILastError&) = delete;

  ~APILastError();

  // Sets the last error for the given |context| to |error|.
  void SetError(v8::Local<v8::Context> context, const std::string& error);

  // Clears the last error in the given |context|. If |report_if_unchecked| is
  // true and the developer didn't check the error, this throws an exception.
  void ClearError(v8::Local<v8::Context> context, bool report_if_unchecked);

  // Returns true if the given context has an active error.
  bool HasError(v8::Local<v8::Context> context);

  // Returns the message from the last error for the given |context| without
  // marking it as accessed. If the given context doesn't have an active error
  // returns std::nullopt.
  std::optional<std::string> GetErrorMessage(v8::Local<v8::Context> context);

  // Reports an unchecked error by logging it to the console. This is used when
  // an error occurs, and there is no way it could be checked.
  void ReportUncheckedError(v8::Local<v8::Context> context,
                            const std::string& error);

 private:
  // Sets the lastError property on the primary parent object (in practice, this
  // is chrome.runtime.lastError);
  void SetErrorOnPrimaryParent(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> parent,
                               const std::string& error);

  // Sets the lastError property on the secondary parent object (in practice,
  // this is chrome.extension.lastError).
  void SetErrorOnSecondaryParent(v8::Local<v8::Context> context,
                                 v8::Local<v8::Object> parent,
                                 const std::string& error);

  GetParent get_parent_;

  binding::AddConsoleError add_console_error_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_LAST_ERROR_H_
