// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api_test_utils.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionFunctionDispatcher;

namespace {

std::unique_ptr<base::Value> ParseJSON(const std::string& data) {
  return base::JSONReader::ReadDeprecated(data);
}

std::unique_ptr<base::ListValue> ParseList(const std::string& data) {
  return base::ListValue::From(ParseJSON(data));
}

}  // namespace

namespace extensions {

namespace api_test_utils {

SendResponseHelper::SendResponseHelper(ExtensionFunction* function) {
  function->set_has_callback(true);
  function->set_response_callback(
      base::BindOnce(&SendResponseHelper::OnResponse, base::Unretained(this)));
}

SendResponseHelper::~SendResponseHelper() {}

bool SendResponseHelper::GetResponse() {
  EXPECT_TRUE(has_response());
  return *response_;
}

void SendResponseHelper::OnResponse(ExtensionFunction::ResponseType response,
                                    base::Value::List results,
                                    const std::string& error,
                                    mojom::ExtraResponseDataPtr) {
  ASSERT_NE(ExtensionFunction::BAD_MESSAGE, response);
  response_ = std::make_unique<bool>(response == ExtensionFunction::SUCCEEDED);
  run_loop_.Quit();
}

void SendResponseHelper::WaitForResponse() {
  run_loop_.Run();
}

std::unique_ptr<base::DictionaryValue> ParseDictionary(
    const std::string& data) {
  return base::DictionaryValue::From(ParseJSON(data));
}

bool GetBoolean(const base::Value::Dict& dict, const std::string& key) {
  absl::optional<bool> value = dict.FindBool(key);
  if (!value.has_value()) {
    ADD_FAILURE() << key << " does not exist or is not a boolean.";
    return false;
  }
  return *value;
}

int GetInteger(const base::Value::Dict& dict, const std::string& key) {
  absl::optional<int> value = dict.FindInt(key);
  if (!value.has_value()) {
    ADD_FAILURE() << key << " does not exist or is not an integer.";
    return 0;
  }
  return *value;
}

std::string GetString(const base::Value::Dict& dict, const std::string& key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    ADD_FAILURE() << key << " does not exist or is not a string.";
    return "";
  }
  return *value;
}

base::Value::List GetList(const base::Value::Dict& dict,
                          const std::string& key) {
  const base::Value::List* value = dict.FindList(key);
  if (!value) {
    ADD_FAILURE() << key << " does not exist or is not a list.";
    return base::Value::List();
  }
  return value->Clone();
}

base::Value::Dict GetDict(const base::Value::Dict& dict,
                          const std::string& key) {
  const base::Value::Dict* value = dict.FindDict(key);
  if (!value) {
    ADD_FAILURE() << key << " does not exist or is not a dict.";
    return base::Value::Dict();
  }
  return value->Clone();
}

std::unique_ptr<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    const std::string& args,
    std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags) {
  std::unique_ptr<base::ListValue> parsed_args = ParseList(args);
  EXPECT_TRUE(parsed_args.get())
      << "Could not parse extension function arguments: " << args;

  return RunFunctionWithDelegateAndReturnSingleResult(
      function, std::move(parsed_args), std::move(dispatcher), flags);
}

std::unique_ptr<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    std::unique_ptr<base::ListValue> args,
    std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags) {
  RunFunction(function.get(), std::move(args), std::move(dispatcher), flags);
  EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
                                            << function->GetError();
  if (function->GetResultList() && !function->GetResultList()->empty()) {
    const base::Value& single_result = (*function->GetResultList())[0];
    return std::make_unique<base::Value>(single_result.Clone());
  }
  return nullptr;
}

std::unique_ptr<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context) {
  return RunFunctionAndReturnSingleResult(function, args, context, NONE);
}

std::unique_ptr<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    RunFunctionFlags flags) {
  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context));

  return RunFunctionWithDelegateAndReturnSingleResult(
      function, args, std::move(dispatcher), flags);
}

std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context) {
  return RunFunctionAndReturnError(function, args, context, NONE);
}

std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context,
                                      RunFunctionFlags flags) {
  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context));
  scoped_refptr<ExtensionFunction> function_owner(function);
  // Without a callback the function will not generate a result.
  RunFunction(function, args, std::move(dispatcher), flags);
  // When sending a response, the function will set an empty list value if there
  // is no specified result.
  const base::Value::List* results = function->GetResultList();
  CHECK(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  return function->GetError();
}

bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context) {
  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context));
  return RunFunction(function, args, std::move(dispatcher), NONE);
}

bool RunFunction(
    ExtensionFunction* function,
    const std::string& args,
    std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags) {
  std::unique_ptr<base::ListValue> parsed_args = ParseList(args);
  EXPECT_TRUE(parsed_args.get())
      << "Could not parse extension function arguments: " << args;
  return RunFunction(function, std::move(parsed_args), std::move(dispatcher),
                     flags);
}

bool RunFunction(
    ExtensionFunction* function,
    std::unique_ptr<base::ListValue> args,
    std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags) {
  SendResponseHelper response_helper(function);
  function->SetArgs(base::Value::FromUniquePtrValue(std::move(args)));

  CHECK(dispatcher);
  function->SetDispatcher(dispatcher->AsWeakPtr());

  function->set_include_incognito_information(flags & INCLUDE_INCOGNITO);
  function->preserve_results_for_testing();
  function->RunWithValidation()->Execute();
  response_helper.WaitForResponse();

  EXPECT_TRUE(response_helper.has_response());
  return response_helper.GetResponse();
}

}  // namespace api_test_utils
}  // namespace extensions
