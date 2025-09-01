// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_test_util.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;

MATCHER_P2(HasException,
           error_name,
           expected_message,
           base::StrCat({"has ", negation ? "no " : "", error_name, "('",
                         expected_message, "')"})) {
  const DummyExceptionStateForTesting& exception_state = arg;
  if (!exception_state.HadException()) {
    *result_listener << "no exception";
    return false;
  }

  bool type_matches = false;
  const std::string_view err(error_name);
  if (err == "RangeError") {
    type_matches =
        exception_state.CodeAs<ESErrorType>() == ESErrorType::kRangeError;
  } else if (err == "AbortError") {
    type_matches = exception_state.CodeAs<DOMExceptionCode>() ==
                   DOMExceptionCode::kAbortError;
  } else if (err == "TypeError") {
    type_matches =
        exception_state.CodeAs<ESErrorType>() == ESErrorType::kTypeError;
  } else if (err == "SecurityError") {
    type_matches = exception_state.CodeAs<DOMExceptionCode>() ==
                   DOMExceptionCode::kSecurityError;
  } else {
    *result_listener << "unsupported error name in matcher: " << error_name;
    return false;
  }

  if (!type_matches) {
    *result_listener << "exception is not " << error_name;
    return false;
  }

  if (exception_state.Message() != expected_message) {
    *result_listener << "unexpected message from " << error_name << ": "
                     << exception_state.Message();
    return false;
  }
  return true;
}

MATCHER_P(MatchNetworkResourceRequest,
          expected,
          base::StrCat({"does ", negation ? "not " : "",
                        "match given network::ResourceRequest"})) {
  const network::ResourceRequest& src = arg;
  if (src.url != expected.url) {
    *result_listener << "mismatched URL: " << src.url;
    return false;
  }
  if (src.request_initiator != expected.request_initiator) {
    *result_listener << "mismatched request_initiator: "
                     << *src.request_initiator;
    return false;
  }
  if (src.referrer != expected.referrer) {
    *result_listener << "mismatched referrer: " << src.referrer;
    return false;
  }
  if (src.referrer_policy != expected.referrer_policy) {
    *result_listener << "mismatched referrer_policy";
    return false;
  }
  if (src.priority != expected.priority) {
    *result_listener << "mismatched priority: " << src.priority;
    return false;
  }
  if (src.priority_incremental != expected.priority_incremental) {
    *result_listener << "mismatched priority_incremental: "
                     << src.priority_incremental;
    return false;
  }
  if (src.cors_preflight_policy != expected.cors_preflight_policy) {
    *result_listener << "mismatched cors_preflight_policy: "
                     << src.cors_preflight_policy;
    return false;
  }
  if (src.mode != expected.mode) {
    *result_listener << "mismatched mode: " << src.mode;
    return false;
  }
  if (src.destination != expected.destination) {
    *result_listener << "mismatched destination: " << src.destination;
    return false;
  }
  if (src.credentials_mode != expected.credentials_mode) {
    *result_listener << "mismatched credentials_mode: " << src.credentials_mode;
    return false;
  }
  if (src.redirect_mode != expected.redirect_mode) {
    *result_listener << "mismatched redirect_mode: " << src.redirect_mode;
    return false;
  }
  if (src.fetch_integrity != expected.fetch_integrity) {
    *result_listener << "mismatched fetch_integrity: " << src.fetch_integrity;
    return false;
  }
  if (src.web_bundle_token_params.has_value()) {
    *result_listener << "unexpected web_bundle_token_params: must not be set";
    return false;
  }
  if (src.is_fetch_like_api != expected.is_fetch_like_api) {
    *result_listener << "unexpected is_fetch_like_api: "
                     << src.is_fetch_like_api;
    return false;
  }
  if (src.is_fetch_later_api != expected.is_fetch_later_api) {
    *result_listener << "unexpected is_fetch_later_api: "
                     << src.is_fetch_later_api;
    return false;
  }
  if (src.keepalive != expected.keepalive) {
    *result_listener << "unexpected keepalive: " << src.keepalive;
    return false;
  }
  if (src.fetch_window_id != expected.fetch_window_id) {
    *result_listener << "unexpected fetch_window_id: " << *src.fetch_window_id;
    return false;
  }
  if (src.is_favicon != expected.is_favicon) {
    *result_listener << "unexpected is_favicon: " << src.is_favicon;
    return false;
  }
  return true;
}

}  // namespace

class FetchLaterTestBase : public testing::Test {
 public:
  FetchLaterTestBase()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
    feature_list_.InitAndEnableFeature(blink::features::kFetchLaterAPI);
  }

  // FetchLater only supports secure context.
  static const String GetSourcePageURL() {
    return AtomicString("https://example.com");
  }

 protected:
  void SetUp() override {
    frame_client_ = MakeGarbageCollected<FakeLocalFrameClient>();
    frame_client_->GetLoaderFactoryBundle()->SetFetchLaterLoaderFactory(
        factory_.BindNewEndpointAndPassDedicatedRemote());
  }
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  Request* CreateFetchLaterRequest(V8TestingScope& scope,
                                   const String& url,
                                   AbortSignal* signal) const {
    auto* request_init = RequestInit::Create();
    request_init->setMethod("GET");
    request_init->setSignal(signal);
    auto* request = Request::Create(scope.GetScriptState(), url, request_init,
                                    scope.GetExceptionState());

    return request;
  }

  std::unique_ptr<network::ResourceRequest> CreateNetworkResourceRequest(
      const KURL& url) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GURL(url);
    request->request_initiator =
        SecurityOrigin::Create(KURL(GetSourcePageURL()))->ToUrlOrigin();
    request->referrer = WebStringToGURL(GetSourcePageURL());
    request->referrer_policy = network::ReferrerPolicyForUrlRequest(
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
    request->priority =
        WebURLRequest::ConvertToNetPriority(WebURLRequest::Priority::kHigh);
    request->mode = network::mojom::RequestMode::kCors;
    request->destination = network::mojom::RequestDestination::kEmpty;
    request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
    request->redirect_mode = network::mojom::RedirectMode::kFollow;
    request->is_fetch_like_api = true;
    request->is_fetch_later_api = true;
    request->keepalive = true;
    request->is_favicon = false;
    return request;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> TaskRunner() {
    return task_runner_;
  }

  FakeLocalFrameClient* FrameClient() { return frame_client_; }

  MockFetchLaterLoaderFactory& Factory() { return factory_; }

  const base::HistogramTester& Histogram() const { return histogram_; }

 private:
  test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  Persistent<FakeLocalFrameClient> frame_client_;
  MockFetchLaterLoaderFactory factory_;
  base::HistogramTester histogram_;
};

class FetchLaterTest : public FetchLaterTestBase {};

// A FetchLater request where its URL has same-origin as its execution context.
TEST_F(FetchLaterTest, CreateSameOriginFetchLaterRequest) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());

  EXPECT_CALL(
      Factory(),
      CreateLoader(_, _, _,
                   MatchNetworkResourceRequest(*CreateNetworkResourceRequest(
                       KURL(GetSourcePageURL() + target_url))),
                   _))
      .Times(1)
      .RetiresOnSaturation();
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), std::nullopt, exception_state);
  Factory().FlushForTesting();

  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 1u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 1);
}

TEST_F(FetchLaterTest, NegativeActivateAfterThrowRangeError) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());

  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), /*activate_after=*/std::make_optional(-1),
      exception_state);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(exception_state,
              HasException("RangeError",
                           "fetchLater's activateAfter cannot be negative."));
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 0);
}

// Test to cover when a FetchLaterManager::FetchLater() call is provided with an
// AbortSignal that has been aborted.
TEST_F(FetchLaterTest, AbortBeforeFetchLater) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Simulates FetchLater aborted by abort signal first.
  controller->abort(scope.GetScriptState());
  // Sets up a FetchLater request.
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), /*activate_after_ms=*/std::nullopt, exception_state);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(
      exception_state,
      HasException("AbortError", "The user aborted a fetchLater request."));
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 0);
}

// Test to cover when a FetchLaterManager::FetchLater() is aborted after being
// called.
TEST_F(FetchLaterTest, AbortAfterFetchLater) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());

  // Sets up a FetchLater request.
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), /*activate_after_ms=*/std::nullopt, exception_state);
  EXPECT_THAT(result, Not(IsNull()));

  // Simulates FetchLater aborted by abort signal.
  controller->abort(scope.GetScriptState());

  // Even aborted, the FetchLaterResult held by user should still exist.
  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 1);
  Histogram().ExpectUniqueSample("FetchLater.Renderer.Metrics",
                                 0 /*kAbortedByUser*/, 1);
}

// Test to cover a FetchLaterManager::FetchLater() with custom activateAfter.
TEST_F(FetchLaterTest, ActivateAfter) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  DOMHighResTimeStamp activate_after_ms = 3000;
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Sets up a FetchLater request.
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), std::make_optional(activate_after_ms),
      exception_state);
  EXPECT_THAT(result, Not(IsNull()));
  fetch_later_manager->RecreateTimerForTesting(
      TaskRunner(), TaskRunner()->GetMockTickClock());

  // Triggers timer's callback by fast forwarding.
  TaskRunner()->FastForwardBy(base::Milliseconds(activate_after_ms * 2));

  EXPECT_FALSE(exception_state.HadException());
  // The FetchLaterResult held by user should still exist.
  EXPECT_THAT(result, Not(IsNull()));
  // The loader should have been activated and removed.
  EXPECT_TRUE(result->activated());
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 1);
  Histogram().ExpectUniqueSample("FetchLater.Renderer.Metrics",
                                 2 /*kActivatedByTimeout*/, 1);
}

// Test to cover when a FetchLaterManager::FetchLater()'s execution context is
// destroyed.
TEST_F(FetchLaterTest, ContextDestroyed) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Sets up a FetchLater request.
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(),
      /*activate_after_ms=*/std::nullopt, exception_state);

  // Simulates destroying execution context.
  fetch_later_manager->ContextDestroyed();

  // The FetchLaterResult held by user should still exist.
  EXPECT_THAT(result, Not(IsNull()));
  // The loader should have been activated and removed.
  EXPECT_TRUE(result->activated());
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 1);
  Histogram().ExpectUniqueSample("FetchLater.Renderer.Metrics",
                                 1 /*kContextDestroyed*/, 1);
}

// Test to cover when a FetchLaterManager::DeferredLoader triggers its Process()
// method when its context enters BackForwardCache with BackgroundSync
// permission off.
TEST_F(FetchLaterTest, ForcedSendingWithBackgroundSyncOff) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());
  auto& exception_state = scope.GetExceptionState();
  auto target_url = AtomicString("/");
  url_test_helpers::RegisterMockedURLLoad(KURL(GetSourcePageURL() + target_url),
                                          test::CoreTestDataPath("foo.html"),
                                          "text/html");
  auto* fetch_later_manager =
      MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* request =
      CreateFetchLaterRequest(scope, target_url, controller->signal());
  // Sets up a FetchLater request.
  auto* result = fetch_later_manager->FetchLater(
      scope.GetScriptState(),
      request->PassRequestData(scope.GetScriptState(), exception_state),
      request->signal(), /*activate_after=*/std::nullopt, exception_state);
  EXPECT_THAT(result, Not(IsNull()));

  // Simluates the context enters BackForwardCache.
  // The default BackgroundSync is DENIED, so the following call should trigger
  // immediate sending.
  fetch_later_manager->ContextEnteredBackForwardCache();

  // The FetchLaterResult held by user should still exist.
  EXPECT_THAT(result, Not(IsNull()));
  // The FetchLater sending is triggered, so its state should be updated.
  EXPECT_TRUE(result->activated());
  EXPECT_FALSE(exception_state.HadException());
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 1);
  Histogram().ExpectUniqueSample("FetchLater.Renderer.Metrics",
                                 3 /*kActivatedOnEnteredBackForwardCache*/, 1);
}

// Base class for fetchLater() URL validation tests.
class FetchLaterUrlTestBase : public FetchLaterTestBase {
 protected:
  std::pair<Persistent<FetchLaterManager>, Persistent<FetchLaterResult>>
  CallFetchLater(V8TestingScope& scope, const std::string& url) {
    auto* manager =
        MakeGarbageCollected<FetchLaterManager>(scope.GetExecutionContext());
    auto* controller = AbortController::Create(scope.GetScriptState());
    auto& exception_state = scope.GetExceptionState();
    auto* request = CreateFetchLaterRequest(scope, String::FromUTF8(url),
                                            controller->signal());
    auto* result = manager->FetchLater(
        scope.GetScriptState(),
        request->PassRequestData(scope.GetScriptState(), exception_state),
        controller->signal(), /*activate_after=*/std::nullopt, exception_state);
    return {manager, result};
  }
};

struct UrlTestParam {
  const std::string test_name;
  const std::string url;
};

// This test verifies that fetchLater() only accepts URLs with HTTP or HTTPS
// schemes. It covers various valid and invalid URL schemes, including http(s),
// localhost, IP addresses, and non-http schemes like data:, file:, etc.
class FetchLaterWithValidUrlTest
    : public FetchLaterUrlTestBase,
      public testing::WithParamInterface<UrlTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         FetchLaterWithValidUrlTest,
                         testing::ValuesIn(std::vector<UrlTestParam>{
                             {"https_example", "https://example.com/"},
                             {"http_localhost", "http://localhost/"},
                             {"https_localhost", "https://localhost/"},
                             {"http_127_0_0_1", "http://127.0.0.1/"},
                             {"https_127_0_0_1", "https://127.0.0.1/"},
                             {"http_ipv6_localhost", "http://[::1]/"},
                             {"https_ipv6_localhost", "https://[::1]/"},
                         }),
                         [](const testing::TestParamInfo<UrlTestParam>& info) {
                           return info.param.test_name;
                         });

// Verifies that fetchLater() succeeds with valid URLs, including HTTPS URLs and
// potentially trustworthy HTTP URLs like localhost.
TEST_P(FetchLaterWithValidUrlTest, Succeeds) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());

  auto [manager, result] = CallFetchLater(scope, GetParam().url);

  EXPECT_THAT(result, Not(IsNull()));
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(manager->NumLoadersForTesting(), 1u);
}

class FetchLaterWithInsecureUrlTest
    : public FetchLaterUrlTestBase,
      public testing::WithParamInterface<UrlTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         FetchLaterWithInsecureUrlTest,
                         testing::ValuesIn(std::vector<UrlTestParam>{
                             {"http_example", "http://example.com/"},
                         }),
                         [](const testing::TestParamInfo<UrlTestParam>& info) {
                           return info.param.test_name;
                         });

// Verifies that fetchLater() throws a SecurityError for insecure URLs, such as
// an HTTP URL that is not localhost.
TEST_P(FetchLaterWithInsecureUrlTest, FailsWithSecurityError) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());

  auto [manager, result] = CallFetchLater(scope, GetParam().url);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(
      scope.GetExceptionState(),
      HasException("SecurityError", "fetchLater was passed an insecure URL."));
  EXPECT_EQ(manager->NumLoadersForTesting(), 0u);
}

class FetchLaterWithInvalidSchemeUrlTest
    : public FetchLaterUrlTestBase,
      public testing::WithParamInterface<UrlTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         FetchLaterWithInvalidSchemeUrlTest,
                         testing::ValuesIn(std::vector<UrlTestParam>{
                             {"data", "data:text/plain,Hello"},
                             {"file", "file:///etc/passwd"},
                             {"ftp", "ftp://example.com/"},
                             {"javascript", "javascript:alert(1)"},
                             {"blob", "blob:https://example.com/some-uuid"},
                         }),
                         [](const testing::TestParamInfo<UrlTestParam>& info) {
                           return info.param.test_name;
                         });

// Verifies that fetchLater() throws a TypeError for URLs with schemes other
// than HTTP or HTTPS.
TEST_P(FetchLaterWithInvalidSchemeUrlTest, FailsWithTypeError) {
  FetchLaterTestingScope scope(FrameClient(), GetSourcePageURL());

  auto [manager, result] = CallFetchLater(scope, GetParam().url);

  EXPECT_THAT(result, IsNull());
  EXPECT_THAT(
      scope.GetExceptionState(),
      HasException("TypeError", "fetchLater is only supported over HTTP(S)."));
  EXPECT_EQ(manager->NumLoadersForTesting(), 0u);
}

}  // namespace blink
