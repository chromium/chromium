// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/exception_handler.h"

#include <optional>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "gin/converter.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

void PopulateError(std::optional<std::string>* error_out,
                   v8::Local<v8::Context> context,
                   const std::string& error) {
  *error_out = error;
}

void ThrowException(v8::Local<v8::Context> context,
                    const std::string& to_throw,
                    ExceptionHandler* handler) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Function> function = FunctionFromString(
      context,
      base::StringPrintf("(function() { throw %s; })", to_throw.c_str()));
  std::ignore = function->Call(context, v8::Undefined(isolate), 0, nullptr);
  ASSERT_TRUE(try_catch.HasCaught());
  handler->HandleException(context, "handled", &try_catch);
}

}  // namespace

using ExceptionHandlerTest = APIBindingTest;

TEST_F(ExceptionHandlerTest, TestBasicHandling) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::optional<std::string> logged_error;
  ExceptionHandler handler(base::BindRepeating(&PopulateError, &logged_error));

  ThrowException(context, "new Error('some error')", &handler);

  ASSERT_TRUE(logged_error);
  EXPECT_THAT(*logged_error, testing::StartsWith("handled: Error: some error"));
}

TEST_F(ExceptionHandlerTest, PerContextHandlers) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  std::optional<std::string> logged_error;
  ExceptionHandler handler(base::BindRepeating(&PopulateError, &logged_error));

  v8::Local<v8::Function> custom_handler = FunctionFromString(
      context_a,
      "(function(message, exception) {\n"
      "  this.loggedMessage = message;\n"
      "  this.loggedExceptionMessage = exception && exception.message;\n"
      "})");

  handler.SetHandlerForContext(context_a, custom_handler);
  ThrowException(context_a, "new Error('context a error')", &handler);
  EXPECT_FALSE(logged_error);
  EXPECT_THAT(GetStringPropertyFromObject(context_a->Global(), context_a,
                                          "loggedMessage"),
              testing::StartsWith("\"handled: Error: context a error"));
  EXPECT_EQ("\"context a error\"",
            GetStringPropertyFromObject(context_a->Global(), context_a,
                                        "loggedExceptionMessage"));

  ASSERT_TRUE(context_a->Global()
                  ->Set(context_a,
                        gin::StringToSymbol(isolate(), "loggedMessage"),
                        v8::Undefined(isolate()))
                  .ToChecked());
  ASSERT_TRUE(
      context_a->Global()
          ->Set(context_a,
                gin::StringToSymbol(isolate(), "loggedExceptionMessage"),
                v8::Undefined(isolate()))
          .ToChecked());

  ThrowException(context_b, "new Error('context b error')", &handler);
  ASSERT_TRUE(logged_error);
  EXPECT_THAT(*logged_error,
              testing::StartsWith("handled: Error: context b error"));
  EXPECT_EQ("undefined", GetStringPropertyFromObject(
                             context_a->Global(), context_a, "loggedMessage"));
  EXPECT_EQ("undefined",
            GetStringPropertyFromObject(context_a->Global(), context_a,
                                        "loggedExceptionMessage"));
}

TEST_F(ExceptionHandlerTest, ThrowingNonErrors) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::optional<std::string> logged_error;
  ExceptionHandler handler(base::BindRepeating(&PopulateError, &logged_error));

  ThrowException(context, "'hello'", &handler);
  ASSERT_TRUE(logged_error);
  EXPECT_EQ("handled: Uncaught hello", *logged_error);
  logged_error.reset();

  ThrowException(context, "{ message: 'hello' }", &handler);
  ASSERT_TRUE(logged_error);
  EXPECT_EQ("handled: Uncaught #<Object>", *logged_error);
  logged_error.reset();

  ThrowException(context, "{ toString: function() { throw 'goodbye' } }",
                 &handler);
  ASSERT_TRUE(logged_error);
  EXPECT_EQ("handled: Uncaught [object Object]", *logged_error);

  v8::Local<v8::Function> custom_handler =
      FunctionFromString(context,
                         "(function(message, exception) {\n"
                         "  this.loggedMessage = message;\n"
                         "  this.loggedException = exception;\n"
                         "})");

  handler.SetHandlerForContext(context, custom_handler);
  ThrowException(context, "'hello'", &handler);
  EXPECT_EQ(
      "\"handled: Uncaught hello\"",
      GetStringPropertyFromObject(context->Global(), context, "loggedMessage"));
  EXPECT_EQ("\"hello\"", GetStringPropertyFromObject(context->Global(), context,
                                                     "loggedException"));
}

TEST_F(ExceptionHandlerTest, StackTraces) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::optional<std::string> logged_error;
  ExceptionHandler handler(base::BindRepeating(&PopulateError, &logged_error));

  {
    v8::TryCatch try_catch(isolate());
    v8::Local<v8::Script> script =
        v8::Script::Compile(context,
                            gin::StringToV8(context->GetIsolate(),
                                            "throw new Error('simple');"))
            .ToLocalChecked();
    ASSERT_TRUE(script->Run(context).IsEmpty());
    ASSERT_TRUE(try_catch.HasCaught());
    handler.HandleException(context, "handled", &try_catch);

    ASSERT_TRUE(logged_error);
    EXPECT_EQ("handled: Error: simple\n    at <anonymous>:1:7", *logged_error);
  }

  logged_error.reset();

  {
    v8::TryCatch try_catch(isolate());
    v8::Local<v8::Function> throw_error_function = FunctionFromString(
        context, "(function() { throw new Error('function'); })");
    std::ignore = throw_error_function->Call(context, v8::Undefined(isolate()),
                                             0, nullptr);
    ASSERT_TRUE(try_catch.HasCaught());
    handler.HandleException(context, "handled", &try_catch);
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("handled: Error: function\n    at <anonymous>:1:21",
              *logged_error);
  }

  logged_error.reset();

  {
    v8::TryCatch try_catch(isolate());
    const char kNestedCall[] =
        "function throwError() { throw new Error('nested'); }\n"
        "function callThrowError() { throwError(); }\n"
        "callThrowError()\n";
    v8::Local<v8::Script> script =
        v8::Script::Compile(context,
                            gin::StringToV8(context->GetIsolate(), kNestedCall))
            .ToLocalChecked();
    ASSERT_TRUE(script->Run(context).IsEmpty());
    ASSERT_TRUE(try_catch.HasCaught());
    handler.HandleException(context, "handled", &try_catch);
    ASSERT_TRUE(logged_error);
    const char kExpectedError[] =
        "handled: Error: nested\n"
        "    at throwError (<anonymous>:1:31)\n"
        "    at callThrowError (<anonymous>:2:29)\n"
        "    at <anonymous>:3:1";
    EXPECT_EQ(kExpectedError, *logged_error);
  }
}

}  // namespace extensions
