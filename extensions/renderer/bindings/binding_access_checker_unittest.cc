// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/binding_access_checker.h"

#include "base/functional/bind.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "gin/converter.h"

namespace extensions {

namespace {

bool APIIsAvailable(v8::Local<v8::Context> context,
                    const std::string& full_name) {
  EXPECT_TRUE(full_name == "available" || full_name == "unavailable")
      << full_name;
  return full_name == "available";
}

bool PromisesAvailable(v8::Local<v8::Context> context) {
  return true;
}

}  // namespace

using BindingAccessCheckerTest = APIBindingTest;

TEST_F(BindingAccessCheckerTest, TestHasAccess) {
  v8::HandleScope handle_scope(isolate());

  BindingAccessChecker checker(base::BindRepeating(&APIIsAvailable),
                               base::BindRepeating(&PromisesAvailable));

  v8::Local<v8::Context> context = MainContext();
  EXPECT_TRUE(checker.HasAccess(context, "available"));
  EXPECT_FALSE(checker.HasAccess(context, "unavailable"));
}

TEST_F(BindingAccessCheckerTest, TestHasAccessOrThrowError) {
  v8::HandleScope handle_scope(isolate());

  BindingAccessChecker checker(base::BindRepeating(&APIIsAvailable),
                               base::BindRepeating(&PromisesAvailable));

  v8::Local<v8::Context> context = MainContext();
  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(checker.HasAccessOrThrowError(context, "available"));
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    EXPECT_FALSE(checker.HasAccessOrThrowError(context, "unavailable"));
    ASSERT_TRUE(try_catch.HasCaught());
    EXPECT_EQ("Uncaught Error: 'unavailable' is not available in this context.",
              gin::V8ToString(isolate(), try_catch.Message()->Get()));
  }
}

TEST_F(BindingAccessCheckerTest, TestHasPromiseAccess) {
  bool context_allows_promises = true;
  auto promises_available = base::BindRepeating(
      [](bool* flag, v8::Local<v8::Context> context) { return *flag; },
      &context_allows_promises);

  v8::HandleScope handle_scope(isolate());

  BindingAccessChecker checker(base::BindRepeating(&APIIsAvailable),
                               promises_available);

  v8::Local<v8::Context> context = MainContext();
  EXPECT_TRUE(checker.HasPromiseAccess(context));

  context_allows_promises = false;
  EXPECT_FALSE(checker.HasPromiseAccess(context));
}

}  // namespace extensions
