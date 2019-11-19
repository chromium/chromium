// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_device_id_calculator.h"

#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionsBrowserClientWithPrefService
    : public TestExtensionsBrowserClient {
 public:
  explicit TestExtensionsBrowserClientWithPrefService(
      content::BrowserContext* main_context)
      : TestExtensionsBrowserClient(main_context) {}
  ~TestExtensionsBrowserClientWithPrefService() override {}

  // ExtensionsBrowserClient override:
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override {
    return &pref_service_;
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsBrowserClientWithPrefService);
};

class AudioDeviceIdCalculatorTest : public testing::Test {
 public:
  AudioDeviceIdCalculatorTest() : test_browser_client_(&browser_context_) {}
  ~AudioDeviceIdCalculatorTest() override {}

  void SetUp() override {
    AudioAPI::RegisterUserPrefs(
        test_browser_client_.pref_service()->registry());
    ExtensionsBrowserClient::Set(&test_browser_client_);
  }

  void TearDown() override { ExtensionsBrowserClient::Set(nullptr); }

  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestExtensionsBrowserClientWithPrefService test_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceIdCalculatorTest);
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
  calculator.reset(new AudioDeviceIdCalculator(browser_context()));
  EXPECT_EQ("3", calculator->GetStableDeviceId(22222));
  EXPECT_EQ("4", calculator->GetStableDeviceId(33333));

  // Test that ID mapping from before calculator reset is not lost.
  EXPECT_EQ("1", calculator->GetStableDeviceId(54321));
  EXPECT_EQ("2", calculator->GetStableDeviceId(11111));
  EXPECT_EQ("0", calculator->GetStableDeviceId(12345));
}

}  // namespace extensions
