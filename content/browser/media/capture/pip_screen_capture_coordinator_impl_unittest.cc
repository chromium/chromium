// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "media/capture/capture_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using NativeWindowId = content::PipScreenCaptureCoordinatorImpl::NativeWindowId;
using testing::_;

namespace content {

namespace {

class MockObserver : public PipScreenCaptureCoordinatorImpl::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnPipWindowIdChanged,
              (std::optional<NativeWindowId>),
              (override));
};

}  // namespace

class PipScreenCaptureCoordinatorImplTest : public testing::Test {
 public:
  PipScreenCaptureCoordinatorImplTest() {
    feature_list_.InitAndEnableFeature(features::kExcludePipFromScreenCapture);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  PipScreenCaptureCoordinatorImpl coordinator_;
};

TEST_F(PipScreenCaptureCoordinatorImplTest, PipWindowId) {
  EXPECT_EQ(coordinator_.PipWindowId(), std::nullopt);

  const NativeWindowId pip_window_id = 123;
  coordinator_.OnPipShown(pip_window_id);
  EXPECT_EQ(coordinator_.PipWindowId(), pip_window_id);

  coordinator_.OnPipClosed();
  EXPECT_EQ(coordinator_.PipWindowId(), std::nullopt);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipShownNotifiesObservers) {
  MockObserver observer;
  coordinator_.AddObserver(&observer);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_.OnPipShown(pip_window_id);

  EXPECT_EQ(coordinator_.PipWindowId(), pip_window_id);

  // Calling again with the same ID should not notify.
  EXPECT_CALL(observer, OnPipWindowIdChanged(_)).Times(0);
  coordinator_.OnPipShown(pip_window_id);

  coordinator_.RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, OnPipClosedNotifiesObservers) {
  MockObserver observer;
  coordinator_.AddObserver(&observer);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_.OnPipShown(pip_window_id);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnPipWindowIdChanged(testing::Eq(std::nullopt)));
  coordinator_.OnPipClosed();

  coordinator_.RemoveObserver(&observer);
}

TEST_F(PipScreenCaptureCoordinatorImplTest, AddAndRemoveObserver) {
  MockObserver observer1;
  MockObserver observer2;

  coordinator_.AddObserver(&observer1);
  coordinator_.AddObserver(&observer2);

  const NativeWindowId pip_window_id = 123;
  EXPECT_CALL(observer1,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  EXPECT_CALL(observer2,
              OnPipWindowIdChanged(std::make_optional(pip_window_id)));
  coordinator_.OnPipShown(pip_window_id);
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  coordinator_.RemoveObserver(&observer1);

  const NativeWindowId new_pip_window_id = 456;
  EXPECT_CALL(observer1, OnPipWindowIdChanged(_)).Times(0);
  EXPECT_CALL(observer2,
              OnPipWindowIdChanged(std::make_optional(new_pip_window_id)));
  coordinator_.OnPipShown(new_pip_window_id);

  coordinator_.RemoveObserver(&observer2);
}

}  // namespace content
