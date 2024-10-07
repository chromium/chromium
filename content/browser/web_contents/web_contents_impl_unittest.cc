// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_page_broadcast.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_handle.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/test/test_network_context.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

namespace content {
namespace {

class WebContentsImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
      // Isolate |isolated_cross_site_url()| so it cannot share a process
      // with another site.
      ChildProcessSecurityPolicyImpl::GetInstance()->AddFutureIsolatedOrigins(
          {url::Origin::Create(isolated_cross_site_url())},
          ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
          browser_context());

      // Reset the WebContents so the isolated origin will be honored by
      // all BrowsingInstances used in the test.
      SetContents(CreateTestWebContents());
    }
  }

  bool has_audio_wake_lock() {
    return contents()
        ->media_web_contents_observer()
        ->has_audio_wake_lock_for_testing();
  }

  GURL isolated_cross_site_url() const {
    return GURL("http://isolated-cross-site.com");
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate LacrosService for WakeLock support.
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif
};

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents) {}

  TestWebContentsObserver(const TestWebContentsObserver&) = delete;
  TestWebContentsObserver& operator=(const TestWebContentsObserver&) = delete;

  ~TestWebContentsObserver() override {
    EXPECT_FALSE(expected_capture_handle_config_) << "Unfulfilled expectation.";
  }

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    last_url_ = validated_url;
  }
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    last_url_ = validated_url;
  }

  void DidFirstVisuallyNonEmptyPaint() override {
    observed_did_first_visually_non_empty_paint_ = true;
    EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  }

  void DidChangeThemeColor() override { ++theme_color_change_calls_; }

  void DidChangeVerticalScrollDirection(
      viz::VerticalScrollDirection scroll_direction) override {
    last_vertical_scroll_direction_ = scroll_direction;
  }

  MOCK_METHOD(void,
              OnDeviceConnectionTypesChanged,
              (DeviceConnectionType connection_type, bool used),
              (override));

  void OnCaptureHandleConfigUpdate(
      const blink::mojom::CaptureHandleConfig& config) override {
    ASSERT_TRUE(expected_capture_handle_config_) << "Unexpected call.";
    EXPECT_EQ(config, *expected_capture_handle_config_);
    expected_capture_handle_config_ = nullptr;
  }

  void OnTextCopiedToClipboard(RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override {
    text_copied_to_clipboard_ = copied_text;
  }

  void ExpectOnCaptureHandleConfigUpdate(
      blink::mojom::CaptureHandleConfigPtr config) {
    CHECK(config) << "Malformed test.";
    ASSERT_FALSE(expected_capture_handle_config_) << "Unfulfilled expectation.";
    expected_capture_handle_config_ = std::move(config);
  }

  const GURL& last_url() const { return last_url_; }
  int theme_color_change_calls() const { return theme_color_change_calls_; }
  std::optional<viz::VerticalScrollDirection> last_vertical_scroll_direction()
      const {
    return last_vertical_scroll_direction_;
  }
  bool observed_did_first_visually_non_empty_paint() const {
    return observed_did_first_visually_non_empty_paint_;
  }

  const std::u16string text_copied_to_clipboard() const {
    return text_copied_to_clipboard_;
  }

 private:
  GURL last_url_;
  int theme_color_change_calls_ = 0;
  std::optional<viz::VerticalScrollDirection> last_vertical_scroll_direction_;
  bool observed_did_first_visually_non_empty_paint_ = false;
  blink::mojom::CaptureHandleConfigPtr expected_capture_handle_config_;
  std::u16string text_copied_to_clipboard_;
};

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit MockWebContentsDelegate(
      blink::ProtocolHandlerSecurityLevel security_level =
          blink::ProtocolHandlerSecurityLevel::kStrict)
      : security_level_(security_level) {}
  MOCK_METHOD2(HandleContextMenu,
               bool(RenderFrameHost&, const ContextMenuParams&));
  MOCK_METHOD4(RegisterProtocolHandler,
               void(RenderFrameHost*, const std::string&, const GURL&, bool));
  MOCK_METHOD(void, NavigationStateChanged, (WebContents*, InvalidateTypes));

  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      RenderFrameHost*) override {
    return security_level_;
  }

 private:
  blink::ProtocolHandlerSecurityLevel security_level_;
};

// Pretends to be a normal browser that receives toggles and transitions to/from
// a fullscreened state.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() : fullscreened_contents_(nullptr) {}

  FakeFullscreenDelegate(const FakeFullscreenDelegate&) = delete;
  FakeFullscreenDelegate& operator=(const FakeFullscreenDelegate&) = delete;

  ~FakeFullscreenDelegate() override = default;

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    fullscreened_contents_ = WebContents::FromRenderFrameHost(requesting_frame);
  }

  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    fullscreened_contents_ = nullptr;
  }

  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return fullscreened_contents_ && web_contents == fullscreened_contents_;
  }

 private:
  raw_ptr<WebContents> fullscreened_contents_;
};

class FakeWebContentsDelegate : public WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;

  FakeWebContentsDelegate(const FakeWebContentsDelegate&) = delete;
  FakeWebContentsDelegate& operator=(const FakeWebContentsDelegate&) = delete;

  ~FakeWebContentsDelegate() override = default;

  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) override {
    loading_state_changed_was_called_ = true;
  }

  bool loading_state_changed_was_called() const {
    return loading_state_changed_was_called_;
  }

 private:
  bool loading_state_changed_was_called_ = false;
};

class FakeImageDownloader : public blink::mojom::ImageDownloader {
 public:
  FakeImageDownloader() = default;
  ~FakeImageDownloader() override = default;

  void Init(service_manager::InterfaceProvider* interface_provider) {
    service_manager::InterfaceProvider::TestApi test_api(interface_provider);
    test_api.SetBinderForName(blink::mojom::ImageDownloader::Name_,
                              base::BindRepeating(&FakeImageDownloader::Bind,
                                                  base::Unretained(this)));
  }

  void DownloadImage(const GURL& url,
                     bool is_favicon,
                     const gfx::Size& preferred_size,
                     uint32_t max_bitmap_size,
                     bool bypass_cache,
                     DownloadImageCallback callback) override {
    if (!base::Contains(fake_response_data_per_url_, url)) {
      // This could return a 404, but there is no test that currently relies on
      // it.
      return;
    }

    const FakeResponseData& response_data = fake_response_data_per_url_[url];
    std::move(callback).Run(/*http_status_code=*/200, response_data.bitmaps,
                            response_data.original_bitmap_sizes);
  }

  void DownloadImageFromAxNode(
      int32_t ax_node_id,
      const ::gfx::Size& preferred_size,
      uint32_t max_bitmap_size,
      bool bypass_cache,
      DownloadImageFromAxNodeCallback callback) override {
    if (!base::Contains(fake_response_data_per_ax_node_id_, ax_node_id)) {
      // This could return a 404, but there is no test that currently relies on
      // it.
      return;
    }

    const FakeResponseData& response_data =
        fake_response_data_per_ax_node_id_[ax_node_id];
    std::move(callback).Run(/*http_status_code=*/0, response_data.bitmaps,
                            response_data.original_bitmap_sizes);
  }

  void SetFakeResponseData(
      const GURL& url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes) {
    fake_response_data_per_url_[url] =
        FakeResponseData{bitmaps, original_bitmap_sizes};
  }

  void SetFakeResponseData(
      const int ax_node_id,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes) {
    fake_response_data_per_ax_node_id_[ax_node_id] =
        FakeResponseData{bitmaps, original_bitmap_sizes};
  }

 private:
  struct FakeResponseData {
    std::vector<SkBitmap> bitmaps;
    std::vector<gfx::Size> original_bitmap_sizes;
  };

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::ImageDownloader>(
        std::move(handle)));
  }

  mojo::Receiver<blink::mojom::ImageDownloader> receiver_{this};
  std::map<GURL, FakeResponseData> fake_response_data_per_url_;
  std::map<int, FakeResponseData> fake_response_data_per_ax_node_id_;
};

class MockPageBroadcast : public TestPageBroadcast {
 public:
  using TestPageBroadcast::TestPageBroadcast;
  MOCK_METHOD(void,
              UpdateColorProviders,
              (const blink::ColorProviderColorMaps& color_provider_colors),
              (override));
};

class TestColorProviderSource : public ui::ColorProviderSource {
 public:
  TestColorProviderSource() = default;

  const ui::ColorProvider* GetColorProvider() const override {
    return &provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    if (forced_colors == ui::ColorProviderKey::ForcedColors::kActive) {
      return forced_colors_map;
    }
    return color_mode == ui::ColorProviderKey::ColorMode::kLight ? light_colors
                                                                 : dark_colors;
  }

  ui::ColorProviderKey GetColorProviderKey() const override { return key_; }

 private:
  ui::ColorProvider provider_;
  ui::ColorProviderKey key_;
  const ui::RendererColorMap light_colors{
      {color::mojom::RendererColorId::kColorMenuBackground, SK_ColorWHITE}};
  const ui::RendererColorMap dark_colors{
      {color::mojom::RendererColorId::kColorMenuBackground, SK_ColorBLACK}};
  const ui::RendererColorMap forced_colors_map{
      {color::mojom::RendererColorId::kColorMenuBackground, SK_ColorCYAN}};
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : receiver_(this, std::move(receiver)) {}
  MOCK_METHOD(
      void,
      NotifyExternalCacheHit,
      (const GURL&, const std::string&, const net::NetworkIsolationKey&, bool),
      (override));

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};

}  // namespace

TEST_F(WebContentsImplTest, SetMainFrameMimeType) {
  ASSERT_TRUE(controller().IsInitialNavigation());
  std::string mime = "text/html";
  main_test_rfh()->GetPage().SetContentsMimeType(mime);
  EXPECT_EQ(mime, contents()->GetContentsMimeType());
}

TEST_F(WebContentsImplTest, UpdateTitle) {
  FakeWebContentsDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  NavigationControllerImpl& cont =
      static_cast<NavigationControllerImpl&>(controller());
  cont.LoadURL(GURL(url::kAboutBlankURL), Referrer(), ui::PAGE_TRANSITION_TYPED,
               std::string());

  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->url = GURL(url::kAboutBlankURL);
  params->origin = url::Origin::Create(params->url);
  params->referrer = blink::mojom::Referrer::New();
  params->transition = ui::PAGE_TRANSITION_TYPED;
  params->should_update_history = false;
  params->did_create_new_entry = true;
  params->method = "GET";
  params->page_state = blink::PageState::CreateFromURL(params->url);

  main_test_rfh()->SendNavigateWithParams(std::move(params),
                                          false /* was_within_same_document */);

  contents()->UpdateTitle(main_test_rfh(), u"    Lots O' Whitespace\n",
                          base::i18n::LEFT_TO_RIGHT);
  // Make sure that title updates get stripped of whitespace.
  EXPECT_EQ(u"Lots O' Whitespace", contents()->GetTitle());
  EXPECT_FALSE(contents()->IsWaitingForResponse());
  EXPECT_TRUE(fake_delegate.loading_state_changed_was_called());

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, UpdateTitleBeforeFirstNavigation) {
  ASSERT_TRUE(controller().IsInitialNavigation());
  const std::u16string title = u"Initial Entry Title";
  contents()->UpdateTitle(main_test_rfh(), title, base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UpdateTitleWhileFirstNavigationIsPending) {
  const GURL kGURL(GetWebUIURL("blah"));
  controller().LoadURL(kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  ASSERT_TRUE(!!controller().GetPendingEntry());
  const std::u16string title = u"Initial Entry Title";
  contents()->UpdateTitle(main_test_rfh(), title, base::i18n::LEFT_TO_RIGHT);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest, DontUsePendingEntryUrlAsTitle) {
  const GURL kGURL(GetWebUIURL("blah"));
  controller().LoadURL(kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  EXPECT_EQ(std::u16string(), contents()->GetTitle());
}

TEST_F(WebContentsImplTest, UpdateAndUseTitleFromFirstNavigationPendingEntry) {
  const GURL kGURL(GetWebUIURL("blah"));
  controller().LoadURL(kGURL, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);
  EXPECT_CALL(delegate,
              NavigationStateChanged(contents(), INVALIDATE_TYPE_TITLE));

  const std::u16string title = u"Initial Entry Title";
  contents()->UpdateTitleForEntry(controller().GetPendingEntry(), title);
  EXPECT_EQ(title, contents()->GetTitle());
}

TEST_F(WebContentsImplTest,
       UpdateAndDontUseTitleFromPendingEntryForSecondNavigation) {
  const GURL first_gurl("http://www.foo.com");
  const GURL second_gurl("http://www.bar.com");

  // Complete first navigation.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), first_gurl);
  std::u16string first_title = contents()->GetTitle();

  // Start second navigation.
  controller().LoadURL(second_gurl, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  // We shouldn't use the title of the second navigation's pending entry, even
  // after explicitly setting it - we only use the pending entry's title if it's
  // for the first navigation.
  contents()->UpdateTitleForEntry(controller().GetPendingEntry(), u"bar");
  EXPECT_EQ(contents()->GetTitle(), first_title);
}

// Stub out local frame mojo binding. Intercepts calls to EnableViewSourceMode
// and marks the message as received. This class attaches to the first
// RenderFrameHostImpl created.
class EnableViewSourceLocalFrame : public content::FakeLocalFrame,
                                   public WebContentsObserver {
 public:
  explicit EnableViewSourceLocalFrame(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (!initialized_) {
      initialized_ = true;
      Init(navigation_handle->GetRenderFrameHost()
               ->GetRemoteAssociatedInterfaces());
    }
  }

  void EnableViewSourceMode() final { enabled_view_source_ = true; }

  bool IsViewSourceModeEnabled() const { return enabled_view_source_; }

 private:
  bool enabled_view_source_ = false;
  bool initialized_ = false;
};

// Browser initiated navigations to view-source URLs of WebUI pages should work.
TEST_F(WebContentsImplTest, DirectNavigationToViewSourceWebUI) {
  const GURL kGURL("view-source:" + GetWebUIURLString("blah/"));
  // NavigationControllerImpl rewrites view-source URLs, simulating that here.
  const GURL kRewrittenURL(GetWebUIURL("blah"));

  EnableViewSourceLocalFrame local_frame(contents());
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kGURL);

  // Did we get the expected message?
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(local_frame.IsViewSourceModeEnabled());

  // This is the virtual URL.
  EXPECT_EQ(
      kGURL,
      contents()->GetController().GetLastCommittedEntry()->GetVirtualURL());

  // The actual URL navigated to.
  EXPECT_EQ(kRewrittenURL,
            contents()->GetController().GetLastCommittedEntry()->GetURL());
}

// Test simple same-SiteInstance navigation.
TEST_F(WebContentsImplTest, SimpleNavigation) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();
  EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());

  // Navigate until ready to commit.
  const GURL url("http://www.google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's pending entry will have a null site instance until we assign
  // it in Commit.
  EXPECT_EQ(nullptr, NavigationEntryImpl::FromNavigationEntry(
                         controller().GetVisibleEntry())
                         ->site_instance());

  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(instance1, orig_rfh->GetSiteInstance());
  // Controller's entry should now have the SiteInstance, or else we won't be
  // able to find it later.
  EXPECT_EQ(instance1, NavigationEntryImpl::FromNavigationEntry(
                           controller().GetVisibleEntry())
                           ->site_instance());
}

// Test that we reject NavigateToEntry if the url is over kMaxURLChars.
TEST_F(WebContentsImplTest, NavigateToExcessivelyLongURL) {
  // Construct a URL that's kMaxURLChars + 1 long of all 'a's.
  const GURL url(
      std::string("http://example.org/").append(url::kMaxURLChars + 1, 'a'));

  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_GENERATED,
                       std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());
}

// Test that we reject NavigateToEntry if the url is invalid.
TEST_F(WebContentsImplTest, NavigateToInvalidURL) {
  // Invalid URLs should not trigger a navigation.
  const GURL invalid_url("view-source:http://example%00.com/");
  controller().LoadURL(invalid_url, Referrer(), ui::PAGE_TRANSITION_GENERATED,
                       std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Empty URLs are supported and should start a navigation.
  controller().LoadURL(GURL(), Referrer(), ui::PAGE_TRANSITION_GENERATED,
                       std::string());
  EXPECT_NE(nullptr, controller().GetPendingEntry());
}

// Test that we reject NavigateToEntry if the url is a renderer debug URL
// inside a view-source: URL. This verifies that the navigation is not allowed
// to proceed after the view-source: URL rewriting logic has run.
TEST_F(WebContentsImplTest, NavigateToViewSourceRendererDebugURL) {
  const GURL renderer_debug_url(blink::kChromeUIKillURL);
  const GURL view_source_debug_url("view-source:" + renderer_debug_url.spec());
  EXPECT_TRUE(blink::IsRendererDebugURL(renderer_debug_url));
  EXPECT_FALSE(blink::IsRendererDebugURL(view_source_debug_url));
  controller().LoadURL(view_source_debug_url, Referrer(),
                       ui::PAGE_TRANSITION_GENERATED, std::string());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());
}

// Test that navigating across a site boundary creates a new RenderViewHost
// with a new SiteInstance.  Going back should do the same.
TEST_F(WebContentsImplTest, CrossSiteBoundaries) {
  // This test assumes no interaction with the back forward cache.
  // Similar coverage when BFCache is on can be found in
  // BackForwardCacheBrowserTest.NavigateBackForwardRepeatedly.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstanceImpl* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Keep the number of active frames in orig_rfh's SiteInstanceGroup non-zero
  // so that orig_rfh doesn't get deleted when it gets swapped out.
  orig_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());

  // Navigate to new site
  const GURL url2("http://www.yahoo.com");
  auto new_site_navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  new_site_navigation->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  EXPECT_TRUE(pending_rfh->GetLastCommittedURL().is_empty());
  int pending_rvh_delete_count = 0;
  pending_rfh->GetRenderViewHost()->set_delete_counter(
      &pending_rvh_delete_count);

  // DidNavigate from the pending page.
  new_site_navigation->Commit();
  SiteInstanceImpl* instance2 = contents()->GetSiteInstance();

  // Keep the number of active frames in pending_rfh's SiteInstanceGroup
  // non-zero so that orig_rfh doesn't get deleted when it gets
  // swapped out.
  pending_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());
  // We keep a proxy for the original RFH's SiteInstanceGroup.
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(instance1->group()));
  EXPECT_EQ(orig_rvh_delete_count, 0);

  // Going back should switch SiteInstances again.  The first SiteInstance is
  // stored in the NavigationEntry, so it should be the same as at the start.
  // We should use the same RFH as before, swapping it back in.
  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->ReadyToCommit();
  TestRenderFrameHost* goback_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // DidNavigate from the back action.
  back_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(goback_rfh, main_test_rfh());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_EQ(instance1, contents()->GetSiteInstance());
  // There should be a proxy for the pending RFH SiteInstance.
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(instance2->group()));
  EXPECT_EQ(pending_rvh_delete_count, 0);

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
  EXPECT_EQ(pending_rvh_delete_count, 1);
}

// Test that navigating across a site boundary after a crash creates a new
// RFH without requiring a cross-site transition (i.e., PENDING state).
TEST_F(WebContentsImplTest, CrossSiteBoundariesAfterCrash) {
  // Ensure that the cross-site transition will also be cross-process on
  // Android.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  TestRenderFrameHost* orig_rfh = main_test_rfh();

  int orig_rvh_delete_count = 0;
  orig_rfh->GetRenderViewHost()->set_delete_counter(&orig_rvh_delete_count);
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh->GetRenderViewHost(), contents()->GetRenderViewHost());

  // Simulate a renderer crash.
  EXPECT_TRUE(orig_rfh->IsRenderFrameLive());
  orig_rfh->GetProcess()->SimulateCrash();
  EXPECT_FALSE(orig_rfh->IsRenderFrameLive());

  // Start navigating to a new site. We should not go into PENDING.
  const GURL url2("http://www.yahoo.com");
  auto navigation_to_url2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation_to_url2->ReadyToCommit();

  TestRenderFrameHost* new_rfh = main_test_rfh();
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    EXPECT_TRUE(contents()->CrossProcessNavigationPending());
    EXPECT_NE(nullptr, contents()->GetSpeculativePrimaryMainFrame());
    EXPECT_EQ(orig_rfh, new_rfh);
    EXPECT_EQ(orig_rvh_delete_count, 0);
  } else {
    EXPECT_FALSE(contents()->CrossProcessNavigationPending());
    EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());
    EXPECT_NE(orig_rfh, new_rfh);
    EXPECT_EQ(orig_rvh_delete_count, 1);
  }

  navigation_to_url2->Commit();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    EXPECT_NE(new_rfh, main_rfh());
  } else {
    EXPECT_EQ(new_rfh, main_rfh());
  }
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());

  // Close contents and ensure RVHs are deleted.
  DeleteContents();
  EXPECT_EQ(orig_rvh_delete_count, 1);
}

// Test that opening a new contents in the same SiteInstance and then navigating
// both contentses to a new site will place both contentses in a single
// SiteInstance.
TEST_F(WebContentsImplTest, NavigateTwoTabsCrossSite) {
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Open a new contents with the same SiteInstance, navigated to the same site.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents2.get(), url);
  EXPECT_EQ(instance1, contents2->GetSiteInstance());

  // Navigate first contents to a new site.
  const GURL url2a = isolated_cross_site_url();
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url2a, contents());
  navigation1->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation1->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  navigation1->Commit();
  SiteInstance* instance2a = contents()->GetSiteInstance();
  EXPECT_NE(instance1, instance2a);

  // Navigate second contents to the same site as the first tab.
  const GURL url2b = isolated_cross_site_url().Resolve("/foo");
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2b, contents2.get());
  navigation2->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation2->ReadyToCommit();
  EXPECT_TRUE(contents2->CrossProcessNavigationPending());

  // NOTE(creis): We used to be in danger of showing a crash page here if the
  // second contents hadn't navigated somewhere first (bug 1145430).  That case
  // is now covered by the CrossSiteBoundariesAfterCrash test.
  navigation2->Commit();
  SiteInstance* instance2b = contents2->GetSiteInstance();
  EXPECT_NE(instance1, instance2b);

  // Both contentses should now be in the same SiteInstance.
  EXPECT_EQ(instance2a, instance2b);
}

// The embedder can request sites for certain urls not be be assigned to the
// SiteInstance by adding their schemes as empty document schemes,
// allowing to reuse the renderer backing certain chrome urls for subsequent
// navigation. The test verifies that the override is honored.
TEST_F(WebContentsImplTest, NavigateFromSitelessUrl) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();

  // Navigate to an URL that will not assign a new SiteInstance.
  // The url also needs to be defined with an empty scheme.
  url::ScopedSchemeRegistryForTests scheme_registry;
  url::AddEmptyDocumentScheme("non-site-url");
  const GURL native_url("non-site-url://stuffandthings");
  EXPECT_FALSE(SiteInstance::ShouldAssignSiteForURL(native_url));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), native_url);

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(native_url, contents()->GetVisibleURL());
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  // Navigate to new site (should keep same site instance, but might change
  // RenderFrameHosts).
  const GURL url("http://www.google.com");
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation1->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(native_url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url, contents()->GetVisibleURL());
  EXPECT_EQ(ShouldCreateNewHostForAllFrames(),
            !!contents()->GetSpeculativePrimaryMainFrame());
  navigation1->Commit();

  // The first entry's SiteInstance should be reset to a new, related one. This
  // prevents wrongly detecting a SiteInstance mismatch when returning to it
  // later.
  SiteInstanceImpl* prev_entry_instance = contents()
                                              ->GetController()
                                              .GetEntryAtIndex(0)
                                              ->root_node()
                                              ->frame_entry->site_instance();
  EXPECT_NE(prev_entry_instance, orig_instance);
  EXPECT_TRUE(orig_instance->IsRelatedSiteInstance(prev_entry_instance));
  EXPECT_FALSE(prev_entry_instance->HasSite());

  SiteInstanceImpl* curr_entry_instance = contents()
                                              ->GetController()
                                              .GetEntryAtIndex(1)
                                              ->root_node()
                                              ->frame_entry->site_instance();
  EXPECT_EQ(curr_entry_instance, orig_instance);
  // Keep the number of active frames in the current RFH's SiteInstanceGroup
  // non-zero so that the RFH doesn't get deleted when it gets
  // swapped out.
  main_test_rfh()->GetSiteInstance()->group()->IncrementActiveFrameCount();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that the empty SiteInstance gets converted into a default
    // SiteInstance because |url| does not require a dedicated process.
    EXPECT_TRUE(contents()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_TRUE(
        contents()->GetSiteInstance()->GetSiteURL().DomainIs("google.com"));
  }
  EXPECT_EQ(url, contents()->GetLastCommittedURL());

  // Navigate to another new site (should create a new site instance).
  const GURL url2 = isolated_cross_site_url();
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  // DidNavigate from the pending page.
  navigation2->Commit();
  SiteInstance* new_instance = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(url2, contents()->GetLastCommittedURL());
  EXPECT_EQ(url2, contents()->GetVisibleURL());
  EXPECT_NE(new_instance, orig_instance);
  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());
}

// Regression test for http://crbug.com/386542 - variation of
// NavigateFromSitelessUrl in which the original navigation is a session
// restore.
TEST_F(WebContentsImplTest, NavigateFromRestoredSitelessUrl) {
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();

  // Restore a navigation entry for URL that should not assign site to the
  // SiteInstance. The url also needs to be defined with an empty scheme.
  url::ScopedSchemeRegistryForTests scheme_registry;
  url::AddEmptyDocumentScheme("non-site-url");
  const GURL native_url("non-site-url://stuffandthings");
  EXPECT_FALSE(SiteInstance::ShouldAssignSiteForURL(native_url));
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          native_url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_LINK,
          false, std::string(), browser_context(),
          nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller().Restore(0, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  ASSERT_EQ(1, controller().GetEntryCount());

  EXPECT_TRUE(controller().NeedsReload());
  controller().LoadIfNecessary();
  auto navigation = NavigationSimulator::CreateFromPending(controller());
  navigation->Commit();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_EQ(GURL(), contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(orig_instance->HasSite());

  // Navigate to a regular site and verify that the SiteInstance was kept.
  const GURL url("http://www.google.com");
  EXPECT_TRUE(SiteInstance::ShouldAssignSiteForURL(url));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());

  // Cleanup.
  DeleteContents();
}

// Complement for NavigateFromRestoredSitelessUrl, verifying that when a regular
// tab is restored, the SiteInstance will change upon navigation.
TEST_F(WebContentsImplTest, NavigateFromRestoredRegularUrl) {
  SiteInstanceImpl* orig_instance = contents()->GetSiteInstance();

  // Restore a navigation entry for a regular URL with non-empty scheme, where
  // ShouldAssignSiteForUrl returns true.
  const GURL regular_url("http://www.yahoo.com");
  EXPECT_TRUE(SiteInstance::ShouldAssignSiteForURL(regular_url));
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          regular_url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_LINK,
          false, std::string(), browser_context(),
          nullptr /* blob_url_loader_factory */);
  entries.push_back(std::move(new_entry));
  controller().Restore(0, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());

  ASSERT_EQ(1, controller().GetEntryCount());
  EXPECT_TRUE(controller().NeedsReload());
  controller().LoadIfNecessary();
  auto navigation = NavigationSimulator::CreateFromPending(controller());
  navigation->Commit();

  EXPECT_EQ(orig_instance, contents()->GetSiteInstance());
  EXPECT_TRUE(orig_instance->HasSite());
  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            orig_instance->IsDefaultSiteInstance());

  // Navigate to another site and verify that a new SiteInstance was created.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify this remains the default SiteInstance since |url| does
    // not require a dedicated process.
    EXPECT_TRUE(contents()->GetSiteInstance()->IsDefaultSiteInstance());

    // Navigate to a URL that does require a dedicated process and verify that
    // the SiteInstance changes.
    NavigationSimulator::NavigateAndCommitFromBrowser(
        contents(), isolated_cross_site_url());
    EXPECT_NE(orig_instance, contents()->GetSiteInstance());
  } else {
    EXPECT_NE(orig_instance, contents()->GetSiteInstance());
  }

  // Cleanup.
  DeleteContents();
}

// Test that we can find an opener RVH even if it's pending.
// http://crbug.com/176252.
TEST_F(WebContentsImplTest, FindOpenerRVHWhenPending) {
  // Navigate to a URL.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Start to navigate first tab to a new site, so that it has a pending RVH.
  const GURL url2("http://www.yahoo.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation->ReadyToCommit();
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  SiteInstanceImpl* instance = pending_rfh->GetSiteInstance();

  // While it is still pending, simulate opening a new tab with the first tab
  // as its opener.  This will call CreateOpenerProxies on the opener to ensure
  // that an RVH exists.
  std::unique_ptr<TestWebContents> popup(
      TestWebContents::Create(browser_context(), instance));
  popup->SetOpener(contents());
  contents()->GetRenderManager()->CreateOpenerProxies(
      instance->group(), nullptr, pending_rfh->browsing_context_state());

  // If swapped out is forbidden, a new proxy should be created for the opener
  // in the group |instance| belongs to, and we should ensure that its routing
  // ID is returned here. Otherwise, we should find the pending RFH and not
  // create a new proxy.
  auto opener_frame_token =
      popup->GetRenderManager()->GetOpenerFrameToken(instance->group());
  RenderFrameProxyHost* proxy =
      contents()
          ->GetRenderManager()
          ->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(instance->group());
  EXPECT_TRUE(proxy);
  EXPECT_EQ(*opener_frame_token, proxy->GetFrameToken());

  // Ensure that committing the navigation removes the proxy.
  navigation->Commit();
  EXPECT_FALSE(contents()
                   ->GetPrimaryMainFrame()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(instance->group()));
}

// Tests that WebContentsImpl uses the current URL, not the SiteInstance's site,
// to determine whether a navigation is cross-site.
TEST_F(WebContentsImplTest, CrossSiteComparesAgainstCurrentPage) {
  // The assumptions this test makes aren't valid with --site-per-process.  For
  // example, a cross-site URL won't ever commit in the old RFH.  The test also
  // assumes that default SiteInstances are enabled, and that aggressive
  // BrowsingInstance swapping (even on renderer-initiated navigations) is
  // disabled.
  if (AreAllSitesIsolatedForTesting() || !AreDefaultSiteInstancesEnabled() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    return;
  }

  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstanceImpl* instance1 = contents()->GetSiteInstance();

  const GURL url("http://www.google.com");

  // Navigate to URL.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Open a related contents to a second site.
  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1));
  const GURL url2("http://www.yahoo.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents2.get());
  navigation->ReadyToCommit();

  // The first RVH in contents2 isn't live yet, so we shortcut the cross site
  // pending.
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());
  navigation->Commit();
  SiteInstance* instance2 = contents2->GetSiteInstance();
  // With default SiteInstances, navigations in both tabs should
  // share the same default SiteInstance, since neither requires a dedicated
  // process.
  EXPECT_EQ(instance1, instance2);
  EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  EXPECT_FALSE(contents2->CrossProcessNavigationPending());

  // Simulate a link click in first contents to second site.  This doesn't
  // switch SiteInstances and stays in the default SiteInstance.
  NavigationSimulator::NavigateAndCommitFromDocument(url2, orig_rfh);
  SiteInstance* instance3 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance3);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate same-site.  This also stays in the default SiteInstance.
  const GURL url3("http://mail.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  SiteInstance* instance4 = contents()->GetSiteInstance();
  EXPECT_EQ(instance1, instance4);
}

// Test that the onbeforeunload and onunload handlers run when navigating
// across site boundaries.
TEST_F(WebContentsImplTest, CrossSiteUnloadHandlers) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate to new site, but simulate an onbeforeunload denial.
  const GURL url2("http://www.yahoo.com");
  orig_rfh->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);
  controller().LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  orig_rfh->SimulateBeforeUnloadCompleted(false);
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate again, but simulate an onbeforeunload approval.
  controller().LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  auto navigation =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  navigation->ReadyToCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();

  // DidNavigate from the pending page.
  navigation->Commit();
  SiteInstance* instance2 = contents()->GetSiteInstance();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_NE(instance1, instance2);
  EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());
}

// Test that during a slow cross-site navigation, the original renderer can
// navigate to a different URL and have it displayed, canceling the slow
// navigation.
TEST_F(WebContentsImplTest, CrossSiteNavigationPreempted) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  // Navigate to URL.  First URL should use first RenderFrameHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Navigate to new site.
  const GURL url2("http://www.yahoo.com");
  orig_rfh->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);
  controller().LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());

  // Suppose the original renderer navigates before the new one is ready.
  const GURL url3("http://www.google.com/foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url3, orig_rfh);

  // Verify that the pending navigation is cancelled.
  SiteInstance* instance2 = contents()->GetSiteInstance();
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If same-site ProactivelySwapBrowsingInstance or main-frame RenderDocument
    // is enabled, the RFH should change.
    EXPECT_NE(orig_rfh, main_test_rfh());
  } else {
    EXPECT_EQ(orig_rfh, main_test_rfh());
  }
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the SiteInstance will change.
    EXPECT_NE(instance1, instance2);
  } else {
    EXPECT_EQ(instance1, instance2);
  }
  EXPECT_FALSE(main_test_rfh()->is_waiting_for_beforeunload_completion());
  EXPECT_EQ(main_test_rfh()->GetLastCommittedURL(), url3);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(nullptr, contents()->GetSpeculativePrimaryMainFrame());
}

// Tests that if we go back twice (same-site then cross-site), and the cross-
// site RFH commits first, we ignore the now-swapped-out RFH's commit.
TEST_F(WebContentsImplTest, CrossSiteNavigationBackOldNavigationIgnored) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // This test assumes no interaction with the back/forward cache. Indeed, it
  // isn't possible to perform the second back navigation in between the
  // ReadyToCommit and Commit of the first back/forward cache one. Both steps
  // are combined with it, nothing can happen in between.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Start with a web ui page, which gets a new RFH with WebUI bindings.
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  TestRenderFrameHost* webui_rfh = main_test_rfh();
  NavigationEntry* entry1 = controller().GetLastCommittedEntry();
  SiteInstance* instance1 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(url1, entry1->GetURL());
  EXPECT_EQ(instance1,
            NavigationEntryImpl::FromNavigationEntry(entry1)->site_instance());
  EXPECT_TRUE(webui_rfh->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  // Navigate to new site.
  const GURL url2("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  TestRenderFrameHost* google_rfh = main_test_rfh();
  NavigationEntry* entry2 = controller().GetLastCommittedEntry();
  SiteInstance* instance2 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_NE(instance1, instance2);
  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());
  EXPECT_EQ(url2, entry2->GetURL());
  EXPECT_EQ(instance2,
            NavigationEntryImpl::FromNavigationEntry(entry2)->site_instance());
  EXPECT_FALSE(
      google_rfh->GetEnabledBindings().Has(BindingsPolicyValue::kWebUi));

  // Navigate to third page on same site.
  const GURL url3("http://google.com/foo");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  NavigationEntry* entry3 = controller().GetLastCommittedEntry();
  SiteInstance* instance3 = contents()->GetSiteInstance();

  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  if (ShouldCreateNewHostForAllFrames()) {
    // If main-frame RenderDocument is enabled, the RFH should change.
    EXPECT_NE(google_rfh, main_test_rfh());
    google_rfh = main_test_rfh();
  } else {
    EXPECT_EQ(google_rfh, main_test_rfh());
  }

  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());
  EXPECT_EQ(url3, entry3->GetURL());
  EXPECT_EQ(instance3,
            NavigationEntryImpl::FromNavigationEntry(entry3)->site_instance());

  // Go back within the site.
  auto back_navigation1 = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation1->Start();
  EXPECT_EQ(entry2, controller().GetPendingEntry());

  // Before that commits, go back again.
  auto back_navigation2 = NavigationSimulatorImpl::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation2->set_drop_unload_ack(true);
  back_navigation2->ReadyToCommit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(contents()->GetSpeculativePrimaryMainFrame());
  EXPECT_EQ(entry1, controller().GetPendingEntry());
  webui_rfh = contents()->GetSpeculativePrimaryMainFrame();

  // DidNavigate from the second back.
  // Note that the process in instance1 is gone at this point, but we will
  // still use instance1 and entry1 because IsSuitableForUrlInfo will return
  // true when there is no process and the site URL matches.
  back_navigation2->Commit();

  // That should have landed us on the first entry.
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());

  // When the second back commits, it should be ignored.
  google_rfh->SendNavigateWithTransition(0, false, url2,
                                         ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(entry1, controller().GetLastCommittedEntry());

  // The newly created process for url1 should be locked to chrome://gpu.
  RenderProcessHost* new_process =
      contents()->GetPrimaryMainFrame()->GetProcess();
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->CanAccessDataForOrigin(new_process->GetID(),
                                             url::Origin::Create(url1)));
  EXPECT_FALSE(policy->CanAccessDataForOrigin(new_process->GetID(),
                                              url::Origin::Create(url2)));
}

// Test that during a slow cross-site navigation, a sub-frame navigation in the
// original renderer will not cancel the slow navigation (bug 42029).
TEST_F(WebContentsImplTest, CrossSiteNavigationNotPreemptedByFrame) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to URL.  First URL should use the original RenderFrameHost.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Start navigating to new site.
  const GURL url2("http://www.yahoo.com");
  orig_rfh->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);
  controller().LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());

  // Simulate a sub-frame navigation arriving and ensure the RVH is still
  // waiting for a before unload response.
  TestRenderFrameHost* child_rfh = orig_rfh->AppendChild("subframe");
  child_rfh->SendNavigateWithTransition(0, false,
                                        GURL("http://google.com/frame"),
                                        ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());

  // Now simulate the onbeforeunload approval and verify the navigation is
  // not canceled.
  orig_rfh->PrepareForCommit();
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
}

// Test that a cross-site navigation is not preempted if the previous
// renderer sends a FrameNavigate message just before being told to stop.
// We should only preempt the cross-site navigation if the previous renderer
// has started a new navigation. See http://crbug.com/79176.
TEST_F(WebContentsImplTest, CrossSiteNotPreemptedDuringBeforeUnload) {
  const GURL kUrl("http://foo");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl);

  // First, make a non-user initiated same-site navigation.
  const GURL kSameSiteUrl("http://foo/1");
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
  // same-site main frame navigations, the same-site navigation below will
  // create a speculative RFH that will be overwritten when the cross-site
  // navigation starts, finishing the same-site navigation, so the scenario in
  // this test cannot be tested. We should disable same-site proactive
  // BrowsingInstance for |orig_rfh| before continuing, and skip this test if
  // RenderDocument is enabled.
  if (ShouldCreateNewHostForAllFrames()) {
    GTEST_SKIP();
  }
  DisableProactiveBrowsingInstanceSwapFor(orig_rfh);
  auto same_site_navigation = NavigationSimulator::CreateRendererInitiated(
      kSameSiteUrl, main_test_rfh());
  same_site_navigation->SetHasUserGesture(false);
  same_site_navigation->ReadyToCommit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());

  // Navigate to a new site, with the beforeunload request in flight.
  const GURL kCrossSiteUrl("http://www.yahoo.com");
  auto cross_site_navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      kCrossSiteUrl, contents());
  cross_site_navigation->set_block_invoking_before_unload_completed_callback(
      true);
  cross_site_navigation->Start();
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_TRUE(orig_rfh->is_waiting_for_beforeunload_completion());
  EXPECT_NE(orig_rfh, pending_rfh);

  // Suppose the first navigation tries to commit now, with a
  // blink::mojom::LocalFrame::StopLoading() in flight. This should not cancel
  // the pending navigation, but it should act as if the beforeunload completion
  // callback had been invoked.
  same_site_navigation->Commit();
  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_FALSE(orig_rfh->is_waiting_for_beforeunload_completion());
  // It should commit.
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kSameSiteUrl, controller().GetLastCommittedEntry()->GetURL());

  // The pending navigation should be able to commit successfully.
  cross_site_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(pending_rfh, main_test_rfh());
  EXPECT_EQ(3, controller().GetEntryCount());
}

// Test that NavigationEntries have the correct page state after going
// forward and back.  Prevents regression for bug 1116137.
TEST_F(WebContentsImplTest, NavigationEntryContentState) {
  // Navigate to URL.  Before the navigation finishes, there should be only the
  // initial NavigationEntry.
  const GURL url("http://www.google.com");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url, contents());
  navigation->ReadyToCommit();
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(!entry || entry->IsInitialEntry());

  // Committed entry should have page state.
  navigation->Commit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Navigate to same site.
  const GURL url2("http://images.google.com");
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->ReadyToCommit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Committed entry should have page state.
  navigation2->Commit();
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // Now go back.  Committed entry should still have page state.
  NavigationSimulator::GoBack(contents());
  entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());
}

// Test that NavigationEntries have the correct page state and SiteInstance
// state after opening a new window to about:blank.  Prevents regression for
// bugs b/1116137 and http://crbug.com/111975.
TEST_F(WebContentsImplTest, NavigationEntryContentStateNewWindow) {
  // Navigate to about:blank.
  const GURL url(url::kAboutBlankURL);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Should have a page state here.
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  EXPECT_TRUE(entry->GetPageState().IsValid());

  // The SiteInstance should be available for other navigations to use.
  NavigationEntryImpl* entry_impl =
      NavigationEntryImpl::FromNavigationEntry(entry);
  EXPECT_FALSE(entry_impl->site_instance()->HasSite());
  auto site_instance_id = entry_impl->site_instance()->GetId();

  // Navigating to a normal page should not cause a process swap.
  const GURL new_url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), new_url);

  NavigationEntryImpl* entry_impl2 = NavigationEntryImpl::FromNavigationEntry(
      controller().GetLastCommittedEntry());
  EXPECT_EQ(site_instance_id, entry_impl2->site_instance()->GetId());
  EXPECT_TRUE(entry_impl2->site_instance()->HasSite());
}

namespace {

void ExpectTrue(bool value) {
  DCHECK(value);
}

void ExpectFalse(bool value) {
  DCHECK(!value);
}

}  // namespace

// Tests that fullscreen is exited throughout the object hierarchy when
// navigating to a new page.
TEST_F(WebContentsImplTest, NavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  orig_rfh->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                            base::BindOnce(&ExpectTrue));
  EXPECT_TRUE(contents()->IsFullscreen());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Navigate to a new site.
  const GURL url2("http://www.yahoo.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

// Tests that fullscreen is exited throughout the object hierarchy when
// instructing NavigationController to GoBack() or GoForward().
TEST_F(WebContentsImplTest, HistoryNavigationExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  EXPECT_EQ(orig_rfh, main_test_rfh());

  // Now, navigate to another page on the same site.
  const GURL url2("http://www.google.com/search?q=kittens");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the same-site navigation above will use a new RFH.
    EXPECT_NE(orig_rfh, main_test_rfh());
  } else {
    EXPECT_EQ(orig_rfh, main_test_rfh());
  }

  // Sanity-check: Confirm we're not starting out in fullscreen mode.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  for (int i = 0; i < 2; ++i) {
    // Toggle fullscreen mode on (as if initiated via IPC from renderer).
    main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
    main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                     base::BindOnce(&ExpectTrue));
    EXPECT_TRUE(contents()->IsFullscreen());
    EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

    // Navigate backward (or forward).
    if (i == 0) {
      NavigationSimulator::GoBack(contents());
    } else {
      NavigationSimulator::GoForward(contents());
    }

    // Confirm fullscreen has exited.
    EXPECT_FALSE(contents()->IsFullscreen());
    EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  }

  contents()->SetDelegate(nullptr);
}

// Tests that fullscreen is exited throughout the object hierarchy on a renderer
// crash.
TEST_F(WebContentsImplTest, CrashExitsFullscreen) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                   base::BindOnce(&ExpectTrue));
  EXPECT_TRUE(contents()->IsFullscreen());
  EXPECT_TRUE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Confirm fullscreen has exited.
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest,
       FailEnterFullscreenWhenNoUserActivationNoOrientationChange) {
  FakeFullscreenDelegate fake_delegate;
  contents()->SetDelegate(&fake_delegate);

  // Navigate to a site.
  const GURL url("http://www.google.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // Toggle fullscreen mode on (as if initiated via IPC from renderer).
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  // When there is no user activation and no orientation change, entering
  // fullscreen will fail.
  main_test_rfh()->EnterFullscreen(blink::mojom::FullscreenOptions::New(),
                                   base::BindOnce(&ExpectFalse));
  EXPECT_TRUE(contents()->IsTransientActivationRequiredForHtmlFullscreen());
  EXPECT_FALSE(
      main_test_rfh()->frame_tree_node()->HasTransientUserActivation());
  EXPECT_FALSE(contents()->IsFullscreen());
  EXPECT_FALSE(fake_delegate.IsFullscreenForTabOrPending(contents()));

  contents()->SetDelegate(nullptr);
}

// Regression test for http://crbug.com/168611 - the URLs passed by the
// DidFinishLoad and DidFailLoadWithError IPCs should get filtered.
TEST_F(WebContentsImplTest, FilterURLs) {
  TestWebContentsObserver observer(contents());

  // A navigation to about:whatever should always look like a navigation to
  // about:blank
  GURL url_normalized(url::kAboutBlankURL);
  GURL url_from_ipc("about:whatever");
  GURL url_blocked(kBlockedURL);

  // We navigate the test WebContents to about:blank, since NavigateAndCommit
  // will use the given URL to create the NavigationEntry as well, and that
  // entry should contain the filtered URL.
  contents()->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  contents()->TestDidFinishLoad(url_from_ipc);

  EXPECT_EQ(url_blocked, observer.last_url());

  // Create and navigate another WebContents.
  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  TestWebContentsObserver other_observer(other_contents.get());
  other_contents->NavigateAndCommit(url_normalized);

  // Check that an IPC with about:whatever is correctly normalized.
  other_contents->GetPrimaryMainFrame()->DidFailLoadWithError(url_from_ipc, 1);
  EXPECT_EQ(url_blocked, other_observer.last_url());
}

// Test that if a pending contents is deleted before it is shown, we don't
// crash.
TEST_F(WebContentsImplTest, PendingContentsDestroyed) {
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents), GURL());
  RenderWidgetHost* widget =
      test_web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // TODO(erikchen): Fix ownership semantics of WebContents. Nothing should be
  // able to delete it beside from the owner. https://crbug.com/832879.
  delete test_web_contents;
  EXPECT_FALSE(contents()->GetCreatedWindow(process_id, widget_id).has_value());
}

TEST_F(WebContentsImplTest, PendingContentsShown) {
  GURL url("http://example.com");
  auto other_contents = base::WrapUnique(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  content::TestWebContents* test_web_contents = other_contents.get();
  contents()->AddPendingContents(std::move(other_contents), url);

  RenderWidgetHost* widget =
      test_web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  int process_id = widget->GetProcess()->GetID();
  int widget_id = widget->GetRoutingID();

  // The first call to GetCreatedWindow pops it off the pending list.
  std::optional<CreatedWindow> created_window =
      contents()->GetCreatedWindow(process_id, widget_id);
  EXPECT_TRUE(created_window.has_value());
  EXPECT_EQ(test_web_contents, created_window->contents.get());
  // Validate target_url.
  EXPECT_EQ(url, created_window->target_url);

  // A second call should return nullopt, verifying that it's been forgotten.
  EXPECT_FALSE(contents()->GetCreatedWindow(process_id, widget_id).has_value());
}

TEST_F(WebContentsImplTest, CaptureHoldsWakeLock) {
  EXPECT_FALSE(contents()->IsBeingCaptured());
  EXPECT_FALSE(contents()->capture_wake_lock_);

  auto expect_wake_lock = [&](bool expect_has_wake_lock) {
    base::RunLoop run_loop;
    contents()->capture_wake_lock_->HasWakeLockForTests(
        base::BindLambdaForTesting([&](bool has_wake_lock) {
          EXPECT_EQ(expect_has_wake_lock, has_wake_lock);
          run_loop.QuitWhenIdle();
        }));
    run_loop.Run();
  };

  // Add capturer which doesn't care to stay awake.
  auto handle1 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/false, /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  ASSERT_FALSE(contents()->capture_wake_lock_);

  // Add capturer and ensure wake lock is held.
  auto handle2 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  ASSERT_TRUE(contents()->capture_wake_lock_);
  expect_wake_lock(true);

  // Add another capturer and ensure the wake lock is still held.
  auto handle3 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  expect_wake_lock(true);

  // Remove one capturer, but one remains so wake lock should still be held.
  handle3.RunAndReset();
  EXPECT_TRUE(contents()->IsBeingCaptured());
  expect_wake_lock(true);

  // Remove the last stay_awake capturer and ensure the wake lock is released.
  handle2.RunAndReset();
  EXPECT_TRUE(contents()->IsBeingCaptured());
  expect_wake_lock(false);

  handle1.RunAndReset();
  EXPECT_FALSE(contents()->IsBeingCaptured());
  expect_wake_lock(false);
}

TEST_F(WebContentsImplTest, CapturerOverridesPreferredSize) {
  const gfx::Size original_preferred_size(1024, 768);
  contents()->UpdateWindowPreferredSize(original_preferred_size);

  // With no capturers, expect the preferred size to be the one propagated into
  // WebContentsImpl via the RenderViewHostDelegate interface.
  EXPECT_FALSE(contents()->IsBeingCaptured());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());

  // Increment capturer count, but without specifying a capture size.  Expect
  // a "not set" preferred size.
  auto handle1 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(gfx::Size(), contents()->GetPreferredSize());

  // Increment capturer count again, but with an overriding capture size.
  // Expect preferred size to now be overridden to the capture size.
  const gfx::Size capture_size(1280, 720);
  auto handle2 = contents()->IncrementCapturerCount(
      capture_size, /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Increment capturer count a third time, but the expect that the preferred
  // size is still the first capture size.
  const gfx::Size another_capture_size(720, 480);
  auto handle3 = contents()->IncrementCapturerCount(another_capture_size,
                                                    /*stay_hidden=*/false,
                                                    /*stay_awake=*/true,
                                                    /*is_activity=*/true);
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count twice, but expect the preferred size to still be
  // overridden.
  handle1.RunAndReset();
  handle2.RunAndReset();
  EXPECT_TRUE(contents()->IsBeingCaptured());
  EXPECT_EQ(capture_size, contents()->GetPreferredSize());

  // Decrement capturer count, and since the count has dropped to zero, the
  // original preferred size should be restored.
  handle3.RunAndReset();
  EXPECT_FALSE(contents()->IsBeingCaptured());
  EXPECT_EQ(original_preferred_size, contents()->GetPreferredSize());
}

TEST_F(WebContentsImplTest, UpdateWebContentsVisibility) {
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  TestWebContentsObserver observer(contents());

  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());

  // WebContents must be made visible once before it can be hidden.
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  // Hiding/occluding/showing the WebContents should hide and show |view|.
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::OCCLUDED);
  EXPECT_TRUE(view->is_showing());
  EXPECT_TRUE(view->is_occluded());
  EXPECT_EQ(Visibility::OCCLUDED, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::OCCLUDED);
  EXPECT_TRUE(view->is_showing());
  EXPECT_TRUE(view->is_occluded());
  EXPECT_EQ(Visibility::OCCLUDED, contents()->GetVisibility());

  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());
}

TEST_F(WebContentsImplTest, VideoPictureInPictureStaysVisibleIfHidden) {
  // Entering video Picture in Picture then hiding keeps the view visible.
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  // Must set the visibility to "visible" before anything interesting happens.
  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  contents()->SetHasPictureInPictureVideo(true);
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_TRUE(view->is_showing());
}

TEST_F(WebContentsImplTest, VisibilityIsUpdatedIfVideoPictureInPictureChanges) {
  // Hiding, then entering video Picture in Picture shows the view.  If we then
  // leave picture-in-picture, the view should become hidden.
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());

  // If the WebContents enters video Picture in Picture while hidden, then it
  // should notify the view that it's visible.
  contents()->SetHasPictureInPictureVideo(true);
  EXPECT_TRUE(view->is_showing());

  // The view should be re-hidden if the WebContents leaves PiP.
  contents()->SetHasPictureInPictureVideo(false);
  EXPECT_FALSE(view->is_showing());
}

TEST_F(WebContentsImplTest, DocumentPictureInPictureStaysVisibleIfHidden) {
  // Entering document Picture in Picture then hiding keeps the view visible.
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  // Must set the visibility to "visible" before anything interesting happens.
  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  contents()->SetHasPictureInPictureDocument(true);
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_TRUE(view->is_showing());
}

TEST_F(WebContentsImplTest,
       VisibilityIsUpdatedIfDocumentPictureInPictureChanges) {
  // Hiding, then entering document Picture in Picture shows the view.  If we
  // then leave picture-in-picture, the view should become hidden.
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      main_test_rfh()->GetRenderViewHost()->GetWidget()->GetView());
  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_FALSE(view->is_showing());

  // If the WebContents enters Picture in Picture while hidden, it should notify
  // the view that it's visible.
  contents()->SetHasPictureInPictureDocument(true);
  EXPECT_TRUE(view->is_showing());

  // The view should be re-hidden if the WebContents leaves PiP.
  contents()->SetHasPictureInPictureDocument(false);
  EXPECT_FALSE(view->is_showing());
}

namespace {

void HideOrOccludeWithCapturerTest(WebContentsImpl* contents,
                                   Visibility hidden_or_occluded) {
  TestRenderWidgetHostView* view = static_cast<TestRenderWidgetHostView*>(
      contents->GetRenderWidgetHostView());

  EXPECT_FALSE(view->is_showing());

  // WebContents must be made visible once before it can be hidden.
  contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents->GetVisibility());

  // Add a capturer when the contents is visible and then hide the contents.
  // |view| should remain visible.
  auto handle1 = contents->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  contents->UpdateWebContentsVisibility(hidden_or_occluded);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(hidden_or_occluded, contents->GetVisibility());

  // Remove the capturer when the contents is hidden/occluded. |view| should be
  // hidden/occluded.
  handle1.RunAndReset();
  if (hidden_or_occluded == Visibility::HIDDEN) {
    EXPECT_FALSE(view->is_showing());
  } else {
    EXPECT_TRUE(view->is_showing());
    EXPECT_TRUE(view->is_occluded());
  }

  // Add a capturer when the contents is hidden. |view| should be unoccluded.
  auto handle2 = contents->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_FALSE(view->is_occluded());

  // Show the contents. The view should be visible.
  contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
  EXPECT_EQ(Visibility::VISIBLE, contents->GetVisibility());

  // Remove the capturer when the contents is visible. The view should remain
  // visible.
  handle2.RunAndReset();
  EXPECT_TRUE(view->is_showing());
  EXPECT_FALSE(view->is_occluded());
}

}  // namespace

TEST_F(WebContentsImplTest, HideWithCapturer) {
  HideOrOccludeWithCapturerTest(contents(), Visibility::HIDDEN);
}

TEST_F(WebContentsImplTest, OccludeWithCapturer) {
  HideOrOccludeWithCapturerTest(contents(), Visibility::OCCLUDED);
}

TEST_F(WebContentsImplTest, HiddenCapture) {
  TestRenderWidgetHostView* rwhv = static_cast<TestRenderWidgetHostView*>(
      contents()->GetRenderWidgetHostView());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, contents()->GetVisibility());

  auto handle1 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(rwhv->is_showing());

  auto handle2 = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false,
      /*stay_awake=*/true, /*is_activity=*/true);
  EXPECT_TRUE(rwhv->is_showing());

  handle1.RunAndReset();
  EXPECT_TRUE(rwhv->is_showing());

  handle2.RunAndReset();
  EXPECT_FALSE(rwhv->is_showing());
}

TEST_F(WebContentsImplTest, NonActivityCaptureDoesNotCountAsActivity) {
  TestRenderWidgetHostView* rwhv = static_cast<TestRenderWidgetHostView*>(
      contents()->GetRenderWidgetHostView());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_EQ(Visibility::VISIBLE, contents()->GetVisibility());

  // Reset the last active time to a known value.
  // This is done because the clock in these tests is frozen,
  // so recording the value and comparing against it later is meaningless.
  contents()->last_active_time_ticks_ = base::TimeTicks();
  contents()->last_active_time_ = base::Time();

  auto handle = contents()->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true,
      /*stay_awake=*/true, /*is_activity=*/false);
  ASSERT_TRUE(rwhv->is_showing());

  // The value returned by GetLastActiveTime() should not have been updated.
  EXPECT_TRUE(contents()->GetLastActiveTimeTicks().is_null());
  EXPECT_TRUE(contents()->GetLastActiveTime().is_null());
}

// Tests that GetLastActiveTimeTicks starts with a real, non-zero time and
// updates on activity.
TEST_F(WebContentsImplTest, GetLastActiveTimeTicks) {
  // The WebContents starts with a valid creation time.
  EXPECT_FALSE(contents()->GetLastActiveTimeTicks().is_null());

  contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_EQ(Visibility::HIDDEN, contents()->GetVisibility());

  // Reset the last active time to a known-bad value.
  // This is done because the clock in these tests is frozen,
  // so recording the value and comparing against it later is meaningless.
  contents()->last_active_time_ticks_ = base::TimeTicks();
  ASSERT_TRUE(contents()->GetLastActiveTimeTicks().is_null());
  contents()->last_active_time_ = base::Time();
  ASSERT_TRUE(contents()->GetLastActiveTime().is_null());

  // Simulate activating the WebContents. The active time should update.
  contents()->WasShown();
  EXPECT_FALSE(contents()->GetLastActiveTimeTicks().is_null());
  EXPECT_FALSE(contents()->GetLastActiveTime().is_null());
}

class ContentsZoomChangedDelegate : public WebContentsDelegate {
 public:
  ContentsZoomChangedDelegate() = default;

  ContentsZoomChangedDelegate(const ContentsZoomChangedDelegate&) = delete;
  ContentsZoomChangedDelegate& operator=(const ContentsZoomChangedDelegate&) =
      delete;

  int GetAndResetContentsZoomChangedCallCount() {
    int count = contents_zoom_changed_call_count_;
    contents_zoom_changed_call_count_ = 0;
    return count;
  }

  bool last_zoom_in() const { return last_zoom_in_; }

  // WebContentsDelegate:
  void ContentsZoomChange(bool zoom_in) override {
    contents_zoom_changed_call_count_++;
    last_zoom_in_ = zoom_in;
  }

 private:
  int contents_zoom_changed_call_count_ = 0;
  bool last_zoom_in_ = false;
};

// Tests that some mouseehweel events get turned into browser zoom requests.
TEST_F(WebContentsImplTest, HandleWheelEvent) {
  using blink::WebInputEvent;

  std::unique_ptr<ContentsZoomChangedDelegate> delegate(
      new ContentsZoomChangedDelegate());
  contents()->SetDelegate(delegate.get());

  int modifiers = 0;
  // Verify that normal mouse wheel events do nothing to change the zoom level.
  blink::WebMouseWheelEvent event =
      blink::SyntheticWebMouseWheelEventBuilder::Build(
          0, 0, 0, 1, modifiers, ui::ScrollGranularity::kScrollByPixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // But whenever the ctrl modifier is applied zoom can be increased or
  // decreased. Except on MacOS where we never want to adjust zoom
  // with mousewheel.
  modifiers = WebInputEvent::kControlKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 0, 1, modifiers, ui::ScrollGranularity::kScrollByPixel);
  bool handled = contents()->HandleWheelEvent(event);
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_TRUE(delegate->last_zoom_in());
#else
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#endif

  modifiers = WebInputEvent::kControlKey | WebInputEvent::kShiftKey |
              WebInputEvent::kAltKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 2, -5, modifiers, ui::ScrollGranularity::kScrollByPixel);
  handled = contents()->HandleWheelEvent(event);
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(handled);
  EXPECT_EQ(1, delegate->GetAndResetContentsZoomChangedCallCount());
  EXPECT_FALSE(delegate->last_zoom_in());
#else
  EXPECT_FALSE(handled);
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());
#endif

  // Unless there is no vertical movement.
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 2, 0, modifiers, ui::ScrollGranularity::kScrollByPixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Events containing precise scrolling deltas also shouldn't result in the
  // zoom being adjusted, to avoid accidental adjustments caused by
  // two-finger-scrolling on a touchpad.
  modifiers = WebInputEvent::kControlKey;
  event = blink::SyntheticWebMouseWheelEventBuilder::Build(
      0, 0, 0, 5, modifiers, ui::ScrollGranularity::kScrollByPrecisePixel);
  EXPECT_FALSE(contents()->HandleWheelEvent(event));
  EXPECT_EQ(0, delegate->GetAndResetContentsZoomChangedCallCount());

  // Ensure pointers to the delegate aren't kept beyond its lifetime.
  contents()->SetDelegate(nullptr);
}

// Tests that GetRelatedActiveContentsCount is shared between related
// SiteInstances and includes WebContents that have not navigated yet.
TEST_F(WebContentsImplTest, ActiveContentsCountBasic) {
  scoped_refptr<SiteInstance> instance1(
      SiteInstance::CreateForURL(browser_context(), GURL("http://a.com")));
  scoped_refptr<SiteInstance> instance2(
      instance1->GetRelatedSiteInstance(GURL("http://b.com")));

  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents1(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents2(
      TestWebContents::Create(browser_context(), instance1.get()));
  EXPECT_EQ(2u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, instance2->GetRelatedActiveContentsCount());

  contents1.reset();
  EXPECT_EQ(1u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance2->GetRelatedActiveContentsCount());

  contents2.reset();
  EXPECT_EQ(0u, instance1->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance2->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount is preserved correctly across
// same-site and cross-site navigations.
TEST_F(WebContentsImplTest, ActiveContentsCountNavigate) {
  scoped_refptr<SiteInstance> instance(SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  auto navigation1 = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://a.com/1"), contents.get());
  navigation1->Start();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  navigation1->Commit();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in the same site.
  auto navigation2 = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://a.com/2"), contents.get());
  navigation2->Start();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  navigation2->Commit();
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance turned on for same-site navigations,
    // the BrowsingInstance will change on same-site navigations.
    EXPECT_NE(instance, contents->GetSiteInstance());
    // Check the previous instance's count.
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // Update the current instance.
    instance = contents->GetSiteInstance();
    EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  }

  // Navigate to a URL in a different site in the same BrowsingInstance.
  const GURL kUrl2("http://b.com");
  auto navigation3 = NavigationSimulator::CreateRendererInitiated(
      kUrl2, contents->GetPrimaryMainFrame());
  navigation3->ReadyToCommit();
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_TRUE(contents->CrossProcessNavigationPending());
  } else {
    EXPECT_FALSE(contents->CrossProcessNavigationPending());
  }
  navigation3->Commit();
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    // When ProactivelySwapBrowsingInstance turned on, the BrowsingInstance will
    // change on cross-site navigations.
    EXPECT_NE(instance, contents->GetSiteInstance());
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // Update the current instance.
    instance = contents->GetSiteInstance();
  }
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL in a different site and different BrowsingInstance, by
  // using a TYPED page transition instead of LINK.
  const GURL kUrl3("http://c.com");
  auto navigation4 =
      NavigationSimulator::CreateBrowserInitiated(kUrl3, contents.get());
  navigation4->SetTransition(ui::PAGE_TRANSITION_TYPED);
  navigation4->ReadyToCommit();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  scoped_refptr<SiteInstance> new_instance =
      contents->GetSpeculativePrimaryMainFrame()->GetSiteInstance();
  navigation4->Commit();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, new_instance->GetRelatedActiveContentsCount());
  EXPECT_FALSE(new_instance->IsRelatedSiteInstance(instance.get()));

  contents.reset();
  EXPECT_EQ(0u, new_instance->GetRelatedActiveContentsCount());
}

// Tests that GetRelatedActiveContentsCount tracks BrowsingInstance changes
// from WebUI.
TEST_F(WebContentsImplTest, ActiveContentsCountChangeBrowsingInstance) {
  scoped_refptr<SiteInstance> instance(SiteInstance::Create(browser_context()));

  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());

  std::unique_ptr<TestWebContents> contents(
      TestWebContents::Create(browser_context(), instance.get()));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL.
  contents->NavigateAndCommit(GURL("http://a.com"));
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL which sort of looks like a chrome:// url.
  contents->NavigateAndCommit(GURL("http://gpu"));
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    // The navigation from "a.com" to "gpu" is using a new BrowsingInstance.
    EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
    // The rest of the test expects |instance| to match the one in the main
    // frame.
    instance = contents->GetPrimaryMainFrame()->GetSiteInstance();
  }
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());

  // Navigate to a URL with WebUI. This will change BrowsingInstances.
  const GURL kWebUIUrl = GURL(GetWebUIURL(kChromeUIGpuHost));
  auto web_ui_navigation =
      NavigationSimulator::CreateBrowserInitiated(kWebUIUrl, contents.get());
  web_ui_navigation->Start();
  EXPECT_TRUE(contents->CrossProcessNavigationPending());
  scoped_refptr<SiteInstance> instance_webui(
      contents->GetSpeculativePrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(instance->IsRelatedSiteInstance(instance_webui.get()));

  // At this point, contents still counts for the old BrowsingInstance.
  EXPECT_EQ(1u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());

  // Commit and contents counts for the new one.
  web_ui_navigation->Commit();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, instance_webui->GetRelatedActiveContentsCount());

  contents.reset();
  EXPECT_EQ(0u, instance->GetRelatedActiveContentsCount());
  EXPECT_EQ(0u, instance_webui->GetRelatedActiveContentsCount());
}

class LoadingWebContentsObserver : public WebContentsObserver {
 public:
  explicit LoadingWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents), is_loading_(false) {}

  LoadingWebContentsObserver(const LoadingWebContentsObserver&) = delete;
  LoadingWebContentsObserver& operator=(const LoadingWebContentsObserver&) =
      delete;

  ~LoadingWebContentsObserver() override = default;

  // The assertions on these messages ensure that they are received in order.
  void DidStartLoading() override {
    ASSERT_FALSE(is_loading_);
    is_loading_ = true;
  }
  void DidStopLoading() override {
    ASSERT_TRUE(is_loading_);
    is_loading_ = false;
  }

  bool is_loading() const { return is_loading_; }

 private:
  bool is_loading_;
};

// Subclass of WebContentsImplTest for cases that need out-of-process iframes.
class WebContentsImplTestWithSiteIsolation : public WebContentsImplTest {
 public:
  WebContentsImplTestWithSiteIsolation() {
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }
};

// Ensure that DidStartLoading/DidStopLoading events balance out properly with
// interleaving cross-process navigations in multiple subframes.
// See https://crbug.com/448601 for details of the underlying issue. The
// sequence of events that reproduce it are as follows:
// * Navigate top-level frame with one subframe.
// * Subframe navigates more than once before the top-level frame has had a
//   chance to complete the load.
// The subframe navigations cause the loading_frames_in_progress_ to drop down
// to 0, while the loading_progresses_ map is not reset.
TEST_F(WebContentsImplTestWithSiteIsolation, StartStopEventsBalance) {
  // The bug manifests itself in regular mode as well, but browser-initiated
  // navigation of subframes is only possible in --site-per-process mode within
  // unit tests.
  const GURL initial_url("about:blank");
  const GURL main_url("http://www.chromium.org");
  const GURL foo_url("http://foo.chromium.org");
  const GURL bar_url("http://bar.chromium.org");
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Use a WebContentsObserver to observe the behavior of the tab's spinner.
  LoadingWebContentsObserver observer(contents());

  // Navigate the main RenderFrame and commit. The frame should still be
  // loading.
  auto main_frame_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  main_frame_navigation->SetKeepLoading(true);
  main_frame_navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_TRUE(contents()->IsLoading());

  // The Observer callback implementations contain assertions to ensure that the
  // events arrive in the correct order.
  EXPECT_TRUE(observer.is_loading());

  // Create a child frame to navigate multiple times.
  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");

  // Navigate the child frame to about:blank, which will send DidStopLoading
  // message.
  NavigationSimulator::NavigateAndCommitFromDocument(initial_url, subframe);

  // Navigate the frame to another URL, which will send again
  // DidStartLoading and DidStopLoading messages.
  NavigationSimulator::NavigateAndCommitFromDocument(foo_url, subframe);

  // Since the main frame hasn't sent any DidStopLoading messages, it is
  // expected that the WebContents is still in loading state.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());

  // After navigation, the RenderFrameHost may change.
  subframe = static_cast<TestRenderFrameHost*>(contents()
                                                   ->GetPrimaryFrameTree()
                                                   .root()
                                                   ->child_at(0)
                                                   ->current_frame_host());
  // Navigate the frame again, this time using LoadURLWithParams. This causes
  // RenderFrameHost to call into WebContents::DidStartLoading, which starts
  // the spinner.
  {
    auto navigation =
        NavigationSimulatorImpl::CreateBrowserInitiated(bar_url, contents());

    NavigationController::LoadURLParams load_params(bar_url);
    load_params.referrer = Referrer(GURL("http://referrer"),
                                    network::mojom::ReferrerPolicy::kDefault);
    load_params.transition_type = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    load_params.extra_headers = "content-type: text/plain";
    load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
    load_params.is_renderer_initiated = false;
    load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
    load_params.frame_tree_node_id =
        subframe->frame_tree_node()->frame_tree_node_id();
    navigation->SetLoadURLParams(&load_params);

    navigation->Commit();
    subframe = static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());
  }

  // At this point the status should still be loading, since the main frame
  // hasn't sent the DidstopLoading message yet.
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(observer.is_loading());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  main_frame_navigation->StopLoading();
  EXPECT_FALSE(contents()->IsLoading());
  EXPECT_FALSE(observer.is_loading());
}

// Tests that WebContentsImpl::ShouldShowLoadingUI only reports main
// frame loads. Browser-initiated navigation of subframes is only possible in
// --site-per-process mode within unit tests.
TEST_F(WebContentsImplTestWithSiteIsolation, ShouldShowLoadingUI) {
  const GURL main_url("http://www.chromium.org");
  TestRenderFrameHost* orig_rfh = main_test_rfh();

  // Navigate the main RenderFrame and commit. The frame should still be
  // loading.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  navigation->SetKeepLoading(true);
  navigation->Commit();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(orig_rfh, main_test_rfh());
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_TRUE(contents()->ShouldShowLoadingUI());

  // Send the DidStopLoading for the main frame and ensure it isn't loading
  // anymore.
  navigation->StopLoading();
  EXPECT_FALSE(contents()->IsLoading());
  EXPECT_FALSE(contents()->ShouldShowLoadingUI());

  // Create a child frame to navigate.
  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");

  // Navigate the child frame to about:blank, make sure the web contents is
  // marked as "loading" but not "showing loading UI".
  subframe->SendNavigateWithTransition(0, false, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_FALSE(contents()->ShouldShowLoadingUI());
  static_cast<mojom::FrameHost*>(subframe)->DidStopLoading();
  EXPECT_FALSE(contents()->IsLoading());
}

// Ensure that WebContentsImpl does not stop loading too early when there still
// is a pending renderer. This can happen if a same-process non user-initiated
// navigation completes while there is an ongoing cross-process navigation.
// TODO(clamy): Rewrite that test when the renderer-initiated non-user-initiated
// navigation no longer kills the speculative RenderFrameHost. See
// https://crbug.com/889039.
TEST_F(WebContentsImplTest, DISABLED_NoEarlyStop) {
  const GURL kUrl1("http://www.chromium.org");
  const GURL kUrl2("http://www.google.com");
  const GURL kUrl3("http://www.chromium.org/foo");

  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* current_rfh = main_test_rfh();

  // Start a browser-initiated cross-process navigation to |kUrl2|. The
  // WebContents should be loading.
  auto cross_process_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  cross_process_navigation->ReadyToCommit();
  TestRenderFrameHost* pending_rfh =
      contents()->GetSpeculativePrimaryMainFrame();
  EXPECT_TRUE(contents()->IsLoading());

  // The current RenderFrameHost starts a non user-initiated render-initiated
  // navigation. The WebContents should still be loading.
  auto same_process_navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl3, current_rfh);
  same_process_navigation->SetHasUserGesture(false);
  same_process_navigation->Start();
  EXPECT_TRUE(contents()->IsLoading());

  // Simulate the commit and DidStopLoading from the renderer-initiated
  // navigation in the current RenderFrameHost. There should still be a pending
  // RenderFrameHost and the WebContents should still be loading.
  same_process_navigation->Commit();
  static_cast<mojom::FrameHost*>(current_rfh)->DidStopLoading();
  EXPECT_EQ(contents()->GetSpeculativePrimaryMainFrame(), pending_rfh);
  EXPECT_TRUE(contents()->IsLoading());

  // The same-process navigation should have committed.
  ASSERT_EQ(2, controller().GetEntryCount());
  EXPECT_EQ(kUrl3, controller().GetLastCommittedEntry()->GetURL());

  // Commit the cross-process navigation. The formerly pending RenderFrameHost
  // should now be the current RenderFrameHost and the WebContents should still
  // be loading.
  cross_process_navigation->Commit();
  EXPECT_FALSE(contents()->GetSpeculativePrimaryMainFrame());
  TestRenderFrameHost* new_current_rfh = main_test_rfh();
  EXPECT_EQ(new_current_rfh, pending_rfh);
  EXPECT_TRUE(contents()->IsLoading());
  EXPECT_EQ(3, controller().GetEntryCount());

  // Simulate the new current RenderFrameHost DidStopLoading. The WebContents
  // should now have stopped loading.
  static_cast<mojom::FrameHost*>(new_current_rfh)->DidStopLoading();
  EXPECT_EQ(main_test_rfh(), new_current_rfh);
  EXPECT_FALSE(contents()->IsLoading());
}

TEST_F(WebContentsImplTest, MediaWakeLock) {
  EXPECT_FALSE(has_audio_wake_lock());

  AudioStreamMonitor* monitor = contents()->audio_stream_monitor();

  // Ensure RenderFrame is initialized before simulating events coming from it.
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Send a fake audio stream monitor notification.  The audio wake lock
  // should be created.
  monitor->set_was_recently_audible_for_testing(true);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
  EXPECT_TRUE(has_audio_wake_lock());

  // Send another fake notification, this time when WasRecentlyAudible() will
  // be false.  The wake lock should be released.
  monitor->set_was_recently_audible_for_testing(false);
  contents()->NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
  EXPECT_FALSE(has_audio_wake_lock());

  main_test_rfh()->GetProcess()->SimulateCrash();

  // Verify that all the wake locks have been released.
  EXPECT_FALSE(has_audio_wake_lock());
}

// Test that the WebContentsObserver is notified when text is copied to the
// clipboard for a given RenderFrameHost.
TEST_F(WebContentsImplTest, OnTextCopiedToClipboard) {
  TestWebContentsObserver observer(contents());
  TestRenderFrameHost* rfh = main_test_rfh();
  const std::u16string copied_text = u"copied_text";

  rfh->OnTextCopiedToClipboard(copied_text);

  EXPECT_EQ(copied_text, observer.text_copied_to_clipboard());
}

TEST_F(WebContentsImplTest, ThemeColorChangeDependingOnFirstVisiblePaint) {
  TestWebContentsObserver observer(contents());
  TestRenderFrameHost* rfh = main_test_rfh();
  rfh->InitializeRenderFrameIfNeeded();

  EXPECT_EQ(std::nullopt, contents()->GetThemeColor());
  EXPECT_EQ(0, observer.theme_color_change_calls());

  // Theme color changes should not propagate past the WebContentsImpl before
  // the first visually non-empty paint has occurred.
  rfh->DidChangeThemeColor(SK_ColorRED);

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(0, observer.theme_color_change_calls());

  // Simulate that the first visually non-empty paint has occurred. This will
  // propagate the current theme color to the delegates.
  rfh->GetPage().OnFirstVisuallyNonEmptyPaint();

  EXPECT_EQ(SK_ColorRED, contents()->GetThemeColor());
  EXPECT_EQ(1, observer.theme_color_change_calls());

  // Additional changes made by the web contents should propagate as well.
  rfh->DidChangeThemeColor(SK_ColorGREEN);

  EXPECT_EQ(SK_ColorGREEN, contents()->GetThemeColor());
  EXPECT_EQ(2, observer.theme_color_change_calls());
}

TEST_F(WebContentsImplTest, ParseDownloadHeaders) {
  download::DownloadUrlParameters::RequestHeadersType request_headers =
      WebContentsImpl::ParseDownloadHeaders("A: 1\r\nB: 2\r\nC: 3\r\n\r\n");
  ASSERT_EQ(3u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);
  EXPECT_EQ("B", request_headers[1].first);
  EXPECT_EQ("2", request_headers[1].second);
  EXPECT_EQ("C", request_headers[2].first);
  EXPECT_EQ("3", request_headers[2].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A:1\r\nA:2\r\n");
  ASSERT_EQ(2u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);
  EXPECT_EQ("A", request_headers[1].first);
  EXPECT_EQ("2", request_headers[1].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A 1\r\nA: 2");
  ASSERT_EQ(1u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("2", request_headers[0].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A: 1");
  ASSERT_EQ(1u, request_headers.size());
  EXPECT_EQ("A", request_headers[0].first);
  EXPECT_EQ("1", request_headers[0].second);

  request_headers = WebContentsImpl::ParseDownloadHeaders("A 1");
  ASSERT_EQ(0u, request_headers.size());
}

// CHECKs should occur when `WebContentsImpl::DidLoadResourceFromMemoryCache()`
// is called with RequestDestinations that can only correspond to navigations.
TEST_F(WebContentsImplTest, DidLoadResourceFromMemoryCache_NavigationCheck) {
  const GURL kImgUrl("https://www.example.com/image.png");

  EXPECT_CHECK_DEATH(contents()->DidLoadResourceFromMemoryCache(
      main_test_rfh(), kImgUrl, "GET", "image/png",
      network::mojom::RequestDestination::kDocument,
      /*include_credentials=*/false));

  EXPECT_CHECK_DEATH(contents()->DidLoadResourceFromMemoryCache(
      main_test_rfh(), kImgUrl, "GET", "image/png",
      network::mojom::RequestDestination::kIframe,
      /*include_credentials=*/false));
}

// Regression test for crbug.com/347934841#comment3 to ensure that if
// `WebContentsImpl::DidLoadResourceFromMemoryCache()` is called with a
// `request_destination` parameter value of
// `network::mojom::RequestDestination::kObject` or
// `network::mojom::RequestDestination::kEmbed`, a CHECK does not occur since
// those can correspond to both navigations and resource loads.
TEST_F(WebContentsImplTest,
       DidLoadResourceFromMemoryCache_BypassNavigationCheck) {
  mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
  MockNetworkContext mock_network_context(
      network_context_remote.InitWithNewPipeAndPassReceiver());
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      GetBrowserContext()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      std::move(network_context_remote));

  const GURL kImgUrl("https://www.example.com/image.png");
  const std::string kGet = "GET";

  for (const auto destination : {network::mojom::RequestDestination::kObject,
                                 network::mojom::RequestDestination::kEmbed}) {
    SCOPED_TRACE(static_cast<int>(destination));

    base::test::TestFuture<void> signal;

    // If the NotifyExternalCacheHit call is reached then we know the CHECKs
    // were evaluated but didn't trigger.
    EXPECT_CALL(
        mock_network_context,
        NotifyExternalCacheHit(kImgUrl, kGet, ::testing::_, ::testing::_))
        .WillOnce(base::test::InvokeFuture(signal));

    contents()->DidLoadResourceFromMemoryCache(main_test_rfh(), kImgUrl, kGet,
                                               "image/png", destination,
                                               /*include_credentials=*/false);
    EXPECT_TRUE(signal.Wait());
  }
}

namespace {

class TestJavaScriptDialogManager : public JavaScriptDialogManager {
 public:
  TestJavaScriptDialogManager() = default;

  TestJavaScriptDialogManager(const TestJavaScriptDialogManager&) = delete;
  TestJavaScriptDialogManager& operator=(const TestJavaScriptDialogManager&) =
      delete;

  ~TestJavaScriptDialogManager() override = default;

  size_t reset_count() { return reset_count_; }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    *did_suppress_message = true;
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {}

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {
    if (reset_state) {
      ++reset_count_;
    }
  }

 private:
  size_t reset_count_ = 0;
};

}  // namespace

TEST_F(WebContentsImplTest, ResetJavaScriptDialogOnUserNavigate) {
  const GURL kUrl("http://www.google.com");
  const GURL kUrl2("http://www.google.com/sub");
  TestJavaScriptDialogManager dialog_manager;
  contents()->SetJavaScriptDialogManagerForTesting(&dialog_manager);

  // A user-initiated navigation.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl);
  EXPECT_EQ(1u, dialog_manager.reset_count());

  // An automatic navigation.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If we changed RenderFrameHost on a renderer-initiated navigation above,
    // we would trigger RenderFrameHostManager::UnloadOldFrame, similar to the
    // first (user/browser-initiated) navigation, which will trigger dialog
    // cancellations and increment the reset_count to 2.
    EXPECT_EQ(2u, dialog_manager.reset_count());
  } else {
    EXPECT_EQ(1u, dialog_manager.reset_count());
  }

  contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

TEST_F(WebContentsImplTest, StartingSandboxFlags) {
  WebContents::CreateParams params(browser_context());
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kPopups |
      network::mojom::WebSandboxFlags::kModals |
      network::mojom::WebSandboxFlags::kTopNavigation;
  params.starting_sandbox_flags = expected_flags;
  std::unique_ptr<WebContentsImpl> new_contents(
      WebContentsImpl::CreateWithOpener(params, nullptr));
  FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
  network::mojom::WebSandboxFlags pending_flags =
      root->pending_frame_policy().sandbox_flags;
  EXPECT_EQ(pending_flags, expected_flags);
  network::mojom::WebSandboxFlags effective_flags =
      root->effective_frame_policy().sandbox_flags;
  EXPECT_EQ(effective_flags, expected_flags);
}

TEST_F(WebContentsImplTest, DidFirstVisuallyNonEmptyPaint) {
  TestWebContentsObserver observer(contents());

  contents()->GetPrimaryPage().OnFirstVisuallyNonEmptyPaint();

  EXPECT_TRUE(observer.observed_did_first_visually_non_empty_paint());
}

TEST_F(WebContentsImplTest, DidChangeVerticalScrollDirection) {
  TestWebContentsObserver observer(contents());

  EXPECT_FALSE(observer.last_vertical_scroll_direction().has_value());

  contents()->OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection::kUp);

  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            observer.last_vertical_scroll_direction().value());
}

TEST_F(WebContentsImplTest, HandleContextMenuDelegate) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestRenderFrameHost& main_rfh = *main_test_rfh();
  EXPECT_CALL(delegate, HandleContextMenu(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(true));

  ContextMenuParams params;
  contents()->ShowContextMenu(main_rfh, mojo::NullAssociatedRemote(), params);

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, RegisterProtocolHandlerDifferentOrigin) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL url("https://www.google.com");
  GURL handler_url1("https://www.google.com/handler/%s");
  GURL handler_url2("https://www.example.com/handler/%s");

  contents()->NavigateAndCommit(url);

  // Only the first call to RegisterProtocolHandler should register because the
  // other call has a handler from a different origin.
  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url1, true))
      .Times(1);
  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url2, true))
      .Times(0);

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url1,
                                        /*user_gesture=*/true);
  }

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url2,
                                        /*user_gesture=*/true);
  }

  // Check behavior for RegisterProtocolHandler::kUntrustedOrigins.
  MockWebContentsDelegate unrestrictive_delegate(
      blink::ProtocolHandlerSecurityLevel::kUntrustedOrigins);
  contents()->SetDelegate(&unrestrictive_delegate);
  EXPECT_CALL(
      unrestrictive_delegate,
      RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url1, true))
      .Times(1);
  EXPECT_CALL(
      unrestrictive_delegate,
      RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url2, true))
      .Times(1);

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url1,
                                        /*user_gesture=*/true);
  }

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url2,
                                        /*user_gesture=*/true);
  }

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, RegisterProtocolHandlerDataURL) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL data("data:text/html,<html><body><b>hello world</b></body></html>");
  GURL data_handler(data.spec() + "%s");

  contents()->NavigateAndCommit(data);

  // Data URLs should fail.
  EXPECT_CALL(delegate,
              RegisterProtocolHandler(contents()->GetPrimaryMainFrame(),
                                      "mailto", data_handler, true))
      .Times(0);

  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", data_handler,
                                        /*user_gesture=*/true);
  }

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, RegisterProtocolHandlerInvalidURLSyntax) {
  MockWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  GURL url("https://www.google.com");
  GURL handler_url1("https://www.google.com/handler/%s");
  GURL handler_url2("https://www.google.com/handler/");

  contents()->NavigateAndCommit(url);

  // Only the first call to RegisterProtocolHandler should register because the
  // other call has a handler from a different origin.
  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url1, true))
      .Times(1);

  EXPECT_CALL(delegate, RegisterProtocolHandler(main_test_rfh(), "mailto",
                                                handler_url2, true))
      .Times(0);
  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url1,
                                        /*user_gesture=*/true);
  }
  {
    contents()->RegisterProtocolHandler(main_test_rfh(), "mailto", handler_url2,
                                        /*user_gesture=*/true);
  }

  contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsImplTest, Usb) {
  testing::StrictMock<TestWebContentsObserver> observer(contents());
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kUSB, true))
      .WillOnce(testing::Invoke([&]() {
        // Accessor must return the updated state when the observer is notified.
        EXPECT_TRUE(contents()->IsConnectedToUsbDevice());
      }));
  contents()->TestIncrementUsbActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());

  // Additional increment/decrement don't modify state.
  contents()->TestIncrementUsbActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());
  contents()->TestDecrementUsbActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToUsbDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kUSB, false))
      .WillOnce(testing::Invoke(
          [&]() { EXPECT_FALSE(contents()->IsConnectedToUsbDevice()); }));
  contents()->TestDecrementUsbActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(contents()->IsConnectedToUsbDevice());
}

TEST_F(WebContentsImplTest, Hid) {
  testing::StrictMock<TestWebContentsObserver> observer(contents());
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kHID, true))
      .WillOnce(testing::Invoke([&]() {
        // Accessor must return the updated state when the observer is notified.
        EXPECT_TRUE(contents()->IsConnectedToHidDevice());
      }));
  contents()->TestIncrementHidActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(contents()->IsConnectedToHidDevice());

  // Additional increment/decrement don't modify state.
  contents()->TestIncrementHidActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToHidDevice());
  contents()->TestDecrementHidActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToHidDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kHID, false))
      .WillOnce(testing::Invoke(
          [&]() { EXPECT_FALSE(contents()->IsConnectedToHidDevice()); }));
  contents()->TestDecrementHidActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(contents()->IsConnectedToHidDevice());
}

TEST_F(WebContentsImplTest, Serial) {
  testing::StrictMock<TestWebContentsObserver> observer(contents());
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kSerial, true))
      .WillOnce(testing::Invoke([&]() {
        // Accessor must return the updated state when the observer is notified.
        EXPECT_TRUE(contents()->IsConnectedToSerialPort());
      }));
  contents()->TestIncrementSerialActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  // Additional increment/decrement don't modify state.
  contents()->TestIncrementSerialActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());
  contents()->TestDecrementSerialActiveFrameCount();
  EXPECT_TRUE(contents()->IsConnectedToSerialPort());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kSerial, false))
      .WillOnce(testing::Invoke(
          [&]() { EXPECT_FALSE(contents()->IsConnectedToSerialPort()); }));
  contents()->TestDecrementSerialActiveFrameCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(contents()->IsConnectedToSerialPort());
}

TEST_F(WebContentsImplTest, Bluetooth) {
  testing::StrictMock<TestWebContentsObserver> observer(contents());
  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kBluetooth, true))
      .WillOnce(testing::Invoke([&]() {
        // Accessor must return the updated state when the observer is notified.
        EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
      }));
  contents()->TestIncrementBluetoothConnectedDeviceCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  // Additional increment/decrement don't modify state.
  contents()->TestIncrementBluetoothConnectedDeviceCount();
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());
  contents()->TestDecrementBluetoothConnectedDeviceCount();
  EXPECT_TRUE(contents()->IsConnectedToBluetoothDevice());

  EXPECT_CALL(observer,
              OnDeviceConnectionTypesChanged(
                  WebContentsObserver::DeviceConnectionType::kBluetooth, false))
      .WillOnce(testing::Invoke(
          [&]() { EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice()); }));
  contents()->TestDecrementBluetoothConnectedDeviceCount();
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(contents()->IsConnectedToBluetoothDevice());
}

TEST_F(WebContentsImplTest, BadDownloadImageResponseFromRenderer) {
  // Avoid using TestWebContents, which fakes image download logic without
  // exercising the code in WebContentsImpl.
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(GetBrowserContext());
  instance->GetProcess()->Init();
  WebContents::CreateParams create_params(GetBrowserContext(),
                                          std::move(instance));
  create_params.desired_renderer_state = WebContents::CreateParams::
      CreateParams::kInitializeAndWarmupRendererProcess;
  std::unique_ptr<WebContentsImpl> contents(
      WebContentsImpl::CreateWithOpener(create_params, /*opener_rfh=*/nullptr));
  ASSERT_FALSE(
      contents->GetPrimaryMainFrame()->GetProcess()->ShutdownRequested());

  // Set up the fake image downloader.
  FakeImageDownloader fake_image_downloader;
  fake_image_downloader.Init(
      contents->GetPrimaryMainFrame()->GetRemoteInterfaces());

  // For the purpose of this test, set up a malformed response with different
  // vector sizes.
  const GURL kImageUrl = GURL("https://example.com/favicon.ico");
  fake_image_downloader.SetFakeResponseData(
      kImageUrl,
      /*bitmaps=*/{}, /*original_bitmap_sizes=*/{gfx::Size(16, 16)});

  base::RunLoop run_loop;
  contents->DownloadImage(
      kImageUrl,
      /*is_favicon=*/true,
      /*preferred_size=*/gfx::Size(16, 16),
      /*max_bitmap_size=*/32,
      /*bypass_cache=*/false,
      base::BindLambdaForTesting([&](int id, int http_status_code,
                                     const GURL& image_url,
                                     const std::vector<SkBitmap>& bitmaps,
                                     const std::vector<gfx::Size>& sizes) {
        EXPECT_EQ(400, http_status_code);
        EXPECT_TRUE(bitmaps.empty());
        EXPECT_TRUE(sizes.empty());
        run_loop.Quit();
      }));
  run_loop.Run();

  // The renderer process should have been killed due to
  // WCI_INVALID_DOWNLOAD_IMAGE_RESULT.
  EXPECT_TRUE(
      contents->GetPrimaryMainFrame()->GetProcess()->ShutdownRequested());
}

TEST_F(WebContentsImplTest,
       GetCaptureHandleConfigBeforeSetIsCalledReturnsEmptyConfig) {
  const auto empty_config = blink::mojom::CaptureHandleConfig::New();
  EXPECT_EQ(contents()->GetCaptureHandleConfig(), *empty_config);
}

TEST_F(WebContentsImplTest, SetAndGetCaptureHandleConfig) {
  // Value set - value returned.
  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"Pay not attention";
    contents()->SetCaptureHandleConfig(config->Clone());
    EXPECT_EQ(*config, contents()->GetCaptureHandleConfig());
  }

  // New value set - new value returned.
  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"to the man behind the curtain.";
    contents()->SetCaptureHandleConfig(config->Clone());
    EXPECT_EQ(*config, contents()->GetCaptureHandleConfig());
  }
}

TEST_F(WebContentsImplTest, NoOnCaptureHandleConfigUpdateCallIfResettingEmpty) {
  const auto empty_config = blink::mojom::CaptureHandleConfig::New();

  // Reminder - empty in the beginning.
  ASSERT_EQ(contents()->GetCaptureHandleConfig(),
            *blink::mojom::CaptureHandleConfig::New());

  TestWebContentsObserver observer(contents());
  // Note that ExpectOnCaptureHandleConfigUpdate() is NOT called.
  // If OnCaptureHandleConfigUpdate() is called, the test will fail.

  contents()->SetCaptureHandleConfig(empty_config.Clone());
}

TEST_F(WebContentsImplTest,
       OnCaptureHandleConfigUpdateCalledWhenHandleChanges) {
  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"Some handle.";
    contents()->SetCaptureHandleConfig(config.Clone());
  }

  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"A different handle.";
    TestWebContentsObserver observer(contents());
    observer.ExpectOnCaptureHandleConfigUpdate(config.Clone());
    contents()->SetCaptureHandleConfig(config.Clone());
  }
}

TEST_F(WebContentsImplTest,
       OnCaptureHandleConfigUpdateNotCalledWhenResettingAnIdenticalHandle) {
  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"The ministry of redundancy ministry.";
    contents()->SetCaptureHandleConfig(config.Clone());
  }

  {
    auto config = blink::mojom::CaptureHandleConfig::New();
    config->capture_handle = u"The ministry of redundancy ministry.";
    TestWebContentsObserver observer(contents());
    // Note that ExpectOnCaptureHandleConfigUpdate() is NOT called.
    // If OnCaptureHandleConfigUpdate() is called, the test will fail.
    contents()->SetCaptureHandleConfig(config.Clone());
  }
}

TEST_F(WebContentsImplTest,
       OnCaptureHandleConfigUpdateCalledWhenClearingTheConfig) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"Some handle.";
  contents()->SetCaptureHandleConfig(config.Clone());

  auto empty_config = blink::mojom::CaptureHandleConfig::New();
  TestWebContentsObserver observer(contents());
  observer.ExpectOnCaptureHandleConfigUpdate(empty_config.Clone());
  contents()->SetCaptureHandleConfig(empty_config.Clone());
}

TEST_F(WebContentsImplTest,
       CrossDocumentMainPageNavigationClearsCaptureHandleConfig) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  ASSERT_EQ(orig_rfh, orig_rfh->GetMainFrame());

  // Navigate to the first site.
  NavigationSimulator::NavigateAndCommitFromBrowser(
      contents(), GURL("http://www.google.com/a.html"));
  orig_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Set a capture handle.
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"Some handle.";
  contents()->SetCaptureHandleConfig(config.Clone());

  // Expect that navigation to a new site will reset the capture handle config.
  const auto empty_config = blink::mojom::CaptureHandleConfig::New();
  TestWebContentsObserver observer(contents());
  observer.ExpectOnCaptureHandleConfigUpdate(empty_config.Clone());

  // Navigate to the second site.
  auto new_site_navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://www.google.com/b.html"), contents());
  new_site_navigation->ReadyToCommit();

  // Further proof that the config was reset.
  EXPECT_EQ(contents()->GetCaptureHandleConfig(), *empty_config);
}

TEST_F(WebContentsImplTest,
       SameDocumentMainPageNavigationDoesNotClearCaptureHandleConfig) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  ASSERT_EQ(orig_rfh, orig_rfh->GetMainFrame());

  // Navigate to the first site.
  NavigationSimulator::NavigateAndCommitFromBrowser(
      contents(), GURL("http://www.google.com/index.html"));
  orig_rfh->GetSiteInstance()->group()->IncrementActiveFrameCount();

  // Set a capture handle.
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"Some handle.";
  contents()->SetCaptureHandleConfig(config.Clone());

  // ExpectOnCaptureHandleConfigUpdate() not called - the test will fail
  // if OnCaptureHandleConfigUpdate() is called.
  TestWebContentsObserver observer(contents());

  // Navigate to the second site.
  auto new_site_navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("http://www.google.com/index.html#same_doc"), contents());
  new_site_navigation->ReadyToCommit();

  // Further proof that the config was not reset.
  EXPECT_EQ(contents()->GetCaptureHandleConfig(), *config);
}

TEST_F(WebContentsImplTest,
       CrossDocumentChildPageNavigationDoesNotClearCaptureHandleConfig) {
  TestRenderFrameHost* orig_rfh = main_test_rfh();
  ASSERT_EQ(orig_rfh, orig_rfh->GetMainFrame());

  NavigationSimulator::NavigateAndCommitFromBrowser(
      contents(), GURL("http://www.google.com/a.html"));

  TestRenderFrameHost* subframe = orig_rfh->AppendChild("subframe");
  ASSERT_NE(subframe, subframe->GetMainFrame());
  subframe->GetSiteInstance()->group()->IncrementActiveFrameCount();
  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://www.google.com/b.html"), subframe);

  // Set a capture handle.
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"Some handle.";
  contents()->SetCaptureHandleConfig(config.Clone());

  // ExpectOnCaptureHandleConfigUpdate() not called - the test will fail
  // if OnCaptureHandleConfigUpdate() is called.
  TestWebContentsObserver observer(contents());

  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://www.google.com/c.html"), subframe);

  // Further proof that the config was not reset.
  EXPECT_EQ(contents()->GetCaptureHandleConfig(), *config);
}

class TestCanonicalUrlLocalFrame : public content::FakeLocalFrame,
                                   public WebContentsObserver {
 public:
  explicit TestCanonicalUrlLocalFrame(WebContents* web_contents,
                                      std::optional<GURL> canonical_url)
      : WebContentsObserver(web_contents), canonical_url_(canonical_url) {}

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    if (!initialized_) {
      initialized_ = true;
      Init(render_frame_host->GetRemoteAssociatedInterfaces());
    }
  }

  void GetCanonicalUrlForSharing(
      base::OnceCallback<void(const std::optional<GURL>&)> callback) override {
    std::move(callback).Run(canonical_url_);
  }

 private:
  bool initialized_ = false;
  std::optional<GURL> canonical_url_;
};

TEST_F(WebContentsImplTest, CanonicalUrlSchemeHttpsIsAllowed) {
  TestCanonicalUrlLocalFrame local_frame(contents(), GURL("https://someurl/"));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GURL("https://site/"));

  base::RunLoop run_loop;
  std::optional<GURL> canonical_url;
  base::RepeatingClosure quit = run_loop.QuitClosure();
  auto on_done = [&](const std::optional<GURL>& result) {
    canonical_url = result;
    quit.Run();
  };
  contents()->GetPrimaryMainFrame()->GetCanonicalUrl(
      base::BindLambdaForTesting(on_done));
  run_loop.Run();

  ASSERT_TRUE(canonical_url);
  EXPECT_EQ(GURL("https://someurl/"), *canonical_url);
}

TEST_F(WebContentsImplTest, CanonicalUrlSchemeChromeIsNotAllowed) {
  TestCanonicalUrlLocalFrame local_frame(contents(), GURL("chrome://someurl/"));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(),
                                                    GURL("https://site/"));

  base::RunLoop run_loop;
  std::optional<GURL> canonical_url;
  base::RepeatingClosure quit = run_loop.QuitClosure();
  auto on_done = [&](const std::optional<GURL>& result) {
    canonical_url = result;
    quit.Run();
  };
  contents()->GetPrimaryMainFrame()->GetCanonicalUrl(
      base::BindLambdaForTesting(on_done));
  run_loop.Run();

  ASSERT_FALSE(canonical_url) << "canonical_url=" << *canonical_url;
}

TEST_F(WebContentsImplTest, RequestMediaAccessPermissionNoDelegate) {
  MediaStreamRequest dummy_request(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
      /*url_origin=*/url::Origin::Create(GURL("")), /*user_gesture=*/false,
      blink::MediaStreamRequestType::MEDIA_GENERATE_STREAM,
      /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  bool callback_run = false;
  contents()->RequestMediaAccessPermission(
      dummy_request,
      base::BindLambdaForTesting(
          [&callback_run](
              const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result,
              std::unique_ptr<MediaStreamUI> ui) {
            EXPECT_TRUE(stream_devices_set.stream_devices.empty());
            EXPECT_EQ(
                result,
                blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN);
            callback_run = true;
          }));
  ASSERT_TRUE(callback_run);
}

TEST_F(WebContentsImplTest, IgnoreInputEvents) {
  // By default, input events should not be ignored.
  EXPECT_FALSE(contents()->ShouldIgnoreInputEvents());
  std::optional<WebContents::ScopedIgnoreInputEvents> ignore_1 =
      contents()->IgnoreInputEvents(std::nullopt);
  EXPECT_TRUE(contents()->ShouldIgnoreInputEvents());

  // A second request to ignore should continue to ignore events.
  WebContents::ScopedIgnoreInputEvents ignore_2 =
      contents()->IgnoreInputEvents(std::nullopt);
  EXPECT_TRUE(contents()->ShouldIgnoreInputEvents());

  // Releasing one of them should not change anything.
  ignore_1.reset();
  EXPECT_TRUE(contents()->ShouldIgnoreInputEvents());

  // Move construction should not allow input.
  WebContents::ScopedIgnoreInputEvents ignore_3(std::move(ignore_2));
  EXPECT_TRUE(contents()->ShouldIgnoreInputEvents());

  {
    // Cannot create an empty `ScopedIgnoreInputEvents`, so get a new one and
    // move-assign over it to verify that we end up with one outstanding token.
    WebContents::ScopedIgnoreInputEvents ignore_4 =
        contents()->IgnoreInputEvents(std::nullopt);
    ignore_4 = std::move(ignore_3);
    EXPECT_TRUE(contents()->ShouldIgnoreInputEvents());
    // `ignore_4` goes out of scope.
  }

  // Now input should be allowed.
  EXPECT_FALSE(contents()->ShouldIgnoreInputEvents());
}

TEST_F(WebContentsImplTest, OnColorProviderChangedTriggersPageBroadcast) {
  TestColorProviderSource color_provider_source;
  mojo::AssociatedRemote<blink::mojom::PageBroadcast> broadcast_remote;
  testing::NiceMock<MockPageBroadcast> mock_page_broadcast(
      broadcast_remote.BindNewEndpointAndPassDedicatedReceiver());
  contents()->GetRenderViewHost()->BindPageBroadcast(broadcast_remote.Unbind());

  contents()->SetColorProviderSource(&color_provider_source);
  const auto color_provider_colors = contents()->GetColorProviderColorMaps();
  color_provider_source.NotifyColorProviderChanged();

  // The page broadcast should have been called twice. Once when first set and
  // again when the source notified of a ColorProvider change.
  EXPECT_CALL(mock_page_broadcast, UpdateColorProviders(color_provider_colors))
      .Times(2);
  mock_page_broadcast.FlushForTesting();
}

TEST_F(WebContentsImplTest, InvalidNetworkHandleAsDefault) {
  WebContents::CreateParams params(browser_context());
  std::unique_ptr<WebContents> contents(WebContents::Create(params));
  EXPECT_EQ(net::handles::kInvalidNetworkHandle, contents->GetTargetNetwork());
}

TEST_F(WebContentsImplTest, CreateWebContentsWithNetworkHandle) {
  int64_t test_target_network_handle = 100;
  WebContents::CreateParams params(browser_context());
  params.target_network = test_target_network_handle;

  std::unique_ptr<WebContents> contents(WebContents::Create(params));
  EXPECT_EQ(test_target_network_handle, contents->GetTargetNetwork());
}

TEST_F(WebContentsImplTest, CreateWebContentsWithOpenerAndNetworkHandle) {
  int64_t test_target_network_handle = 100;
  WebContents::CreateParams params(browser_context());
  params.target_network = test_target_network_handle;

  std::unique_ptr<WebContentsImpl> contents(
      WebContentsImpl::CreateWithOpener(params, /*opener_rfh=*/nullptr));
  EXPECT_EQ(test_target_network_handle, contents->GetTargetNetwork());
}

TEST_F(WebContentsImplTest, BadDownloadImageFromAXNodeId) {
  // Avoid using TestWebContents, which fakes image download logic without
  // exercising the code in WebContentsImpl.
  scoped_refptr<SiteInstance> instance =
      SiteInstance::Create(GetBrowserContext());
  instance->GetProcess()->Init();
  WebContents::CreateParams create_params(GetBrowserContext(),
                                          std::move(instance));
  create_params.desired_renderer_state = WebContents::CreateParams::
      CreateParams::kInitializeAndWarmupRendererProcess;
  std::unique_ptr<WebContentsImpl> contents(
      WebContentsImpl::CreateWithOpener(create_params, /*opener_rfh=*/nullptr));
  ASSERT_FALSE(
      contents->GetPrimaryMainFrame()->GetProcess()->ShutdownRequested());

  // Set up the fake image downloader.
  FakeImageDownloader fake_image_downloader;
  fake_image_downloader.Init(
      contents->GetPrimaryMainFrame()->GetRemoteInterfaces());

  int img_node_id = 3;
  fake_image_downloader.SetFakeResponseData(img_node_id, {},
                                            {gfx::Size(30, 30)});

  base::RunLoop run_loop;
  contents->DownloadImageFromAxNode(
      contents->GetPrimaryMainFrame()->GetAXTreeID(), img_node_id, gfx::Size(),
      0, false,
      base::BindLambdaForTesting([&](int download_id, int http_status_code,
                                     const GURL& url,
                                     const std::vector<SkBitmap>& bitmaps,
                                     const std::vector<gfx::Size>& sizes) {
        EXPECT_EQ(400, http_status_code);
        EXPECT_TRUE(bitmaps.empty());
        EXPECT_TRUE(sizes.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace content
