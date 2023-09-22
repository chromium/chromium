// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_exception_state.h"

namespace blink {

class NoAllocDirectCallHostTest : public ::testing::Test {
 public:
  bool IsFallbackRequested() { return callback_options_.fallback; }

  void SetUp() override { callback_options_.fallback = false; }

  v8::FastApiCallbackOptions* callback_options() { return &callback_options_; }

 private:
  v8::FastApiCallbackOptions callback_options_ = {false, {0}};
};

TEST_F(NoAllocDirectCallHostTest, ActionsExecutedImmediatelyWhenAllocAllowed) {
  NoAllocDirectCallHost host;
  ASSERT_FALSE(host.IsInFastMode());
  bool change_me = false;
  host.PostDeferrableAction(WTF::BindOnce(
      [](bool* change_me) { *change_me = true; }, WTF::Unretained(&change_me)));
  ASSERT_TRUE(change_me);
  ASSERT_FALSE(host.HasDeferredActions());
  ASSERT_FALSE(IsFallbackRequested());
}

TEST_F(NoAllocDirectCallHostTest, ActionsDeferredWhenAllocDisallowed) {
  NoAllocDirectCallHost host;
  bool change_me = false;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    ASSERT_TRUE(host.IsInFastMode());
    host.PostDeferrableAction(
        WTF::BindOnce([](bool* change_me) { *change_me = true; },
                      WTF::Unretained(&change_me)));
  }
  ASSERT_FALSE(host.IsInFastMode());
  ASSERT_FALSE(change_me);
  ASSERT_TRUE(IsFallbackRequested());
  ASSERT_TRUE(host.HasDeferredActions());
}

TEST_F(NoAllocDirectCallHostTest, FlushDeferredActions) {
  NoAllocDirectCallHost host;
  bool change_me = false;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    host.PostDeferrableAction(
        WTF::BindOnce([](bool* change_me) { *change_me = true; },
                      WTF::Unretained(&change_me)));
  }
  ASSERT_TRUE(IsFallbackRequested());
  if (host.HasDeferredActions()) {
    host.FlushDeferredActions();
  }
  ASSERT_TRUE(change_me);
  ASSERT_FALSE(host.HasDeferredActions());
}

TEST_F(NoAllocDirectCallHostTest, ThrowDOMException) {
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError, "baz");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    ASSERT_EQ(no_alloc_exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kInvalidAccessError);
  }
  v8::TryCatch try_catch(test_scope.GetIsolate());
  host.FlushDeferredActions();
  ASSERT_TRUE(try_catch.HasCaught());
}

TEST_F(NoAllocDirectCallHostTest, ThrowTypeError) {
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowTypeError("baz");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    ASSERT_EQ(no_alloc_exception_state.CodeAs<ESErrorType>(),
              ESErrorType::kTypeError);
  }
  v8::TryCatch try_catch(test_scope.GetIsolate());
  host.FlushDeferredActions();
  ASSERT_TRUE(try_catch.HasCaught());
}

TEST_F(NoAllocDirectCallHostTest, ThrowSecurityError) {
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowSecurityError("baz", "bam");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    ASSERT_EQ(no_alloc_exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kSecurityError);
  }
  v8::TryCatch try_catch(test_scope.GetIsolate());
  host.FlushDeferredActions();
  ASSERT_TRUE(try_catch.HasCaught());
}

TEST_F(NoAllocDirectCallHostTest, ThrowRangeError) {
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowRangeError("baz");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    ASSERT_EQ(no_alloc_exception_state.CodeAs<ESErrorType>(),
              ESErrorType::kRangeError);
  }
  v8::TryCatch try_catch(test_scope.GetIsolate());
  host.FlushDeferredActions();
  ASSERT_TRUE(try_catch.HasCaught());
}

TEST_F(NoAllocDirectCallHostTest, MultipleExceptions) {
  // Checks that the second exception clobbers the first one.
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowRangeError("baz");
    no_alloc_exception_state.ThrowTypeError("boo");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    ASSERT_EQ(no_alloc_exception_state.CodeAs<ESErrorType>(),
              ESErrorType::kTypeError);
  }
  v8::TryCatch try_catch(test_scope.GetIsolate());
  host.FlushDeferredActions();
  ASSERT_TRUE(try_catch.HasCaught());
}

TEST_F(NoAllocDirectCallHostTest, ClearException) {
  V8TestingScope test_scope;
  NoAllocDirectCallHost host;
  {
    NoAllocDirectCallScope scope(&host, callback_options());
    NoAllocDirectCallExceptionState no_alloc_exception_state(
        &host, test_scope.GetIsolate(), ExceptionContextType::kOperationInvoke,
        "foo", "bar");
    ASSERT_FALSE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ThrowRangeError("baz");
    ASSERT_TRUE(no_alloc_exception_state.HadException());
    no_alloc_exception_state.ClearException();
    ASSERT_FALSE(no_alloc_exception_state.HadException());
  }
  ASSERT_FALSE(IsFallbackRequested());
  ASSERT_FALSE(host.HasDeferredActions());
}

}  // namespace blink
