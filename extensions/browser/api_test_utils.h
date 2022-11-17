// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionFunctionDispatcher;

// TODO(yoz): crbug.com/394840: Remove duplicate functionality in
// chrome/browser/extensions/extension_function_test_utils.h.
//
// TODO(ckehoe): Accept args as std::unique_ptr<base::Value>,
// and migrate existing users to the new API.
namespace api_test_utils {

// A helper class to handle waiting for a function response.
class SendResponseHelper {
 public:
  explicit SendResponseHelper(ExtensionFunction* function);

  SendResponseHelper(const SendResponseHelper&) = delete;
  SendResponseHelper& operator=(const SendResponseHelper&) = delete;

  ~SendResponseHelper();

  bool has_response() { return response_.get() != nullptr; }

  // Asserts a response has been posted (has_response()) and returns the value.
  bool GetResponse();

  // Waits until a response is posted.
  void WaitForResponse();

 private:
  // Response handler.
  void OnResponse(ExtensionFunction::ResponseType response,
                  base::Value::List results,
                  const std::string& error,
                  mojom::ExtraResponseDataPtr);

  base::RunLoop run_loop_;
  std::unique_ptr<bool> response_;
};

enum RunFunctionFlags { NONE = 0, INCLUDE_INCOGNITO = 1 << 0 };

// Parses JSON and returns the dictionary, or absl::nullopt if the JSON is
// invalid or not a dictionary.
absl::optional<base::Value::Dict> ParseDictionary(const std::string& data);

// Get |key| from |val| as the specified type. If |key| does not exist, or is
// not of the specified type, adds a failure to the current test and returns
// false, 0, empty string, etc.
bool GetBoolean(const base::Value::Dict& val, const std::string& key);
int GetInteger(const base::Value::Dict& val, const std::string& key);
std::string GetString(const base::Value::Dict& val, const std::string& key);
base::Value::List GetList(const base::Value::Dict& val, const std::string& key);
base::Value::Dict GetDict(const base::Value::Dict& val, const std::string& key);

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. Takes ownership of
// |function|. The caller takes ownership of the result.
absl::optional<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    const std::string& args,
    std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags);
absl::optional<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    base::Value::List args,
    std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags);

// RunFunctionWithDelegateAndReturnSingleResult, except with a NULL
// implementation of the Delegate.
absl::optional<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context);
absl::optional<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    RunFunctionFlags flags);

// Run |function| with |args| and return the resulting error. Adds an error to
// the current test if |function| returns a result. Takes ownership of
// |function|.
std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context,
                                      RunFunctionFlags flags);
std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context);

// Create and run |function| with |args|. Works with both synchronous and async
// functions. Ownership of |function| remains with the caller.
//
// TODO(aa): It would be nice if |args| could be validated against the schema
// that |function| expects. That way, we know that we are testing something
// close to what the bindings would actually send.
//
// TODO(aa): I'm concerned that this style won't scale to all the bits and bobs
// we're going to need to frob for all the different extension functions. But
// we can refactor when we see what is needed.
bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context);
bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags);
bool RunFunction(ExtensionFunction* function,
                 base::Value::List args,
                 std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags);

}  // namespace api_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_UTILS_H_
