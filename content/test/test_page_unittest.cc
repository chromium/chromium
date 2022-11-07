// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_page.h"

#include "content/public/browser/web_contents_observer.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestPageTest : public RenderViewHostImplTestHarness,
                     public WebContentsObserver {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    Observe(RenderViewHostImplTestHarness::web_contents());
  }

  // WebContentsObserver
  void DidUpdateWebManifestURL(RenderFrameHost* target_frame,
                               const GURL& manifest_url) override {
    EXPECT_EQ(target_frame, main_rfh());
    updated_manifest_url_ = manifest_url;
  }

  const GURL& update_manifest_url() const { return updated_manifest_url_; }

 private:
  GURL updated_manifest_url_;
};

TEST_F(TestPageTest, UpdateManifestUrl) {
  TestPage& page = main_test_rfh()->GetPage();
  const GURL kFakeManifestURL = GURL("http://www.google.com/manifest.json");

  page.UpdateManifestUrl(kFakeManifestURL);
  auto& manifest_url = page.GetManifestUrl();

  EXPECT_TRUE(manifest_url.has_value());
  EXPECT_EQ(kFakeManifestURL, manifest_url.value());
  EXPECT_EQ(kFakeManifestURL, update_manifest_url());
}

}  // namespace content
