// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/presentation/mock_presentation_service.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_callbacks.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::_;

namespace blink {

using mojom::blink::ScreenAvailability;

class MockPresentationAvailabilityObserver
    : public GarbageCollected<MockPresentationAvailabilityObserver>,
      public PresentationAvailabilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MockPresentationAvailabilityObserver);

 public:
  explicit MockPresentationAvailabilityObserver(const Vector<KURL>& urls)
      : urls_(urls) {}
  ~MockPresentationAvailabilityObserver() override = default;

  MOCK_METHOD1(AvailabilityChanged, void(ScreenAvailability availability));
  const Vector<KURL>& Urls() const override { return urls_; }

 private:
  const Vector<KURL> urls_;
};

class MockPresentationAvailabilityCallbacks
    : public PresentationAvailabilityCallbacks {
 public:
  MockPresentationAvailabilityCallbacks()
      : PresentationAvailabilityCallbacks(nullptr, WTF::Vector<KURL>()) {}
  ~MockPresentationAvailabilityCallbacks() override = default;

  MOCK_METHOD1(Resolve, void(bool value));
  MOCK_METHOD0(RejectAvailabilityNotSupported, void());
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
    if (state != ScreenAvailability::UNKNOWN)
      state_->UpdateAvailability(url, state);
  }

  void RequestAvailabilityAndAddObservers() {
    for (auto& mock_observer : mock_observers_) {
      state_->RequestAvailability(
          mock_observer->Urls(),
          MakeGarbageCollected<MockPresentationAvailabilityCallbacks>());
      state_->AddObserver(mock_observer);
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // |urls| change state to |states|. This function takes ownership of
  // |mock_callback|.
  void TestRequestAvailability(
      const Vector<KURL>& urls,
      const Vector<ScreenAvailability>& states,
      MockPresentationAvailabilityCallbacks* mock_callback) {
    DCHECK_EQ(urls.size(), states.size());

    for (const auto& url : urls) {
      EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
          .Times(1);
      EXPECT_CALL(mock_presentation_service_,
                  StopListeningForScreenAvailability(url))
          .Times(1);
    }

    state_->RequestAvailability(urls, mock_callback);
    for (wtf_size_t i = 0; i < urls.size(); i++)
      ChangeURLState(urls[i], states[i]);
  }

 protected:
  const KURL url1_;
  const KURL url2_;
  const KURL url3_;
  const KURL url4_;
  const Vector<KURL> urls_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer_all_urls_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer1_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer2_;
  Persistent<MockPresentationAvailabilityObserver> mock_observer3_;
  Vector<Persistent<MockPresentationAvailabilityObserver>> mock_observers_;

  MockPresentationService mock_presentation_service_;
  Persistent<PresentationAvailabilityState> state_;
};

TEST_F(PresentationAvailabilityStateTest, RequestAvailability) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }

  state_->RequestAvailability(
      urls_, MakeGarbageCollected<MockPresentationAvailabilityCallbacks>());
  state_->UpdateAvailability(url1_, ScreenAvailability::AVAILABLE);

  for (const auto& url : urls_)
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));

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
  for (const auto& url : urls_)
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));

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
  auto* mock_callback = MakeGarbageCollected<
      testing::StrictMock<MockPresentationAvailabilityCallbacks>>();

  EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url1_))
      .Times(1);

  state_->RequestAvailability(Vector<KURL>({url1_}), mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesAvailable) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(true));

  TestRequestAvailability({url1_}, {ScreenAvailability::AVAILABLE},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesNotCompatible) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(false));

  TestRequestAvailability({url1_}, {ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnavailable) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(false));

  TestRequestAvailability({url1_}, {ScreenAvailability::UNAVAILABLE},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnsupported) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, RejectAvailabilityNotSupported());

  TestRequestAvailability({url1_}, {ScreenAvailability::DISABLED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesAvailable) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(true)).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::AVAILABLE, ScreenAvailability::AVAILABLE},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnavailable) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(false)).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::UNAVAILABLE, ScreenAvailability::UNAVAILABLE},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesNotCompatible) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, Resolve(false)).Times(1);

  TestRequestAvailability({url1_, url2_},
                          {ScreenAvailability::SOURCE_NOT_SUPPORTED,
                           ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnsupported) {
  auto* mock_callback =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback, RejectAvailabilityNotSupported()).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::DISABLED, ScreenAvailability::DISABLED},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  // First getAvailability() call.
  auto* mock_callback_1 =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback_1, Resolve(false)).Times(1);

  Vector<ScreenAvailability> state_seq = {ScreenAvailability::UNAVAILABLE,
                                          ScreenAvailability::AVAILABLE,
                                          ScreenAvailability::UNAVAILABLE};
  TestRequestAvailability({url1_, url2_, url3_}, state_seq, mock_callback_1);

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_->Urls()) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }
  auto* mock_callback_2 =
      MakeGarbageCollected<MockPresentationAvailabilityCallbacks>();
  EXPECT_CALL(*mock_callback_2, Resolve(true)).Times(1);

  state_->RequestAvailability(mock_observer3_->Urls(), mock_callback_2);
}

TEST_F(PresentationAvailabilityStateTest, StartListeningListenToEachURLOnce) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers();
}

TEST_F(PresentationAvailabilityStateTest, StopListeningListenToEachURLOnce) {
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

  RequestAvailabilityAndAddObservers();

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);

  for (auto& mock_observer : mock_observers_)
    state_->RemoveObserver(mock_observer);
}

TEST_F(PresentationAvailabilityStateTest,
       StopListeningDoesNotStopIfURLListenedByOthers) {
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

  RequestAvailabilityAndAddObservers();

  for (auto& mock_observer : mock_observers_)
    state_->AddObserver(mock_observer);

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
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(*mock_observer1_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));

  RequestAvailabilityAndAddObservers();

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
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::AVAILABLE));
  }

  RequestAvailabilityAndAddObservers();

  ChangeURLState(url2_, ScreenAvailability::AVAILABLE);

  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  }
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       SourceNotSupportedPropagatedToMultipleObservers) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers();
  for (auto& mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  }
  ChangeURLState(url2_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

}  // namespace blink
