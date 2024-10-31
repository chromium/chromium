// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
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

  void RequestAvailabilityAndAddObservers(V8TestingScope& scope) {
    for (auto& mock_observer : mock_observers_) {
      state_->RequestAvailability(
          mock_observer->Urls(),
          MakeGarbageCollected<PresentationAvailabilityProperty>(
              scope.GetExecutionContext()));
      state_->AddObserver(mock_observer);
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // `urls` change state to `states`. This function takes ownership of
  // `promise`.
  void TestRequestAvailability(const Vector<KURL>& urls,
                               const Vector<ScreenAvailability>& states,
                               PresentationAvailabilityProperty* promise) {
    DCHECK_EQ(urls.size(), states.size());

    for (const auto& url : urls) {
      EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
          .Times(1);
      EXPECT_CALL(mock_presentation_service_,
                  StopListeningForScreenAvailability(url))
          .Times(1);
    }

    state_->RequestAvailability(urls, promise);
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
  V8TestingScope scope;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }

  state_->RequestAvailability(
      urls_, MakeGarbageCollected<PresentationAvailabilityProperty>(
                 scope.GetExecutionContext()));
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
  V8TestingScope scope;
  EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url1_))
      .Times(1);

  state_->RequestAvailability(
      Vector<KURL>({url1_}),
      MakeGarbageCollected<PresentationAvailabilityProperty>(
          scope.GetExecutionContext()));
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesAvailable) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability({url1_}, {ScreenAvailability::AVAILABLE}, promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesNotCompatible) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability({url1_}, {ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnavailable) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability({url1_}, {ScreenAvailability::UNAVAILABLE}, promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnsupported) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability({url1_}, {ScreenAvailability::DISABLED}, promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kRejected, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesAvailable) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::AVAILABLE, ScreenAvailability::AVAILABLE}, promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnavailable) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::UNAVAILABLE, ScreenAvailability::UNAVAILABLE},
      promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesNotCompatible) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability({url1_, url2_},
                          {ScreenAvailability::SOURCE_NOT_SUPPORTED,
                           ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnsupported) {
  V8TestingScope scope;
  auto* promise = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::DISABLED, ScreenAvailability::DISABLED}, promise);
  EXPECT_EQ(PresentationAvailabilityProperty::kRejected, promise->GetState());
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  // First getAvailability() call.
  V8TestingScope scope;
  auto* promise1 = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());

  Vector<ScreenAvailability> state_seq = {ScreenAvailability::UNAVAILABLE,
                                          ScreenAvailability::AVAILABLE,
                                          ScreenAvailability::UNAVAILABLE};
  TestRequestAvailability({url1_, url2_, url3_}, state_seq, promise1);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise1->GetState());

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_->Urls()) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  auto* promise2 = MakeGarbageCollected<PresentationAvailabilityProperty>(
      scope.GetExecutionContext());
  state_->RequestAvailability(mock_observer3_->Urls(), promise2);
  EXPECT_EQ(PresentationAvailabilityProperty::kResolved, promise2->GetState());
}

TEST_F(PresentationAvailabilityStateTest, StartListeningListenToEachURLOnce) {
  V8TestingScope scope;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers(scope);
}

TEST_F(PresentationAvailabilityStateTest, StopListeningListenToEachURLOnce) {
  V8TestingScope scope;
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

  RequestAvailabilityAndAddObservers(scope);

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);

  for (auto& mock_observer : mock_observers_) {
    state_->RemoveObserver(mock_observer);
  }
}

TEST_F(PresentationAvailabilityStateTest,
       StopListeningDoesNotStopIfURLListenedByOthers) {
  V8TestingScope scope;
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

  RequestAvailabilityAndAddObservers(scope);

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
  V8TestingScope scope;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));

  RequestAvailabilityAndAddObservers(scope);

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
  V8TestingScope scope;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::AVAILABLE));
  }

  RequestAvailabilityAndAddObservers(scope);

  ChangeURLState(url2_, ScreenAvailability::AVAILABLE);

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  }
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       SourceNotSupportedPropagatedToMultipleObservers) {
  V8TestingScope scope;
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers(scope);
  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  }
  ChangeURLState(url2_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

}  // namespace blink
