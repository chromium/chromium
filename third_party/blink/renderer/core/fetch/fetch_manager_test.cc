// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
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
  ExceptionState& exception_state = scope.GetExceptionState();
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
      request->signal(), scope.GetExceptionState());

  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
