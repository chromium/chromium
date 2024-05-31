// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/color_chooser.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

// Mock content::ColorChooser to test whether End() is called.
class MockColorChooser : public content::ColorChooser {
 public:
  MockColorChooser() = default;

  MockColorChooser(const MockColorChooser&) = delete;
  MockColorChooser& operator=(const MockColorChooser&) = delete;

  ~MockColorChooser() override = default;

  MOCK_METHOD0(End, void());
  MOCK_METHOD1(SetSelectedColor, void(SkColor color));
};

// Delegate to override OpenColorChooser.
class OpenColorChooserDelegate : public WebContentsDelegate {
 public:
  explicit OpenColorChooserDelegate(
      std::unique_ptr<MockColorChooser> mock_color_chooser)
      : mock_color_chooser_(std::move(mock_color_chooser)) {}

  OpenColorChooserDelegate(const OpenColorChooserDelegate&) = delete;
  OpenColorChooserDelegate& operator=(const OpenColorChooserDelegate&) = delete;

  ~OpenColorChooserDelegate() override = default;

  // WebContentsDelegate:
  std::unique_ptr<ColorChooser> OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override {
    return std::move(mock_color_chooser_);
  }

  bool IsBackForwardCacheSupported(WebContents& web_contents) override {
    return true;
  }

 private:
  std::unique_ptr<MockColorChooser> mock_color_chooser_;
};

}  // namespace

class ColorChooserUnitTest : public RenderViewHostImplTestHarness {};

#if BUILDFLAG(IS_ANDROID)
// The ColorChooser is only available/called on Android.
TEST_F(ColorChooserUnitTest, ColorChooserCallsEndOnNavigatingAway) {
  GURL kUrl1("https://foo.com");
  GURL kUrl2("https://bar.com");

  // End should be called at least once on navigating to a new URL.
  std::unique_ptr<MockColorChooser> mock_color_chooser =
      std::make_unique<MockColorChooser>();
  EXPECT_CALL(*mock_color_chooser.get(), End()).Times(testing::AtLeast(1));

  // Set OpenColorChooserDelegate as the new WebContentsDelegate.
  std::unique_ptr<OpenColorChooserDelegate> delegate =
      std::make_unique<OpenColorChooserDelegate>(std::move(mock_color_chooser));
  contents()->SetDelegate(delegate.get());

  // Navigate to A.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl1);

  mojo::PendingRemote<blink::mojom::ColorChooserClient> pending_client;
  mojo::Remote<blink::mojom::ColorChooser> pending_remote;
  mojo::PendingReceiver<blink::mojom::ColorChooser> pending_receiver =
      pending_remote.BindNewPipeAndPassReceiver();

  // Call WebContentsImpl::OpenColorChooser.
  static_cast<WebContentsImpl*>(contents())
      ->OpenColorChooser(std::move(pending_receiver), std::move(pending_client),
                         SkColorSetRGB(0, 0, 1), {});

  // Navigate to B.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl2);

  contents()->SetDelegate(nullptr);
}
#endif

// Run tests with BackForwardCache.
class ColorChooserTestWithBackForwardCache : public ColorChooserUnitTest {
 public:
  ColorChooserTestWithBackForwardCache() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_ANDROID)
// The ColorChooser is only available/called on Android.
TEST_F(ColorChooserTestWithBackForwardCache,
       ColorChooserCallsEndOnEnteringBackForwardCache) {
  ASSERT_TRUE(IsBackForwardCacheEnabled());
  GURL kUrl1("https://foo.com");
  GURL kUrl2("https://bar.com");

  // End should be called at least once on navigating to a new URL.
  std::unique_ptr<MockColorChooser> mock_color_chooser =
      std::make_unique<MockColorChooser>();
  EXPECT_CALL(*mock_color_chooser.get(), End()).Times(testing::AtLeast(1));

  // Set OpenColorChooserDelegate as the new WebContentsDelegate.
  std::unique_ptr<OpenColorChooserDelegate> delegate =
      std::make_unique<OpenColorChooserDelegate>(std::move(mock_color_chooser));
  contents()->SetDelegate(delegate.get());

  // Navigate to A.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl1);
  RenderFrameHostImpl* rfh_a = contents()->GetPrimaryMainFrame();

  mojo::PendingRemote<blink::mojom::ColorChooserClient> pending_client;
  mojo::Remote<blink::mojom::ColorChooser> pending_remote;
  mojo::PendingReceiver<blink::mojom::ColorChooser> pending_receiver =
      pending_remote.BindNewPipeAndPassReceiver();

  // Call WebContentsImpl::OpenColorChooser.
  static_cast<WebContentsImpl*>(contents())
      ->OpenColorChooser(std::move(pending_receiver), std::move(pending_client),
                         SkColorSetRGB(0, 0, 1), {});

  // Navigate to B, A enters BackForwardCache.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl2);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  contents()->SetDelegate(nullptr);
}
#endif

}  // namespace content
