// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"

#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_presentation_availability.h"
#include "third_party/blink/renderer/modules/presentation/mock_presentation_service.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::_;

namespace blink {

using mojom::blink::ScreenAvailability;

class MockPresentationAvailabilityObserver
    : public GarbageCollected<MockPresentationAvailabilityObserver>,
      public PresentationAvailabilityObserver {
 public:
  explicit MockPresentationAvailabilityObserver(const Vector<KURL>& urls)
      : urls_(urls) {}
  ~MockPresentationAvailabilityObserver() override = default;

  MOCK_METHOD1(AvailabilityChanged, void(ScreenAvailability availability));
  const Vector<KURL>& Urls() const override { return urls_; }

 private:
  const Vector<KURL> urls_;
};

// Helper classes for WaitForPromise{Fulfillment,Rejection}(). Provides a
// function that invokes |callback| when a ScriptPromise is resolved/rejected.
class ClosureOnResolve final
    : public ThenCallable<PresentationAvailability, ClosureOnResolve> {
 public:
  explicit ClosureOnResolve(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  void React(ScriptState*, PresentationAvailability*) {
    CHECK(callback_);
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

class ClosureOnReject final : public ThenCallable<IDLAny, ClosureOnReject> {
 public:
  explicit ClosureOnReject(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  void React(ScriptState*, ScriptValue) {
    CHECK(callback_);
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

class PresentationAvailabilityStateTestingContext final {
  STACK_ALLOCATED();

 public:
  PresentationAvailabilityStateTestingContext() = default;
  ~PresentationAvailabilityStateTestingContext() = default;

  ScriptState* GetScriptState() { return testing_scope_.GetScriptState(); }

  const ExceptionContext& GetExceptionContext() {
    return testing_scope_.GetExceptionState().GetContext();
  }

  // Synchronously waits for |promise| to be fulfilled.
  void WaitForPromiseFulfillment(
      ScriptPromise<PresentationAvailability> promise) {
    base::RunLoop run_loop;
    promise.React(GetScriptState(), MakeGarbageCollected<ClosureOnResolve>(
                                        run_loop.QuitClosure()));
    // Execute pending microtasks, otherwise it can take a few seconds for the
    // promise to resolve.
    GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        GetScriptState()->GetIsolate());
    run_loop.Run();
  }

  // Synchronously waits for |promise| to be rejected.
  void WaitForPromiseRejection(
      ScriptPromise<PresentationAvailability> promise) {
    base::RunLoop run_loop;
    promise.Catch(GetScriptState(), MakeGarbageCollected<ClosureOnReject>(
                                        run_loop.QuitClosure()));
    // Execute pending microtasks, otherwise it can take a few seconds for the
    // promise to resolve.
    GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        GetScriptState()->GetIsolate());
    run_loop.Run();
  }

  PresentationAvailability* GetPromiseResolutionAsPresentationAvailability(
      const ScriptPromise<PresentationAvailability>& promise) {
    return V8PresentationAvailability::ToWrappable(
        GetScriptState()->GetIsolate(), promise.V8Promise()->Result());
  }

 private:
  V8TestingScope testing_scope_;
};

class PresentationAvailabilityStateTest : public testing::Test {
 public:
  PresentationAvailabilityStateTest()
      : url1_(KURL("https://www.example.com/1.html")),
        url2_(KURL("https://www.example.com/2.html")),
        url3_(KURL("https://www.example.com/3.html")),
        url4_(KURL("https://www.example.com/4.html")),
        urls_({url1_, url2_, url3_, url4_}),
        mock_observer_all_urls_(
            MakeGarbageCollected<MockPresentationAvailabilityObserver>(urls_)),
        mock_observer1_(
            MakeGarbageCollected<MockPresentationAvailabilityObserver>(
                Vector<KURL>({url1_, url2_, url3_}))),
        mock_observer2_(
            MakeGarbageCollected<MockPresentationAvailabilityObserver>(
                Vector<KURL>({url2_, url3_, url4_}))),
        mock_observer3_(
            MakeGarbageCollected<MockPresentationAvailabilityObserver>(
                Vector<KURL>({url2_, url3_}))),
        mock_observers_({mock_observer1_, mock_observer2_, mock_observer3_}),
        mock_presentation_service_(),
        state_(MakeGarbageCollected<PresentationAvailabilityState>(
            &mock_presentation_service_)) {}

  ~PresentationAvailabilityStateTest() override = default;

  void ChangeURLState(const KURL& url, ScreenAvailability state) {
    if (state != ScreenAvailability::UNKNOWN) {
      state_->UpdateAvailability(url, state);
    }
  }

  void RequestAvailabilityAndAddObservers(
      ScriptPromiseResolver<PresentationAvailability>* resolver) {
    for (auto& mock_observer : mock_observers_) {
      state_->RequestAvailability(mock_observer->Urls(), resolver);
      state_->AddObserver(mock_observer);
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // `urls` change state to `states`. This function takes ownership of
  // `promise`.
  void TestRequestAvailability(
      const Vector<KURL>& urls,
      const Vector<ScreenAvailability>& states,
      ScriptPromiseResolver<PresentationAvailability>* resolver) {
    DCHECK_EQ(urls.size(), states.size());

    for (const auto& url : urls) {
      EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
          .Times(1);
      EXPECT_CALL(mock_presentation_service_,
                  StopListeningForScreenAvailability(url))
          .Times(1);
    }

    state_->RequestAvailability(urls, resolver);
    for (wtf_size_t i = 0; i < urls.size(); i++) {
      ChangeURLState(urls[i], states[i]);
    }
  }

 protected:
  const KURL url1_;
  const KURL url2_;
  const KURL url3_;
  const KURL url4_;
  const Vector<KURL> urls_;
  test::TaskEnvironment task_environment_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer_all_urls_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer1_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer2_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer3_;
  Vector<Persistent<MockPresentationAvailabilityObserver>> mock_observers_;

  MockPresentationService mock_presentation_service_;
  Persistent<PresentationAvailabilityState> state_;
};

TEST_F(PresentationAvailabilityStateTest, RequestAvailability) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }

  state_->RequestAvailability(
      urls_,
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext()));
  state_->UpdateAvailability(url1_, ScreenAvailability::AVAILABLE);

  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
  }

  state_->AddObserver(mock_observer_all_urls_);

  EXPECT_CALL(*mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  state_->UpdateAvailability(url1_, ScreenAvailability::UNAVAILABLE);
  EXPECT_CALL(*mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));
  state_->UpdateAvailability(url1_, ScreenAvailability::AVAILABLE);
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }
  state_->RemoveObserver(mock_observer_all_urls_);

  // After RemoveObserver(), |mock_observer_all_urls_| should no longer be
  // notified.
  EXPECT_CALL(*mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE))
      .Times(0);
  state_->UpdateAvailability(url1_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       ScreenAvailabilitySourceNotSupported) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
  }

  state_->AddObserver(mock_observer_all_urls_);

  EXPECT_CALL(*mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  state_->UpdateAvailability(url1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);

  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }
  state_->RemoveObserver(mock_observer_all_urls_);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlNoAvailabilityChange) {
  PresentationAvailabilityStateTestingContext context;
  EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url1_))
      .Times(1);

  state_->RequestAvailability(
      Vector<KURL>({url1_}),
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext()));
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesAvailable) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability({url1_}, {ScreenAvailability::AVAILABLE}, resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_TRUE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesNotCompatible) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability({url1_}, {ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_FALSE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnavailable) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability({url1_}, {ScreenAvailability::UNAVAILABLE}, resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_FALSE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnsupported) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability({url1_}, {ScreenAvailability::DISABLED}, resolver);
  context.WaitForPromiseRejection(promise);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesAvailable) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::AVAILABLE, ScreenAvailability::AVAILABLE}, resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_TRUE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnavailable) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::UNAVAILABLE, ScreenAvailability::UNAVAILABLE},
      resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_FALSE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesNotCompatible) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability({url1_, url2_},
                          {ScreenAvailability::SOURCE_NOT_SUPPORTED,
                           ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* presentation_availability =
      context.GetPromiseResolutionAsPresentationAvailability(promise);
  EXPECT_FALSE(presentation_availability->value());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnsupported) {
  PresentationAvailabilityStateTestingContext context;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise = resolver->Promise();

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::DISABLED, ScreenAvailability::DISABLED}, resolver);
  context.WaitForPromiseRejection(promise);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  PresentationAvailabilityStateTestingContext context;
  // First getAvailability() call.
  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise1 = resolver1->Promise();

  Vector<ScreenAvailability> state_seq = {ScreenAvailability::UNAVAILABLE,
                                          ScreenAvailability::AVAILABLE,
                                          ScreenAvailability::UNAVAILABLE};
  TestRequestAvailability({url1_, url2_, url3_}, state_seq, resolver1);
  context.WaitForPromiseFulfillment(promise1);
  auto* presentation_availability1 =
      context.GetPromiseResolutionAsPresentationAvailability(promise1);
  EXPECT_FALSE(presentation_availability1->value());

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_->Urls()) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  auto promise2 = resolver2->Promise();
  state_->RequestAvailability(mock_observer3_->Urls(), resolver2);
  context.WaitForPromiseFulfillment(promise2);
  auto* presentation_availability2 =
      context.GetPromiseResolutionAsPresentationAvailability(promise2);
  EXPECT_TRUE(presentation_availability2->value());
}

TEST_F(PresentationAvailabilityStateTest, StartListeningListenToEachURLOnce) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);
}

TEST_F(PresentationAvailabilityStateTest, StopListeningListenToEachURLOnce) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(*mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(*mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);

  for (auto& mock_observer : mock_observers_) {
    state_->RemoveObserver(mock_observer);
  }
}

TEST_F(PresentationAvailabilityStateTest,
       StopListeningDoesNotStopIfURLListenedByOthers) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  //  |url1_| is only listened to by |observer1_|.
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url1_))
      .Times(1);
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url2_))
      .Times(0);
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url3_))
      .Times(0);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);

  for (auto& mock_observer : mock_observers_) {
    state_->AddObserver(mock_observer);
  }

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(*mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(*mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
  state_->RemoveObserver(mock_observer1_);
}

TEST_F(PresentationAvailabilityStateTest,
       UpdateAvailabilityInvokesAvailabilityChanged) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);

  ChangeURLState(url1_, ScreenAvailability::AVAILABLE);

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  ChangeURLState(url1_, ScreenAvailability::UNAVAILABLE);

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  ChangeURLState(url1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

TEST_F(PresentationAvailabilityStateTest,
       UpdateAvailabilityInvokesMultipleAvailabilityChanged) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::AVAILABLE));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);

  ChangeURLState(url2_, ScreenAvailability::AVAILABLE);

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  }
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       SourceNotSupportedPropagatedToMultipleObservers) {
  PresentationAvailabilityStateTestingContext context;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationAvailability>>(
          context.GetScriptState(), context.GetExceptionContext());
  RequestAvailabilityAndAddObservers(resolver);
  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  }
  ChangeURLState(url2_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

}  // namespace blink
