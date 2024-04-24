// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/host_zoom_map_impl.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace content {

class HostZoomMapImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // We must navigate so the WebContents has a committed entry.
    url_ = GURL(embedded_test_server()->GetURL("abc.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url_));

    host_zoom_map_impl_ = static_cast<HostZoomMapImpl*>(
        HostZoomMap::GetForWebContents(shell()->web_contents()));
  }
  void TearDownOnMainThread() override { host_zoom_map_impl_ = nullptr; }

  void RunTestForURL(double host_zoom_level, double temp_zoom_level) {
    WebContents* web_contents = shell()->web_contents();
    GlobalRenderFrameHostId rfh_id =
        web_contents->GetPrimaryMainFrame()->GetGlobalId();
    // Assume caller has set the zoom level to |host_zoom_level| using
    // either a host or host+scheme entry in the HostZoomMap prior to
    // calling this function.
    EXPECT_DOUBLE_EQ(host_zoom_level,
                     host_zoom_map_impl_->GetZoomLevel(web_contents));

    // Make sure that GetZoomLevel() works for temporary zoom levels.
    host_zoom_map_impl_->SetTemporaryZoomLevel(rfh_id, temp_zoom_level);
    EXPECT_DOUBLE_EQ(temp_zoom_level,
                     host_zoom_map_impl_->GetZoomLevel(web_contents));
    // Clear the temporary zoom level in case subsequent test calls use the same
    // web_contents.
    host_zoom_map_impl_->ClearTemporaryZoomLevel(rfh_id);
  }

  // Placeholder GURL to give WebContents instances a committed entry.
  GURL url_;

  // Customizable set of Features used for Android-specific tests.
  base::test::ScopedFeatureList feature_list_;

  // Instance of HostZoomMapImpl for convenience in tests.
  raw_ptr<HostZoomMapImpl> host_zoom_map_impl_;
};

#if BUILDFLAG(IS_ANDROID)
// For Android, there are experimental features that affect the value of zoom.
// These classes allow easy testing of the combination of enabled features.
class HostZoomMapImplBrowserTestWithPageZoom
    : public HostZoomMapImplBrowserTest {
 public:
  HostZoomMapImplBrowserTestWithPageZoom() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAccessibilityPageZoom, {{"AdjustForOSLevel", "true"}});
  }
};

class HostZoomMapImplBrowserTestWithPageZoomNoOSAdjustment
    : public HostZoomMapImplBrowserTest {
 public:
  HostZoomMapImplBrowserTestWithPageZoomNoOSAdjustment() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAccessibilityPageZoom, {{"AdjustForOSLevel", "false"}});
  }
};
#endif

// Test to make sure that GetZoomLevel() works properly for zoom levels
// stored by host value, and can distinguish temporary zoom levels from
// these.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTest, GetZoomForView_Host) {
  double default_zoom_level = host_zoom_map_impl_->GetDefaultZoomLevel();
  double host_zoom_level = default_zoom_level + 1.0;
  double temp_zoom_level = default_zoom_level + 2.0;

  host_zoom_map_impl_->SetZoomLevelForHost(url_.host(), host_zoom_level);

  RunTestForURL(host_zoom_level, temp_zoom_level);
}

// Test to make sure that GetZoomLevel() works properly for zoom levels
// stored by host and scheme values, and can distinguish temporary zoom levels
// from these.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTest,
                       GetZoomForView_HostAndScheme) {
  double default_zoom_level = host_zoom_map_impl_->GetDefaultZoomLevel();
  double host_zoom_level = default_zoom_level + 1.0;
  double temp_zoom_level = default_zoom_level + 2.0;

  host_zoom_map_impl_->SetZoomLevelForHostAndScheme(url_.scheme(), url_.host(),
                                                    host_zoom_level);

  RunTestForURL(host_zoom_level, temp_zoom_level);
}

#if BUILDFLAG(IS_ANDROID)
// Test to make sure that GetZoomLevelForHostAndSchemeAndroid() adjusts zoom
// level when there is a non-default OS-level font size setting on Android.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTestWithPageZoom,
                       GetZoomLevelForHostAndSchemeAndroid) {
  // At the default level, there should be no adjustment.
  EXPECT_DOUBLE_EQ(host_zoom_map_impl_->GetDefaultZoomLevel(),
                   host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                       url_.scheme(), url_.host()));

  // Test various levels of system font size.
  // A scale of 1.3 is equivalent to an Android OS font size of XL.
  // Zoom level will be 1.44 for exponential scale: 1.2 ^ 1.44 = 1.30.
  host_zoom_map_impl_->SetSystemFontScaleForTesting(1.30);
  EXPECT_DOUBLE_EQ(1.44,
                   host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                       url_.scheme(), url_.host()));

  host_zoom_map_impl_->SetSystemFontScaleForTesting(0.85);
  EXPECT_DOUBLE_EQ(-0.89,
                   host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                       url_.scheme(), url_.host()));

  host_zoom_map_impl_->SetSystemFontScaleForTesting(1.15);
  EXPECT_DOUBLE_EQ(0.77,
                   host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                       url_.scheme(), url_.host()));
}

// Same as above test but without the OS-level adjustment.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTestWithPageZoomNoOSAdjustment,
                       GetZoomLevelForHostAndSchemeAndroid) {
  // At the default level, there should be no adjustment.
  EXPECT_DOUBLE_EQ(host_zoom_map_impl_->GetDefaultZoomLevel(),
                   host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                       url_.scheme(), url_.host()));

  // Test various levels of system font size.
  // A scale of 1.3 is equivalent to an Android OS font size of XL.
  // Zoom level should remain zero because we are ignoring OS setting.
  host_zoom_map_impl_->SetSystemFontScaleForTesting(1.30);
  EXPECT_DOUBLE_EQ(0, host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                          url_.scheme(), url_.host()));

  host_zoom_map_impl_->SetSystemFontScaleForTesting(0.85);
  EXPECT_DOUBLE_EQ(0, host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                          url_.scheme(), url_.host()));

  host_zoom_map_impl_->SetSystemFontScaleForTesting(1.15);
  EXPECT_DOUBLE_EQ(0, host_zoom_map_impl_->GetZoomLevelForHostAndSchemeAndroid(
                          url_.scheme(), url_.host()));
}

#endif
}  // namespace content
