// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using ::testing::IsNull;
using ::testing::Not;

MATCHER_P(HasRangeError,
          expected_message,
          base::StrCat({"has ", negation ? "no " : "", "RangeError('",
                        expected_message, "')"})) {
  const ExceptionState& exception_state = arg;
  if (!exception_state.HadException()) {
    *result_listener << "no exception";
    return false;
  }
  if (exception_state.CodeAs<ESErrorType>() != ESErrorType::kRangeError) {
    *result_listener << "exception is not RangeError";
    return false;
  }
  if (exception_state.Message() != expected_message) {
    *result_listener << "unexpected message from RangeError: "
                     << exception_state.Message();
    return false;
  }
  return true;
}

MATCHER_P(HasAbortError,
          expected_message,
          base::StrCat({"has ", negation ? "no " : "", "AbortError('",
                        expected_message, "')"})) {
  const ExceptionState& exception_state = arg;
  if (!exception_state.HadException()) {
    *result_listener << "no exception";
    return false;
  }
  if (exception_state.CodeAs<DOMExceptionCode>() !=
      DOMExceptionCode::kAbortError) {
    *result_listener << "exception is not AbortError";
    return false;
  }
  if (exception_state.Message() != expected_message) {
    *result_listener << "unexpected message from AbortError: "
                     << exception_state.Message();
    return false;
  }
  return true;
}

}  // namespace

class FetchLaterTest : public testing::Test {
 public:
  // FetchLater only supports secure context.
  static const WTF::String GetSourcePageURL() {
    return AtomicString("https://example.com");
  }

 protected:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  Request* CreateFetchLaterRequest(V8TestingScope& scope,
                                   const WTF::String& url,
                                   AbortSignal* signal) const {
    auto* request_init = RequestInit::Create();
    request_init->setMethod("GET");
    request_init->setSignal(signal);
    auto* request = Request::Create(scope.GetScriptState(), url, request_init,
                                    scope.GetExceptionState());

    return request;
  }
};

class FetchLaterTestingScope : public V8TestingScope {
  STACK_ALLOCATED();

 public:
  FetchLaterTestingScope()
      : V8TestingScope(KURL(FetchLaterTest::GetSourcePageURL())) {}
};

TEST_F(FetchLaterTest, CreateSameOriginFetchLaterRequest) {
  FetchLaterTestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_manager =
      MakeGarbageCollected<FetchManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());

  auto* result = fetch_manager->FetchLater(
      scope.GetScriptState(), request->PassRequestData(scope.GetScriptState()),
      request->signal(), absl::nullopt, exception_state);

  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(FetchLaterTest, NegativeActivationTimeoutThrowRangeError) {
  FetchLaterTestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_manager =
      MakeGarbageCollected<FetchManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());

  auto* result = fetch_manager->FetchLater(
      scope.GetScriptState(), request->PassRequestData(scope.GetScriptState()),
      request->signal(), /*activation_timeout=*/absl::make_optional(-1),
      exception_state);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(
      exception_state,
      HasRangeError("fetchLater's activationTimeout cannot be negative."));
}

// Test to cover when a FetchManager::FetchLater() call is provided with an
// AbortSignal that has been aborted.
TEST_F(FetchLaterTest, AbortBeforeFetchLater) {
  FetchLaterTestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_manager =
      MakeGarbageCollected<FetchManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Simulates FetchLater aborted by abort signal first.
  controller->abort(scope.GetScriptState());
  // Sets up a FetchLater request.
  auto* result = fetch_manager->FetchLater(
      scope.GetScriptState(), request->PassRequestData(scope.GetScriptState()),
      request->signal(), /*background_timeout=*/absl::nullopt, exception_state);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(exception_state,
              HasAbortError("The user aborted a fetchLater request."));
}

// Test to cover when a FetchManager::FetchLater() is aborted after being
// called.
TEST_F(FetchLaterTest, AbortAfterFetchLater) {
  FetchLaterTestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_manager =
      MakeGarbageCollected<FetchManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Sets up a FetchLater request.
  auto* result = fetch_manager->FetchLater(
      scope.GetScriptState(), request->PassRequestData(scope.GetScriptState()),
      request->signal(), /*background_timeout=*/absl::nullopt, exception_state);
  EXPECT_THAT(result, Not(IsNull()));

  // Simulates FetchLater aborted by abort signal.
  controller->abort(scope.GetScriptState());

  // Even aborted, the FetchLaterResult held by user should still exist.
  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
