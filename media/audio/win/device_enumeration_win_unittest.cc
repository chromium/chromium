// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/device_enumeration_win.h"

#include "base/test/task_environment.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/test_audio_thread.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DeviceEnumerationWin, GetDeviceSuffix) {
  // Some real-world USB devices
  EXPECT_EQ(
      GetDeviceSuffixWin("USB\\VID_046D&PID_09A6&MI_02\\6&318d810e&1&0002"),
      " (046d:09a6)");
  EXPECT_EQ(GetDeviceSuffixWin("USB\\VID_8087&PID_07DC&REV_0001"),
            " (8087:07dc)");
  EXPECT_EQ(GetDeviceSuffixWin("USB\\VID_0403&PID_6010"), " (0403:6010)");

  // Some real-world Bluetooth devices
  EXPECT_EQ(GetDeviceSuffixWin("BTHHFENUM\\BthHFPAudio\\8&39e29755&0&97"),
            " (Bluetooth)");
  EXPECT_EQ(GetDeviceSuffixWin("BTHENUM\\{0000110b-0000-1000-8000-"
                               "00805f9b34fb}_LOCALMFG&0002\\7&25f92e87&0&"
                               "70886B900BB0_C00000000"),
            " (Bluetooth)");

  // Other real-world devices
  EXPECT_TRUE(GetDeviceSuffixWin("INTELAUDIO\\FUNC_01&VEN_8086&DEV_280B&SUBSYS_"
                                 "80860101&REV_1000\\4&c083774&0&0201")
                  .empty());
  EXPECT_TRUE(GetDeviceSuffixWin("INTELAUDIO\\FUNC_01&VEN_10EC&DEV_0298&SUBSYS_"
                                 "102807BF&REV_1001\\4&c083774&0&0001")
                  .empty());
  EXPECT_TRUE(
      GetDeviceSuffixWin("PCI\\VEN_1000&DEV_0001&SUBSYS_00000000&REV_02\\1&08")
          .empty());

  // Other input strings.
  EXPECT_TRUE(GetDeviceSuffixWin(std::string()).empty());
  EXPECT_TRUE(GetDeviceSuffixWin("            ").empty());
  EXPECT_TRUE(GetDeviceSuffixWin("USBVID_1234&PID1234").empty());
}

class DeviceEnumerationWinTest : public ::testing::Test {
 public:
  DeviceEnumerationWinTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(std::make_unique<TestAudioThread>());
  }
  ~DeviceEnumerationWinTest() override { audio_manager_->Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AudioManager> audio_manager_;
};

static bool HasCoreAudioAndInputDevices(AudioManager* audio_man) {
  return CoreAudioUtil::IsSupported() &&
         AudioDeviceInfoAccessorForTests(audio_man).HasAudioInputDevices();
}

// This is a smoke test only. We do not mock the Windows COM interfaces
// (IMMDevice, IPropertyStore, etc.) here because doing so would require
// a massive amount of boilerplate code just to simulate the specific hardware
// edge case (Intel SST DSP masking) that triggers the Bluetooth fallback.
// Instead, this test simply runs the full enumeration against the host's actual
// audio hardware. This ensures that the new lazy-loading and Container ID
// cross-referencing logic does not crash or fail unexpectedly when executed
// on standard generic hardware setups (like those found on CI bots).
TEST_F(DeviceEnumerationWinTest, EnumerationDoesNotCrashWithFallbackLogic) {
  // Gracefully abort if the test environment does not have CoreAudio or any
  // audio devices.
  ABORT_AUDIO_TEST_IF_NOT(HasCoreAudioAndInputDevices(audio_manager_.get()));

  media::AudioDeviceDescriptions input_devices;
  media::AudioDeviceDescriptions output_devices;

  // Executing these will internally run GetDeviceNamesWinImpl, exercising our
  // lazy-loading opposite_collection logic and Container ID checks on whatever
  // generic hardware the test bot provides.
  AudioDeviceInfoAccessorForTests(audio_manager_.get())
      .GetAudioInputDeviceDescriptions(&input_devices);
  AudioDeviceInfoAccessorForTests(audio_manager_.get())
      .GetAudioOutputDeviceDescriptions(&output_devices);

  EXPECT_FALSE(input_devices.empty());
  EXPECT_FALSE(output_devices.empty());
}

}  // namespace media
