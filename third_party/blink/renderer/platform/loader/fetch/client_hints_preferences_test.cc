// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

TEST(ClientHintsPreferencesTest, BasicSecure) {
  struct TestCase {
    const char* header_value;
    bool expectation_resource_width_DEPRECATED;
    bool expectation_resource_width;
    bool expectation_dpr_DEPRECATED;
    bool expectation_dpr;
    bool expectation_viewport_width_DEPRECATED;
    bool expectation_viewport_width;
    bool expectation_rtt;
    bool expectation_downlink;
    bool expectation_ect;
    bool expectation_ua;
    bool expectation_ua_arch;
    bool expectation_ua_platform;
    bool expectation_ua_model;
    bool expectation_ua_full_version;
    bool expectation_prefers_color_scheme;
    bool expectation_prefers_reduced_motion;
    bool expectation_prefers_reduced_transparency;
  } cases[] = {
      {"width, sec-ch-width, dpr, sec-ch-dpr, viewportWidth, "
       "sec-ch-viewportWidth",
       true, true, true, true, false, false, false, false, false, false, false,
       false, false, false, false, false, false},
      {"WiDtH, sEc-ch-WiDtH, dPr, sec-cH-dPr, viewport-width, "
       "sec-ch-viewport-width, rtt, downlink, ect, "
       "sec-ch-prefers-color-scheme, sec-ch-prefers-reduced-motion, "
       "sec-ch-prefers-reduced-transparency",
       true, true, true, true, true, true, true, true, true, false, false,
       false, false, false, true, true, true},
      {"WiDtH, dPr, viewport-width, rtt, downlink, effective-connection-type",
       true, false, true, false, true, false, true, true, false, false, false,
       false, false, false, false, false, false},
      {"sec-ch-WIDTH, DPR, VIWEPROT-Width", false, true, true, false, false,
       false, false, false, false, false, false, false, false, false, false,
       false, false},
      {"sec-ch-VIewporT-Width, wutwut, width", true, false, false, false, false,
       true, false, false, false, false, false, false, false, false, false,
       false, false},
      {"dprw", false, false, false, false, false, false, false, false, false,
       false, false, false, false, false, false, false, false},
      {"DPRW", false, false, false, false, false, false, false, false, false,
       false, false, false, false, false, false, false, false},
      {"sec-ch-ua", false, false, false, false, false, false, false, false,
       false, true, false, false, false, false, false, false, false},
      {"sec-ch-ua-arch", false, false, false, false, false, false, false, false,
       false, false, true, false, false, false, false, false, false},
      {"sec-ch-ua-platform", false, false, false, false, false, false, false,
       false, false, false, false, true, false, false, false, false, false},
      {"sec-ch-ua-model", false, false, false, false, false, false, false,
       false, false, false, false, false, true, false, false, false, false},
      {"sec-ch-ua, sec-ch-ua-arch, sec-ch-ua-platform, sec-ch-ua-model, "
       "sec-ch-ua-full-version",
       false, false, false, false, false, false, false, false, false, true,
       true, true, true, true, false, false, false},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(testing::Message() << test_case.header_value);
    ClientHintsPreferences preferences;
    const KURL kurl(String::FromUTF8("https://www.google.com/"));
    bool did_update = preferences.UpdateFromMetaCH(
        test_case.header_value, kurl, nullptr,
        network::MetaCHType::HttpEquivAcceptCH,
        /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
    EXPECT_TRUE(did_update);
    EXPECT_EQ(
        test_case.expectation_resource_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_resource_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr_DEPRECATED,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_viewport_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kViewportWidth));
    EXPECT_EQ(test_case.expectation_rtt,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));
    EXPECT_EQ(test_case.expectation_downlink,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
    EXPECT_EQ(test_case.expectation_ect,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kEct_DEPRECATED));
    EXPECT_EQ(test_case.expectation_ua,
              preferences.ShouldSend(network::mojom::WebClientHintsType::kUA));
    EXPECT_EQ(
        test_case.expectation_ua_arch,
        preferences.ShouldSend(network::mojom::WebClientHintsType::kUAArch));
    EXPECT_EQ(test_case.expectation_ua_platform,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kUAPlatform));
    EXPECT_EQ(
        test_case.expectation_ua_model,
        preferences.ShouldSend(network::mojom::WebClientHintsType::kUAModel));
    EXPECT_EQ(test_case.expectation_prefers_color_scheme,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kPrefersColorScheme));
    EXPECT_EQ(test_case.expectation_prefers_reduced_motion,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kPrefersReducedMotion));
    EXPECT_EQ(
        test_case.expectation_prefers_reduced_transparency,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kPrefersReducedTransparency));

    // Calling UpdateFromMetaCH with an invalid header should
    // have no impact on client hint preferences.
    did_update = preferences.UpdateFromMetaCH(
        "1, 42,", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
        /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
    EXPECT_FALSE(did_update);
    EXPECT_EQ(
        test_case.expectation_resource_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_resource_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr_DEPRECATED,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_viewport_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kViewportWidth));

    // Calling UpdateFromMetaCH with empty header is also a
    // no-op, since ClientHintsPreferences only deals with meta tags, and
    // hence merge.
    did_update = preferences.UpdateFromMetaCH(
        "", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
        /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
    EXPECT_TRUE(did_update);
    EXPECT_EQ(
        test_case.expectation_resource_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_resource_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(test_case.expectation_dpr_DEPRECATED,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_EQ(test_case.expectation_dpr,
              preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test_case.expectation_viewport_width_DEPRECATED,
        preferences.ShouldSend(
            network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
    EXPECT_EQ(test_case.expectation_viewport_width,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kViewportWidth));
  }
}

// Verify that the set of enabled client hints is merged every time
// Update*() methods are called.
TEST(ClientHintsPreferencesTest, SecureEnabledTypesMerge) {
  ClientHintsPreferences preferences;
  const KURL kurl(String::FromUTF8("https://www.google.com/"));
  bool did_update = preferences.UpdateFromMetaCH(
      "rtt, downlink", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
      /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
  EXPECT_TRUE(did_update);
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(network::mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAModel));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency));

  // Calling UpdateFromMetaCH with an invalid header should
  // have no impact on client hint preferences.
  did_update = preferences.UpdateFromMetaCH(
      "1,,42", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
      /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
  EXPECT_FALSE(did_update);
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(network::mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAModel));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency));

  // Calling UpdateFromMetaCH with "width" header should
  // replace add width to preferences
  did_update = preferences.UpdateFromMetaCH(
      "width,sec-ch-width", kurl, nullptr,
      network::MetaCHType::HttpEquivAcceptCH,
      /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
  EXPECT_TRUE(did_update);
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(network::mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAModel));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency));

  // Calling UpdateFromMetaCH with empty header should not
  // change anything.
  did_update = preferences.UpdateFromMetaCH(
      "", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
      /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
  EXPECT_TRUE(did_update);
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED));
  EXPECT_TRUE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED));
  EXPECT_FALSE(preferences.ShouldSend(network::mojom::WebClientHintsType::kUA));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAArch));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAPlatform));
  EXPECT_FALSE(
      preferences.ShouldSend(network::mojom::WebClientHintsType::kUAModel));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion));
  EXPECT_FALSE(preferences.ShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency));
}

TEST(ClientHintsPreferencesTest, Insecure) {
  for (const auto& use_secure_url : {false, true}) {
    ClientHintsPreferences preferences;
    const KURL kurl = use_secure_url
                          ? KURL(String::FromUTF8("https://www.google.com/"))
                          : KURL(String::FromUTF8("http://www.google.com/"));
    bool did_update = preferences.UpdateFromMetaCH(
        "dpr", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
        /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
    EXPECT_EQ(did_update, use_secure_url);
    did_update = preferences.UpdateFromMetaCH(
        "sec-ch-dpr", kurl, nullptr, network::MetaCHType::HttpEquivAcceptCH,
        /*is_doc_preloader=*/true, /*is_sync_parser=*/true);
    EXPECT_EQ(did_update, use_secure_url);
    EXPECT_EQ(use_secure_url,
              preferences.ShouldSend(
                  network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_EQ(use_secure_url,
              preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr));
  }
}

// Verify that the client hints header and the lifetime header is parsed
// correctly.
TEST(ClientHintsPreferencesTest, ParseHeaders) {
  struct TestCase {
    const char* accept_ch_header_value;
    bool expect_device_memory_DEPRECATED;
    bool expect_device_memory;
    bool expect_width_DEPRECATED;
    bool expect_width;
    bool expect_dpr_DEPRECATED;
    bool expect_dpr;
    bool expect_viewport_width_DEPRECATED;
    bool expect_viewport_width;
    bool expect_rtt;
    bool expect_downlink;
    bool expect_ect;
    bool expect_ua;
    bool expect_ua_arch;
    bool expect_ua_platform;
    bool expect_ua_model;
    bool expect_ua_full_version;
    bool expect_prefers_color_scheme;
    bool expect_prefers_reduced_motion;
    bool expect_prefers_reduced_transparency;
  } test_cases[] = {
      {"width, sec-ch-width, dpr, sec-ch-dpr, viewportWidth, "
       "sec-ch-viewportWidth, sec-ch-prefers-color-scheme, "
       "sec-ch-prefers-reduced-motion, sec-ch-prefers-reduced-transparency",
       false,
       false,
       true,
       true,
       true,
       true,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       true,
       true,
       true},
      {"width, dpr, viewportWidth",
       false,
       false,
       true,
       false,
       true,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false},
      {"width, sec-ch-width, dpr, sec-ch-dpr, viewportWidth",
       false,
       false,
       true,
       true,
       true,
       true,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false},
      {"width, sec-ch-dpr, viewportWidth",
       false,
       false,
       true,
       false,
       false,
       true,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false},
      {"sec-ch-width, dpr, rtt, downlink, ect",
       false,
       false,
       false,
       true,
       true,
       false,
       false,
       false,
       true,
       true,
       true,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false},
      {"device-memory", true,  false, false, false, false, false,
       false,           false, false, false, false, false, false,
       false,           false, false, false, false, false},
      {"sec-ch-dpr rtt",
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false},
      {"sec-ch-ua, sec-ch-ua-arch, sec-ch-ua-platform, sec-ch-ua-model, "
       "sec-ch-ua-full-version",
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       false,
       true,
       true,
       true,
       true,
       true,
       false,
       false,
       false},
  };

  for (const auto& test : test_cases) {
    ClientHintsPreferences preferences;
    EnabledClientHints enabled_types = preferences.GetEnabledClientHints();
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kDeviceMemory));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_FALSE(
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kDpr));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kResourceWidth));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kViewportWidth));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kRtt_DEPRECATED));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kEct_DEPRECATED));
    EXPECT_FALSE(
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUA));
    EXPECT_FALSE(
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUAArch));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kUAPlatform));
    EXPECT_FALSE(
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUAModel));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kPrefersColorScheme));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kPrefersReducedMotion));
    EXPECT_FALSE(enabled_types.IsEnabled(
        network::mojom::WebClientHintsType::kPrefersReducedTransparency));

    const KURL kurl(String::FromUTF8("https://www.google.com/"));
    preferences.UpdateFromMetaCH(test.accept_ch_header_value, kurl, nullptr,
                                 network::MetaCHType::HttpEquivAcceptCH,
                                 /*is_doc_preloader=*/true,
                                 /*is_sync_parser=*/true);

    enabled_types = preferences.GetEnabledClientHints();

    EXPECT_EQ(
        test.expect_device_memory_DEPRECATED,
        enabled_types.IsEnabled(
            network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED));
    EXPECT_EQ(test.expect_device_memory,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kDeviceMemory));
    EXPECT_EQ(test.expect_dpr_DEPRECATED,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kDpr_DEPRECATED));
    EXPECT_EQ(test.expect_dpr, enabled_types.IsEnabled(
                                   network::mojom::WebClientHintsType::kDpr));
    EXPECT_EQ(
        test.expect_width_DEPRECATED,
        enabled_types.IsEnabled(
            network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
    EXPECT_EQ(test.expect_width,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kResourceWidth));
    EXPECT_EQ(
        test.expect_viewport_width_DEPRECATED,
        enabled_types.IsEnabled(
            network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
    EXPECT_EQ(test.expect_viewport_width,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kViewportWidth));
    EXPECT_EQ(test.expect_rtt,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));
    EXPECT_EQ(test.expect_downlink,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kDownlink_DEPRECATED));
    EXPECT_EQ(test.expect_ect,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kEct_DEPRECATED));
    EXPECT_EQ(test.expect_ua,
              enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUA));
    EXPECT_EQ(
        test.expect_ua_arch,
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUAArch));
    EXPECT_EQ(test.expect_ua_platform,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kUAPlatform));
    EXPECT_EQ(
        test.expect_ua_model,
        enabled_types.IsEnabled(network::mojom::WebClientHintsType::kUAModel));
    EXPECT_EQ(test.expect_prefers_color_scheme,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kPrefersColorScheme));
    EXPECT_EQ(test.expect_prefers_reduced_motion,
              enabled_types.IsEnabled(
                  network::mojom::WebClientHintsType::kPrefersReducedMotion));
    EXPECT_EQ(
        test.expect_prefers_reduced_transparency,
        enabled_types.IsEnabled(
            network::mojom::WebClientHintsType::kPrefersReducedTransparency));
  }
}

}  // namespace blink
