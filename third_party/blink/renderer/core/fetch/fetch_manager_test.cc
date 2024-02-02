// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"

#include <optional>

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

class MockFetchLaterLoaderFactory
    : public blink::mojom::FetchLaterLoaderFactory {
 public:
  MockFetchLaterLoaderFactory() = default;

  mojo::PendingAssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
  BindNewEndpointAndPassDedicatedRemote() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // blink::mojom::FetchLaterLoaderFactory overrides:
  MOCK_METHOD(void,
              CreateLoader,
              (mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoader>,
               int32_t,
               uint32_t,
               const network::ResourceRequest&,
               const net::MutableNetworkTrafficAnnotationTag&),
              (override));
  MOCK_METHOD(
      void,
      Clone,
      (mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>),
      (override));

 private:
  mojo::AssociatedReceiver<blink::mojom::FetchLaterLoaderFactory> receiver_{
      this};
};

// A fake LocalFrameClient that provides non-null ChildURLLoaderFactoryBundle.
class FakeLocalFrameClient : public EmptyLocalFrameClient {
 public:
  FakeLocalFrameClient()
      : loader_factory_bundle_(
            base::MakeRefCounted<blink::ChildURLLoaderFactoryBundle>()) {}

  // EmptyLocalFrameClient overrides:
  blink::ChildURLLoaderFactoryBundle* GetLoaderFactoryBundle() override {
    return loader_factory_bundle_.get();
  }

 private:
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> loader_factory_bundle_;
};

}  // namespace

class FetchLaterTest : public testing::Test {
 public:
  FetchLaterTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
    feature_list_.InitAndEnableFeature(blink::features::kFetchLaterAPI);
  }

  // FetchLater only supports secure context.
  static const WTF::String GetSourcePageURL() {
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
                                   const WTF::String& url,
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

class FetchLaterTestingScope : public V8TestingScope {
  STACK_ALLOCATED();

 public:
  explicit FetchLaterTestingScope(LocalFrameClient* frame_client)
      : V8TestingScope(DummyPageHolder::CreateAndCommitNavigation(
            KURL(FetchLaterTest::GetSourcePageURL()),
            /*initial_view_size=*/gfx::Size(),
            /*chrome_client=*/nullptr,
            frame_client)) {}
};

// A FetchLater request where its URL has same-origin as its execution context.
TEST_F(FetchLaterTest, CreateSameOriginFetchLaterRequest) {
  FetchLaterTestingScope scope(FrameClient());
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
  FetchLaterTestingScope scope(FrameClient());
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
              HasRangeError("fetchLater's activateAfter cannot be negative."));
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 0);
}

// Test to cover when a FetchLaterManager::FetchLater() call is provided with an
// AbortSignal that has been aborted.
TEST_F(FetchLaterTest, AbortBeforeFetchLater) {
  FetchLaterTestingScope scope(FrameClient());
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
  EXPECT_THAT(exception_state,
              HasAbortError("The user aborted a fetchLater request."));
  EXPECT_EQ(fetch_later_manager->NumLoadersForTesting(), 0u);
  Histogram().ExpectTotalCount("FetchLater.Renderer.Total", 0);
}

// Test to cover when a FetchLaterManager::FetchLater() is aborted after being
// called.
TEST_F(FetchLaterTest, AbortAfterFetchLater) {
  FetchLaterTestingScope scope(FrameClient());
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
  FetchLaterTestingScope scope(FrameClient());
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
  FetchLaterTestingScope scope(FrameClient());
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
  FetchLaterTestingScope scope(FrameClient());
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

// The default priority for FetchLater request without FetchPriorityHint or
// RenderBlockingBehavior should be kHigh.
TEST(FetchLaterLoadPriorityTest, DefaultHigh) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(
      KURL(FetchLaterTest::GetSourcePageURL() + "/fetch-later"));
  FetchParameters params(std::move(request), options);

  auto computed_priority =
      FetchLaterManager::ComputeLoadPriorityForTesting(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

// The priority for FetchLater request with FetchPriorityHint::kAuto should be
// kHigh.
TEST(FetchLaterLoadPriorityTest, WithFetchPriorityHintAuto) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(
      KURL(FetchLaterTest::GetSourcePageURL() + "/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kAuto);
  FetchParameters params(std::move(request), options);

  auto computed_priority =
      FetchLaterManager::ComputeLoadPriorityForTesting(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

// The priority for FetchLater request with FetchPriorityHint::kLow should be
// kLow.
TEST(FetchLaterLoadPriorityTest, WithFetchPriorityHintLow) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(
      KURL(FetchLaterTest::GetSourcePageURL() + "/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kLow);
  FetchParameters params(std::move(request), options);

  auto computed_priority =
      FetchLaterManager::ComputeLoadPriorityForTesting(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kLow);
}

// The priority for FetchLater request with RenderBlockingBehavior::kBlocking
// should be kHigh.
TEST(FetchLaterLoadPriorityTest,
     WithFetchPriorityHintLowAndRenderBlockingBehaviorBlocking) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(
      KURL(FetchLaterTest::GetSourcePageURL() + "/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kLow);
  FetchParameters params(std::move(request), options);
  params.SetRenderBlockingBehavior(RenderBlockingBehavior::kBlocking);

  auto computed_priority =
      FetchLaterManager::ComputeLoadPriorityForTesting(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

}  // namespace blink
