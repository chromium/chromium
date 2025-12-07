// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/test/task_environment.h"
#include "remoting/base/constants.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/fake_capture_stream.h"
#include "remoting/host/linux/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {
namespace {

using testing::_;
using testing::Return;

static const webrtc::ScreenId kScreenId1 = 12345;
static const webrtc::ScreenId kScreenId2 = 67890;

class MockObserver : public PipewireMouseCursorCapturer::Observer {
 public:
  MOCK_METHOD(void,
              OnCursorShapeChanged,
              (PipewireMouseCursorCapturer * capturer),
              (override));
  MOCK_METHOD(void,
              OnCursorPositionChanged,
              (PipewireMouseCursorCapturer * capturer),
              (override));
};

class FakeDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  void Start() override { started_ = true; }

  bool IsStarted() const override { return started_; }

  const DesktopDisplayInfo* GetLatestDisplayInfo() const override {
    return info_.get();
  }

  void AddCallback(base::RepeatingClosure callback) override {
    callbacks_.AddUnsafe(std::move(callback));
  }

  void SetDisplayInfo(std::unique_ptr<DesktopDisplayInfo> info) {
    info_ = std::move(info);
    callbacks_.Notify();
  }

 private:
  bool started_ = false;
  std::unique_ptr<DesktopDisplayInfo> info_;
  base::RepeatingClosureList callbacks_;
};

}  // namespace

class PipewireMouseCursorCapturerTest : public testing::Test {
 public:
  PipewireMouseCursorCapturerTest();

 protected:
  void SetDisplayInfo(std::unique_ptr<DesktopDisplayInfo> info);
  const base::flat_map<webrtc::ScreenId,
                       CaptureStream::CursorObserver::Subscription>&
  stream_subscriptions() const;

  base::test::TaskEnvironment task_environment_;
  FakeCaptureStreamManager stream_manager_;
  std::unique_ptr<PipewireMouseCursorCapturer> capturer_;
  raw_ptr<FakeDesktopDisplayInfoMonitor> display_info_monitor_;
  PipewireMouseCursorCapturer::Observer::Subscription subscription_;
  MockObserver mock_observer_;
};

PipewireMouseCursorCapturerTest::PipewireMouseCursorCapturerTest() {
  auto display_info_monitor = std::make_unique<FakeDesktopDisplayInfoMonitor>();
  display_info_monitor_ = display_info_monitor.get();
  capturer_ = std::make_unique<PipewireMouseCursorCapturer>(
      std::move(display_info_monitor), stream_manager_.GetWeakPtr());
  subscription_ = capturer_->AddObserver(&mock_observer_);
  base::WeakPtr<CaptureStream> stream1 =
      stream_manager_.AddVirtualStream(kScreenId1, {100, 100});
  base::WeakPtr<CaptureStream> stream2 =
      stream_manager_.AddVirtualStream(kScreenId2, {200, 200});

  auto display_info = std::make_unique<DesktopDisplayInfo>();
  display_info->set_pixel_type(DesktopDisplayInfo::PixelType::LOGICAL);
  display_info->AddDisplay(
      DisplayGeometry(kScreenId1, 0, 0, 100, 100, kDefaultDpi, 32, true, ""));
  display_info->AddDisplay(DisplayGeometry(kScreenId2, 100, 0, 100, 100,
                                           kDefaultDpi * 2, 32, true, ""));
  SetDisplayInfo(std::move(display_info));
}

void PipewireMouseCursorCapturerTest::SetDisplayInfo(
    std::unique_ptr<DesktopDisplayInfo> info) {
  display_info_monitor_->SetDisplayInfo(std::move(info));
}

const base::flat_map<webrtc::ScreenId,
                     CaptureStream::CursorObserver::Subscription>&
PipewireMouseCursorCapturerTest::stream_subscriptions() const {
  return capturer_->stream_subscriptions_;
}

TEST_F(PipewireMouseCursorCapturerTest,
       OnCursorShapeChanged_FirstMonitorLowDpi) {
  EXPECT_CALL(mock_observer_, OnCursorShapeChanged(_)).Times(1);
  auto cursor = std::make_unique<webrtc::MouseCursor>(
      std::make_unique<webrtc::BasicDesktopFrame>(webrtc::DesktopSize{10, 10}),
      /*hotspot=*/webrtc::DesktopVector{5, 5});
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  EXPECT_CALL(*stream_1, CaptureCursor()).WillOnce(Return(std::move(cursor)));

  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorShapeChanged);

  std::unique_ptr<webrtc::MouseCursor> latest_cursor =
      capturer_->GetLatestCursor();
  ASSERT_NE(latest_cursor, nullptr);
  ASSERT_EQ(latest_cursor->image()->size().width(), 10);
  ASSERT_EQ(latest_cursor->image()->size().height(), 10);
  ASSERT_EQ(latest_cursor->image()->dpi().x(), kDefaultDpi);
  ASSERT_EQ(latest_cursor->image()->dpi().y(), kDefaultDpi);
  ASSERT_EQ(latest_cursor->hotspot().x(), 5);
  ASSERT_EQ(latest_cursor->hotspot().y(), 5);
}

TEST_F(PipewireMouseCursorCapturerTest,
       OnCursorShapeChanged_SecondMonitorHighDpi) {
  EXPECT_CALL(mock_observer_, OnCursorShapeChanged(_)).Times(1);
  auto cursor = std::make_unique<webrtc::MouseCursor>(
      std::make_unique<webrtc::BasicDesktopFrame>(webrtc::DesktopSize{20, 20}),
      /*hotspot=*/webrtc::DesktopVector{10, 10});
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  EXPECT_CALL(*stream_1, CaptureCursor()).WillOnce(Return(nullptr));
  FakeCaptureStream* stream_2 = stream_manager_.GetFakeStream(kScreenId2);
  EXPECT_CALL(*stream_2, CaptureCursor()).WillOnce(Return(std::move(cursor)));

  // Simulate that the cursor has moved from stream 1 to stream 2.
  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorShapeChanged);
  stream_2->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorShapeChanged);

  std::unique_ptr<webrtc::MouseCursor> latest_cursor =
      capturer_->GetLatestCursor();
  ASSERT_NE(latest_cursor, nullptr);
  ASSERT_EQ(latest_cursor->image()->size().width(), 20);
  ASSERT_EQ(latest_cursor->image()->size().height(), 20);
  ASSERT_EQ(latest_cursor->image()->dpi().x(), kDefaultDpi * 2);
  ASSERT_EQ(latest_cursor->image()->dpi().y(), kDefaultDpi * 2);
  ASSERT_EQ(latest_cursor->hotspot().x(), 10);
  ASSERT_EQ(latest_cursor->hotspot().y(), 10);
}

TEST_F(PipewireMouseCursorCapturerTest,
       OnCursorPositionChanged_FirstMonitorLowDpi) {
  EXPECT_CALL(mock_observer_, OnCursorPositionChanged(_)).Times(1);
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  EXPECT_CALL(*stream_1, CaptureCursorPosition())
      .WillOnce(Return(webrtc::DesktopVector{50, 80}));

  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);

  const auto& global_pos = capturer_->GetLatestGlobalCursorPosition();
  ASSERT_TRUE(global_pos.has_value());
  ASSERT_EQ(global_pos->x(), 50);
  ASSERT_EQ(global_pos->y(), 80);
  const auto& fractional_pos = capturer_->GetLatestFractionalCursorPosition();
  ASSERT_TRUE(fractional_pos.has_value());
  ASSERT_NEAR(fractional_pos->x(), 50.0f / 99.0f, 0.0001);
  ASSERT_NEAR(fractional_pos->y(), 80.0f / 99.0f, 0.0001);
  ASSERT_EQ(fractional_pos->screen_id(), kScreenId1);
}

TEST_F(PipewireMouseCursorCapturerTest,
       OnCursorPositionChanged_SecondMonitorHighDpi) {
  EXPECT_CALL(mock_observer_, OnCursorPositionChanged(_)).Times(1);
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  FakeCaptureStream* stream_2 = stream_manager_.GetFakeStream(kScreenId2);
  EXPECT_CALL(*stream_1, CaptureCursorPosition())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*stream_2, CaptureCursorPosition())
      .WillOnce(Return(webrtc::DesktopVector{50, 80}));

  // Simulate that the cursor has moved from stream 1 to stream 2.
  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);
  stream_2->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);

  const auto& global_pos = capturer_->GetLatestGlobalCursorPosition();
  ASSERT_TRUE(global_pos.has_value());
  ASSERT_EQ(global_pos->x(), 100 + 50 / 2);
  ASSERT_EQ(global_pos->y(), 80 / 2);
  const auto& fractional_pos = capturer_->GetLatestFractionalCursorPosition();
  ASSERT_TRUE(fractional_pos.has_value());
  ASSERT_NEAR(fractional_pos->x(), 50.0f / 199.0f, 0.0001);
  ASSERT_NEAR(fractional_pos->y(), 80.0f / 199.0f, 0.0001);
  ASSERT_EQ(fractional_pos->screen_id(), kScreenId2);
}

TEST_F(PipewireMouseCursorCapturerTest,
       OnCursorPositionChanged_PhysicalPixels) {
  auto display_info = std::make_unique<DesktopDisplayInfo>();
  display_info->set_pixel_type(DesktopDisplayInfo::PixelType::PHYSICAL);
  display_info->AddDisplay(DisplayGeometry(kScreenId1, 0, 0, 200, 200,
                                           kDefaultDpi * 2, 32, true, ""));
  display_info->AddDisplay(DisplayGeometry(kScreenId2, 200, 0, 100, 100,
                                           kDefaultDpi * 2, 32, true, ""));
  SetDisplayInfo(std::move(display_info));
  EXPECT_CALL(mock_observer_, OnCursorPositionChanged(_)).Times(1);
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  FakeCaptureStream* stream_2 = stream_manager_.GetFakeStream(kScreenId2);
  EXPECT_CALL(*stream_1, CaptureCursorPosition())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*stream_2, CaptureCursorPosition())
      .WillOnce(Return(webrtc::DesktopVector{50, 80}));

  // Simulate that the cursor has moved from stream 1 to stream 2.
  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);
  stream_2->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);

  const auto& global_pos = capturer_->GetLatestGlobalCursorPosition();
  ASSERT_TRUE(global_pos.has_value());
  ASSERT_EQ(global_pos->x(), 200 + 50);
  ASSERT_EQ(global_pos->y(), 80);
  const auto& fractional_pos = capturer_->GetLatestFractionalCursorPosition();
  ASSERT_TRUE(fractional_pos.has_value());
  ASSERT_NEAR(fractional_pos->x(), 50.0f / 99.0f, 0.0001);
  ASSERT_NEAR(fractional_pos->y(), 80.0f / 99.0f, 0.0001);
  ASSERT_EQ(fractional_pos->screen_id(), kScreenId2);
}

TEST_F(PipewireMouseCursorCapturerTest, CaptureStreamRemoved_Unsubscribe) {
  ASSERT_FALSE(stream_subscriptions().empty());
  stream_manager_.RemoveVirtualStream(kScreenId1);
  stream_manager_.RemoveVirtualStream(kScreenId2);
  ASSERT_TRUE(stream_subscriptions().empty());
}

TEST_F(PipewireMouseCursorCapturerTest,
       SubscriptionDestroyed_NoLongerNotified) {
  EXPECT_CALL(mock_observer_, OnCursorShapeChanged(_)).Times(0);
  EXPECT_CALL(mock_observer_, OnCursorPositionChanged(_)).Times(0);
  FakeCaptureStream* stream_1 = stream_manager_.GetFakeStream(kScreenId1);
  // PipewireMouseCursorCapturer will still call these methods and store the
  // returned values so that they are available via GetLatest*().
  EXPECT_CALL(*stream_1, CaptureCursor()).WillOnce(Return(nullptr));
  EXPECT_CALL(*stream_1, CaptureCursorPosition())
      .WillOnce(Return(std::nullopt));

  subscription_ = {};
  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorShapeChanged);
  stream_1->NotifyCursorObservers(
      &CaptureStream::CursorObserver::OnCursorPositionChanged);
}

}  // namespace remoting
