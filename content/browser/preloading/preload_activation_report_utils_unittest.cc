// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_utils.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "https://example.com";

// Generated token for https://example.com expiring at 2000000000.
constexpr char kSharedToken[] =
    "AwDxgoBi3BtnlfvEQ4Hamhy2AT4eLyQretRtF254uesFLoa/TbepvRdNhl6AH47lD80Cod6"
    "d70PKd7ODrOm86AEAAABueyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLC"
    "AiZmVhdHVyZSI6ICJQcmVmZXRjaEFuZFByZXJlbmRlckFjdGl2YXRpb25CZWFjb24iLCAiZ"
    "XhwaXJ5IjogMjAwMDAwMDAwMH0=";

}  // namespace

class PreloadActivationReportUtilsTest : public testing::Test {
 public:
  PreloadActivationReportUtilsTest() = default;

 private:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_DefaultDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // No trial token -> disabled.
  EXPECT_EQ(GetPrefetchActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kNoOriginTrialToken);

  // With trial token -> enabled.
  headers->AddHeader("Origin-Trial", kSharedToken);
  EXPECT_EQ(GetPrefetchActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kEnabled);
}

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_GloballyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrefetchActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Enabled by default (no token required).
  EXPECT_EQ(GetPrefetchActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kEnabled);
}

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_KillSwitched) {
  base::test::ScopedFeatureList override_feature_list;
  override_feature_list.InitAndDisableFeature(
      features::kPrefetchActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Origin-Trial", kSharedToken);

  // Even with trial token, the kill-switch (override to disabled) keeps it
  // disabled.
  EXPECT_EQ(GetPrefetchActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kDisabledByFeatureFlag);
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_DefaultDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // No trial token -> disabled.
  EXPECT_EQ(GetPrerenderActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kNoOriginTrialToken);

  // With trial token -> enabled.
  headers->AddHeader("Origin-Trial", kSharedToken);
  EXPECT_EQ(GetPrerenderActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kEnabled);
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_GloballyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrerenderActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Enabled by default (no token required).
  EXPECT_EQ(GetPrerenderActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kEnabled);
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_KillSwitched) {
  base::test::ScopedFeatureList override_feature_list;
  override_feature_list.InitAndDisableFeature(
      features::kPrerenderActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Origin-Trial", kSharedToken);

  // Even with trial token, the kill-switch keeps it disabled.
  EXPECT_EQ(GetPrerenderActivationBeaconFeatureState(url, headers.get()),
            PreloadActivationBeaconFeatureState::kDisabledByFeatureFlag);
}

}  // namespace content
