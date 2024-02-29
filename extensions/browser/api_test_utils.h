// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionFunctionDispatcher;

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

// The mode a function is supposed to be run with.
enum class FunctionMode {
  kNone,
  kIncognito,
};

// Get |key| from |val| as the specified type. If |key| does not exist, or is
// not of the specified type, adds a failure to the current test and returns
// false, 0, empty string, etc.
bool GetBoolean(const base::Value::Dict& val, const std::string& key);
int GetInteger(const base::Value::Dict& val, const std::string& key);
std::string GetString(const base::Value::Dict& val, const std::string& key);
base::Value::List GetList(const base::Value::Dict& val, const std::string& key);
base::Value::Dict GetDict(const base::Value::Dict& val, const std::string& key);

// If |val| is a dictionary, return it as one, otherwise create an empty one.
base::Value::Dict ToDict(std::optional<base::ValueView> val);
// If |val| is a list, return it as one, otherwise create an empty one.
base::Value::List ToList(std::optional<base::ValueView> val);

// Currently, we allow either a string for the args, which is parsed to a list,
// or an already-constructed list.
using ArgsType = absl::variant<std::string, base::Value::List>;

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. Takes ownership of
// |function|. The caller takes ownership of the result.
std::optional<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    ArgsType args,
    std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
    FunctionMode mode);

// RunFunctionWithDelegateAndReturnSingleResult, except with a NULL
// implementation of the Delegate.
std::optional<base::Value> RunFunctionAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    ArgsType args,
    content::BrowserContext* context,
    FunctionMode mode = FunctionMode::kNone);

// Run |function| with |args| and return the resulting error. Adds an error to
// the current test if |function| returns a result. Takes ownership of
// |function|.
std::string RunFunctionAndReturnError(scoped_refptr<ExtensionFunction> function,
                                      ArgsType args,
                                      content::BrowserContext* context,
                                      FunctionMode mode = FunctionMode::kNone);

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
bool RunFunction(scoped_refptr<ExtensionFunction> function,
                 ArgsType args,
                 content::BrowserContext* context,
                 FunctionMode mode = FunctionMode::kNone);
bool RunFunction(scoped_refptr<ExtensionFunction> function,
                 ArgsType args,
                 std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
                 FunctionMode mode);

}  // namespace api_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_UTILS_H_
