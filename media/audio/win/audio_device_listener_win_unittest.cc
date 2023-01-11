// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_device_listener_win.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;

namespace media {

constexpr char kFirstTestDevice[] = "test_device_0";
constexpr char kSecondTestDevice[] = "test_device_1";

class AudioDeviceListenerWinTest
    : public testing::Test,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  AudioDeviceListenerWinTest() {
    DCHECK(com_init_.Succeeded());
    if (!CoreAudioUtil::IsSupported())
      return;

    system_monitor_.AddDevicesChangedObserver(this);

    output_device_listener_ = std::make_unique<AudioDeviceListenerWin>(
        base::BindRepeating(&AudioDeviceListenerWinTest::OnDeviceChange,
                            base::Unretained(this)));

    tick_clock_.Advance(base::Seconds(12345));
    output_device_listener_->tick_clock_ = &tick_clock_;
  }

  AudioDeviceListenerWinTest(const AudioDeviceListenerWinTest&) = delete;
  AudioDeviceListenerWinTest& operator=(const AudioDeviceListenerWinTest&) =
      delete;

  ~AudioDeviceListenerWinTest() override {
    system_monitor_.RemoveDevicesChangedObserver(this);
  }

  void AdvanceLastDeviceChangeTime() {
    tick_clock_.Advance(AudioDeviceListenerWin::kDeviceChangeLimit +
                        base::Milliseconds(1));
  }

  // Simulate a device change where no output devices are available.
  bool SimulateNullDefaultOutputDeviceChange() {
    auto result = output_device_listener_->OnDefaultDeviceChanged(
        static_cast<EDataFlow>(eConsole), static_cast<ERole>(eRender), nullptr);
    task_environment_.RunUntilIdle();
    return result == S_OK;
  }

  bool SimulateDefaultOutputDeviceChange(const char* new_device_id) {
    auto result = output_device_listener_->OnDefaultDeviceChanged(
        static_cast<EDataFlow>(eConsole), static_cast<ERole>(eRender),
        base::ASCIIToWide(new_device_id).c_str());
    task_environment_.RunUntilIdle();
    return result == S_OK;
  }

  MOCK_METHOD0(OnDeviceChange, void());
  MOCK_METHOD1(OnDevicesChanged, void(base::SystemMonitor::DeviceType));

 private:
  ScopedCOMInitializer com_init_;
  base::test::TaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<AudioDeviceListenerWin> output_device_listener_;
};

// Simulate a device change events and ensure we get the right callbacks.
TEST_F(AudioDeviceListenerWinTest, OutputDeviceChange) {
  ABORT_AUDIO_TEST_IF_NOT(CoreAudioUtil::IsSupported());

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kFirstTestDevice));

  testing::Mock::VerifyAndClear(this);
  AdvanceLastDeviceChangeTime();
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kSecondTestDevice));

  // Since it occurs too soon, the second device event shouldn't call
  // OnDeviceChange(), but it should notify OnDevicesChanged().
  EXPECT_CALL(*this, OnDeviceChange()).Times(0);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kSecondTestDevice));
}

// Ensure that null output device changes don't crash.  Simulates the situation
// where we have no output devices.
TEST_F(AudioDeviceListenerWinTest, NullOutputDeviceChange) {
  ABORT_AUDIO_TEST_IF_NOT(CoreAudioUtil::IsSupported());

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateNullDefaultOutputDeviceChange());

  testing::Mock::VerifyAndClear(this);
  AdvanceLastDeviceChangeTime();
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(kFirstTestDevice));

  // Since it occurs too soon, the second device event shouldn't call
  // OnDeviceChange(), but it should notify OnDevicesChanged().
  testing::Mock::VerifyAndClear(this);
  EXPECT_CALL(*this, OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO))
      .Times(1);
  ASSERT_TRUE(SimulateNullDefaultOutputDeviceChange());
}

}  // namespace media
