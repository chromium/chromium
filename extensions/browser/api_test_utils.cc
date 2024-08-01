// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api_test_utils.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionFunctionDispatcher;

namespace extensions {

namespace api_test_utils {

SendResponseHelper::SendResponseHelper(ExtensionFunction* function) {
  function->set_has_callback(true);
  function->set_response_callback(
      base::BindOnce(&SendResponseHelper::OnResponse, base::Unretained(this)));
}

SendResponseHelper::~SendResponseHelper() = default;

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

bool GetBoolean(const base::Value::Dict& dict, const std::string& key) {
  std::optional<bool> value = dict.FindBool(key);
  if (!value.has_value()) {
    ADD_FAILURE() << key << " does not exist or is not a boolean.";
    return false;
  }
  return *value;
}

int GetInteger(const base::Value::Dict& dict, const std::string& key) {
  std::optional<int> value = dict.FindInt(key);
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

base::Value::Dict ToDict(std::optional<base::ValueView> val) {
  if (!val) {
    ADD_FAILURE() << "val is nullopt";
    return base::Value::Dict();
  }
  base::Value result = val->ToValue();
  if (!result.is_dict()) {
    ADD_FAILURE() << "val is not a dictionary";
    return base::Value::Dict();
  }
  return std::move(result).TakeDict();
}

base::Value::List ToList(std::optional<base::ValueView> val) {
  if (!val) {
    ADD_FAILURE() << "val is nullopt";
    return base::Value::List();
  }
  base::Value result = val->ToValue();
  if (!result.is_list()) {
    ADD_FAILURE() << "val is not a dictionary";
    return base::Value::List();
  }
  return std::move(result).TakeList();
}

std::optional<base::Value> RunFunctionWithDelegateAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    ArgsType args,
    std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
    FunctionMode mode) {
  RunFunction(function, std::move(args), std::move(dispatcher), mode);
  EXPECT_TRUE(function->GetError().empty())
      << "Unexpected error: " << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  if (!results || results->empty()) {
    return std::nullopt;
  }
  return (*results)[0].Clone();
}

std::optional<base::Value> RunFunctionAndReturnSingleResult(
    scoped_refptr<ExtensionFunction> function,
    ArgsType args,
    content::BrowserContext* context,
    FunctionMode mode) {
  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(context);

  return RunFunctionWithDelegateAndReturnSingleResult(
      std::move(function), std::move(args), std::move(dispatcher), mode);
}

std::string RunFunctionAndReturnError(scoped_refptr<ExtensionFunction> function,
                                      ArgsType args,
                                      content::BrowserContext* context,
                                      FunctionMode mode) {
  // Without a callback the function will not generate a result.
  RunFunction(function, std::move(args), context, mode);
  // When sending a response, the function will set an empty list value if there
  // is no specified result.
  const base::Value::List* results = function->GetResultListForTest();
  CHECK(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  return function->GetError();
}

bool RunFunction(scoped_refptr<ExtensionFunction> function,
                 ArgsType args,
                 content::BrowserContext* context,
                 FunctionMode mode) {
  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(context);
  return RunFunction(function, std::move(args), std::move(dispatcher), mode);
}

bool RunFunction(scoped_refptr<ExtensionFunction> function,
                 ArgsType args,
                 std::unique_ptr<ExtensionFunctionDispatcher> dispatcher,
                 FunctionMode mode) {
  static_assert(absl::variant_size<ArgsType>::value == 2, "Unhandled variant!");
  base::Value::List parsed_args =
      args.index() == 0 ? base::test::ParseJsonList(absl::get<0>(args))
                        : std::move(absl::get<1>(args));
  SendResponseHelper response_helper(function.get());
  function->SetArgs(std::move(parsed_args));

  CHECK(dispatcher);
  function->SetDispatcher(dispatcher->AsWeakPtr());

  function->set_include_incognito_information(mode == FunctionMode::kIncognito);
  function->preserve_results_for_testing();
  function->RunWithValidation().Execute();
  response_helper.WaitForResponse();

  EXPECT_TRUE(response_helper.has_response());
  return response_helper.GetResponse();
}

}  // namespace api_test_utils
}  // namespace extensions
