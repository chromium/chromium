// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_device_id_calculator.h"

#include <memory>

#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class AudioDeviceIdCalculatorTest : public testing::Test {
 public:
  AudioDeviceIdCalculatorTest() : test_browser_client_(&browser_context_) {}

  AudioDeviceIdCalculatorTest(const AudioDeviceIdCalculatorTest&) = delete;
  AudioDeviceIdCalculatorTest& operator=(const AudioDeviceIdCalculatorTest&) =
      delete;

  ~AudioDeviceIdCalculatorTest() override = default;

  void SetUp() override {
    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    AudioAPI::RegisterUserPrefs(pref_service_.registry());
    ExtensionsBrowserClient::Set(&test_browser_client_);
  }

  void TearDown() override { ExtensionsBrowserClient::Set(nullptr); }

  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestingPrefServiceSimple pref_service_;
  TestExtensionsBrowserClient test_browser_client_;
};

}  // namespace

TEST_F(AudioDeviceIdCalculatorTest, Test) {
  std::unique_ptr<AudioDeviceIdCalculator> calculator(
      new AudioDeviceIdCalculator(browser_context()));

  // Generating new IDs.
  EXPECT_EQ("0", calculator->GetStableDeviceId(12345));
  EXPECT_EQ("1", calculator->GetStableDeviceId(54321));
  EXPECT_EQ("2", calculator->GetStableDeviceId(11111));

  // Test getting IDs generated so far.
  EXPECT_EQ("1", calculator->GetStableDeviceId(54321));
  EXPECT_EQ("0", calculator->GetStableDeviceId(12345));
  EXPECT_EQ("2", calculator->GetStableDeviceId(11111));

  // Reset the calculator and test adding stable IDs does not produce ID
  // conflicting with previously added ones.
  calculator = std::make_unique<AudioDeviceIdCalculator>(browser_context());
  EXPECT_EQ("3", calculator->GetStableDeviceId(22222));
  EXPECT_EQ("4", calculator->GetStableDeviceId(33333));

  // Test that ID mapping from before calculator reset is not lost.
  EXPECT_EQ("1", calculator->GetStableDeviceId(54321));
  EXPECT_EQ("2", calculator->GetStableDeviceId(11111));
  EXPECT_EQ("0", calculator->GetStableDeviceId(12345));
}

}  // namespace extensions
