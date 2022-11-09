// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {
constexpr char kPageUrl[] = "http://example.com";
}  // namespace

class HostZoomMapAndroidTest : public content::RenderViewHostTestHarness {
 public:
  HostZoomMapAndroidTest() = default;

 protected:
  void SetUp() override;
};

void HostZoomMapAndroidTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  NavigateAndCommit(GURL(kPageUrl));
}

// Tests that the default desktop site zoom scale of 1.1 is returned when the
// desktop user agent is used, when Request Desktop Site Zoom is enabled.
TEST_F(HostZoomMapAndroidTest, GetDesktopSiteZoomScale_DesktopUserAgent) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kRequestDesktopSiteZoom);
  // Simulate the web contents to use the desktop user agent.
  web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);

  HostZoomMapImpl host_zoom_map;
  EXPECT_DOUBLE_EQ(1.1, host_zoom_map.GetDesktopSiteZoomScale(web_contents()));
}

// Tests that a Finch-configured desktop site zoom scale is returned when the
// desktop user agent is used, when Request Desktop Site Zoom is enabled.
TEST_F(HostZoomMapAndroidTest,
       GetDesktopSiteZoomScale_NonDefault_DesktopUserAgent) {
  base::test::ScopedFeatureList scoped_list;
  base::FieldTrialParams params{{"desktop_site_zoom_scale", "1.3"}};
  scoped_list.InitAndEnableFeatureWithParameters(
      features::kRequestDesktopSiteZoom, params);
  // Simulate the web contents to use the desktop user agent.
  web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);

  HostZoomMapImpl host_zoom_map;
  EXPECT_DOUBLE_EQ(1.3, host_zoom_map.GetDesktopSiteZoomScale(web_contents()));
}

// Tests that a desktop site zoom scale of 1.0 (no Request Desktop Site zoom) is
// returned when the mobile user agent is used, when Request Desktop Site Zoom
// is enabled.
TEST_F(HostZoomMapAndroidTest, GetDesktopSiteZoomScale_MobileUserAgent) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kRequestDesktopSiteZoom);
  // Simulate the web contents to use the mobile user agent.
  web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(false);

  HostZoomMapImpl host_zoom_map;
  EXPECT_DOUBLE_EQ(1.0, host_zoom_map.GetDesktopSiteZoomScale(web_contents()));
}

}  // namespace content
