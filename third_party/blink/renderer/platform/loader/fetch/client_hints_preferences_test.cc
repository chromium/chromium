// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ClientHintsPreferencesTest : public testing::Test {};

TEST_F(ClientHintsPreferencesTest, BasicSecure) {
  struct TestCase {
    const char* header_value;
    bool expectation_resource_width;
    bool expectation_dpr;
    bool expectation_viewport_width;
    bool expectation_rtt;
    bool expectation_downlink;
    bool expectation_ect;
    bool expectation_lang;
    bool expectation_ua;
    bool expectation_ua_arch;
    bool expectation_ua_platform;
    bool expectation_ua_model;
  } cases[] = {
      {"width, dpr, viewportWidth", true, true, false, false, false, false,
       false, false, false, false, false},
      {"WiDtH, dPr, viewport-width, rtt, downlink, ect, lang", true, true, true,
       true, true, true, true, false, false, false, false},
      {"WiDtH, dPr, viewport-width, rtt, downlink, effective-connection-type",
       true, true, true, true, true, false, false, false, false, false, false},
      {"WIDTH, DPR, VIWEPROT-Width", true, true, false, false, false, false,
       false, false, false, false, false},
      {"VIewporT-Width, wutwut, width", true, false, true, false, false, false,
       false, false, false, false, false},
      {"dprw", false, false, false, false, false, false, false, false, false,
       false, false},
      {"DPRW", false, false, false, false, false, false, false, false, false,
       false, false},
      {"ua", false, false, false, false, false, false, false, true, false,
       false, false},
      {"arch", false, false, false, false, false, false, false, false, true,
       false, false},
      {"platform", false, false, false, false, false, false, false, false,
       false, true, false},
      {"model", false, false, false, false, false, false, false, false, false,
       false, true},
      {"ua, arch, platform, model", false, false, false, false, false, false,
       false, true, true, true, true},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(testing::Message() << test_case.header_value);
    ClientHintsPreferences preferences;
    const KURL kurl(String::FromUTF8("https://www.google.com/"));
    preferences.UpdateFromAcceptClientHintsHeader(test_case.header_value, kurl,
                                                  nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));
    EXPECT_EQ(test_case.expectation_rtt,
              preferences.ShouldSend(mojom::WebClientHintsType::kRtt));
    EXPECT_EQ(test_case.expectation_downlink,
              preferences.ShouldSend(mojom::WebClientHintsType::kDownlink));
    EXPECT_EQ(test_case.expectation_ect,
              preferences.ShouldSend(mojom::WebClientHintsType::kEct));
    EXPECT_EQ(test_case.expectation_lang,
              preferences.ShouldSend(mojom::WebClientHintsType::kLang));
    EXPECT_EQ(test_case.expectation_ua,
              preferences.ShouldSend(mojom::WebClientHintsType::kUA));
    EXPECT_EQ(test_case.expectation_ua_arch,
              preferences.ShouldSend(mojom::WebClientHintsType::kUAArch));
    EXPECT_EQ(test_case.expectation_ua_platform,
              preferences.ShouldSend(mojom::WebClientHintsType::kUAPlatform));
    EXPECT_EQ(test_case.expectation_ua_model,
              preferences.ShouldSend(mojom::WebClientHintsType::kUAModel));

    // Calling UpdateFromAcceptClientHintsHeader with empty header should have
    // no impact on client hint preferences.
    preferences.UpdateFromAcceptClientHintsHeader("", kurl, nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));

    // Calling UpdateFromAcceptClientHintsHeader with an invalid header should
    // have no impact on client hint preferences.
    preferences.UpdateFromAcceptClientHintsHeader("foobar", kurl, nullptr);
    EXPECT_EQ(
        test_case.expectation_resource_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width,
        preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));
  }
}

// Verify that the set of enabled client hints is updated every time Update*()
// methods are called.
TEST_F(ClientHintsPreferencesTest, SecureEnabledTypesAreUpdated) {
  ClientHintsPreferences preferences;
  const KURL kurl(String::FromUTF8("https://www.google.com/"));
  preferences.UpdateFromAcceptClientHintsHeader("rtt, downlink", kurl, nullptr);

  EXPECT_EQ(base::TimeDelta(), preferences.GetPersistDuration());
  EXPECT_FALSE(
      preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
  EXPECT_FALSE(
      preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kRtt));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kDownlink));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kEct));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kLang));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAModel));

  // Calling UpdateFromAcceptClientHintsHeader with empty header should have
  // no impact on client hint preferences.
  preferences.UpdateFromAcceptClientHintsHeader("", kurl, nullptr);
  EXPECT_EQ(base::TimeDelta(), preferences.GetPersistDuration());
  EXPECT_FALSE(
      preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kRtt));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kDownlink));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kEct));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kLang));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAModel));

  // Calling UpdateFromAcceptClientHintsHeader with an invalid header should
  // have no impact on client hint preferences.
  preferences.UpdateFromAcceptClientHintsHeader("foobar", kurl, nullptr);
  EXPECT_EQ(base::TimeDelta(), preferences.GetPersistDuration());
  EXPECT_FALSE(
      preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kRtt));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kDownlink));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kLang));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAModel));

  // Calling UpdateFromAcceptClientHintsHeader with "width" header should
  // have no impact on already enabled client hint preferences.
  preferences.UpdateFromAcceptClientHintsHeader("width", kurl, nullptr);
  EXPECT_EQ(base::TimeDelta(), preferences.GetPersistDuration());
  EXPECT_TRUE(
      preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kRtt));
  EXPECT_TRUE(preferences.ShouldSend(mojom::WebClientHintsType::kDownlink));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kEct));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kLang));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(preferences.ShouldSend(mojom::WebClientHintsType::kUAModel));

  preferences.UpdateFromAcceptClientHintsLifetimeHeader("1000", kurl, nullptr);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1000),
            preferences.GetPersistDuration());
}

TEST_F(ClientHintsPreferencesTest, Insecure) {
  for (const auto& use_secure_url : {false, true}) {
    ClientHintsPreferences preferences;
    const KURL kurl = use_secure_url
                          ? KURL(String::FromUTF8("https://www.google.com/"))
                          : KURL(String::FromUTF8("http://www.google.com/"));
    preferences.UpdateFromAcceptClientHintsHeader("dpr", kurl, nullptr);
    EXPECT_EQ(use_secure_url,
              preferences.ShouldSend(mojom::WebClientHintsType::kDpr));
  }
}

// Verify that the client hints header and the lifetime header is parsed
// correctly.
TEST_F(ClientHintsPreferencesTest, ParseHeaders) {
  struct TestCase {
    const char* accept_ch_header_value;
    const char* accept_lifetime_header_value;
    int64_t expect_persist_duration_seconds;
    bool expect_device_memory;
    bool expect_width;
    bool expect_dpr;
    bool expect_viewport_width;
    bool expect_rtt;
    bool expect_downlink;
    bool expect_ect;
    bool expect_lang;
    bool expect_ua;
    bool expect_ua_arch;
    bool expect_ua_platform;
    bool expect_ua_model;
  } test_cases[] = {
      {"width, dpr, viewportWidth, lang", "", 0, false, true, true, false,
       false, false, false, true, false, false, false, false},
      {"width, dpr, viewportWidth", "-1000", 0, false, true, true, false, false,
       false, false, false, false, false, false, false},
      {"width, dpr, viewportWidth", "1000s", 0, false, true, true, false, false,
       false, false, false, false, false, false, false},
      {"width, dpr, viewportWidth", "1000.5", 0, false, true, true, false,
       false, false, false, false, false, false, false, false},
      {"width, dpr, rtt, downlink, ect", "1000", 1000, false, true, true, false,
       true, true, true, false, false, false, false, false},
      {"device-memory", "-1000", 0, true, false, false, false, false, false,
       false, false, false, false, false, false},
      {"dpr rtt", "1000", 1000, false, false, false, false, false, false, false,
       false, false, false, false, false},
      {"ua, arch, platform, model", "1000", 1000, false, false, false, false,
       false, false, false, false, true, true, true, true},
  };

  for (const auto& test : test_cases) {
    ClientHintsPreferences preferences;
    WebEnabledClientHints enabled_types =
        preferences.GetWebEnabledClientHints();
    EXPECT_FALSE(
        enabled_types.IsEnabled(mojom::WebClientHintsType::kDeviceMemory));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kDpr));
    EXPECT_FALSE(
        enabled_types.IsEnabled(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_FALSE(
        enabled_types.IsEnabled(mojom::WebClientHintsType::kViewportWidth));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kRtt));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kDownlink));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kEct));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kLang));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kUA));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kUAArch));
    EXPECT_FALSE(
        enabled_types.IsEnabled(mojom::WebClientHintsType::kUAPlatform));
    EXPECT_FALSE(enabled_types.IsEnabled(mojom::WebClientHintsType::kUAModel));
    base::TimeDelta persist_duration = preferences.GetPersistDuration();
    EXPECT_EQ(base::TimeDelta(), persist_duration);

    const KURL kurl(String::FromUTF8("https://www.google.com/"));
    preferences.UpdateFromAcceptClientHintsHeader(test.accept_ch_header_value,
                                                  kurl, nullptr);
    preferences.UpdateFromAcceptClientHintsLifetimeHeader(
        test.accept_lifetime_header_value, kurl, nullptr);

    enabled_types = preferences.GetWebEnabledClientHints();
    persist_duration = preferences.GetPersistDuration();

    EXPECT_EQ(test.expect_persist_duration_seconds,
              persist_duration.InSeconds());

    EXPECT_EQ(
        test.expect_device_memory,
        enabled_types.IsEnabled(mojom::WebClientHintsType::kDeviceMemory));
    EXPECT_EQ(test.expect_dpr,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test.expect_width,
        enabled_types.IsEnabled(mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(
        test.expect_viewport_width,
        enabled_types.IsEnabled(mojom::WebClientHintsType::kViewportWidth));
    EXPECT_EQ(test.expect_rtt,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kRtt));
    EXPECT_EQ(test.expect_downlink,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kDownlink));
    EXPECT_EQ(test.expect_ect,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kEct));
    EXPECT_EQ(test.expect_lang,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kLang));
    EXPECT_EQ(test.expect_ua,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kUA));
    EXPECT_EQ(test.expect_ua_arch,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kUAArch));
    EXPECT_EQ(test.expect_ua_platform,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kUAPlatform));
    EXPECT_EQ(test.expect_ua_model,
              enabled_types.IsEnabled(mojom::WebClientHintsType::kUAModel));
  }
}

}  // namespace blink
