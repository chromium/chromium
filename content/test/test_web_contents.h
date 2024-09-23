// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_WEB_CONTENTS_H_
#define CONTENT_TEST_TEST_WEB_CONTENTS_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-forward.h"
#include "ui/base/page_transition_types.h"

class GURL;
class SkBitmap;

namespace gfx {
class Size;
}

namespace content {

struct Referrer;
class RenderViewHost;
class TestRenderViewHost;
class WebContentsTester;

// Subclass WebContentsImpl to ensure it creates TestRenderViewHosts
// and does not do anything involving views.
class TestWebContents : public WebContentsImpl, public WebContentsTester {
 public:
  ~TestWebContents() override;

  static std::unique_ptr<TestWebContents> Create(
      BrowserContext* browser_context,
      scoped_refptr<SiteInstance> instance);
  static TestWebContents* Create(const CreateParams& params);

  // WebContentsImpl overrides (returning the same values, but in Test* types)
  const TestRenderFrameHost* GetPrimaryMainFrame() const override;
  TestRenderFrameHost* GetPrimaryMainFrame() override;
  TestRenderViewHost* GetRenderViewHost() override;
  // Overrides to avoid establishing Mojo connection with renderer process.
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    const gfx::Size& preferred_size,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    ImageDownloadCallback callback) override;
  const GURL& GetLastCommittedURL() override;
  const std::u16string& GetTitle() override;

  // Override to cache the tab switch start time without going through
  // VisibleTimeRequestTrigger.
  void SetTabSwitchStartTime(base::TimeTicks start_time,
                             bool destination_is_loaded) final;

  // WebContentsTester implementation.
  void CommitPendingNavigation() override;

  void NavigateAndCommit(
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) override;

  void NavigateAndFail(const GURL& url, int error_code) override;
  void TestSetIsLoading(bool value) override;
  void SetOpener(WebContents* opener) override;
  void SetIsCrashed(base::TerminationStatus status, int error_code) override;
  const std::string& GetSaveFrameHeaders() override;
  const std::u16string& GetSuggestedFileName() override;
  bool HasPendingDownloadImage(const GURL& url) override;
  bool TestDidDownloadImage(
      const GURL& url,
      int http_status_code,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes) override;
  void TestSetFaviconURL(
      const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) override;
  void TestUpdateFaviconURL(
      const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) override;
  void SetLastCommittedURL(const GURL& url) override;
  void SetTitle(const std::u16string& new_title) override;
  void SetMainFrameMimeType(const std::string& mime_type) override;
  void SetMainFrameSize(const gfx::Size& frame_size) override;
  const std::string& GetContentsMimeType() override;
  void SetIsCurrentlyAudible(bool audible) override;
  void TestDidReceiveMouseDownEvent() override;
  void TestDidFinishLoad(const GURL& url) override;
  void TestDidFailLoadWithError(const GURL& url, int error_code) override;
  void TestDidFirstVisuallyNonEmptyPaint() override;

  // True if a cross-site navigation is pending.
  bool CrossProcessNavigationPending();

  // Prevent interaction with views.
  bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      const std::optional<blink::FrameToken>& opener_frame_token,
      RenderFrameProxyHost* proxy_host) override;

  // Returns a clone of this TestWebContents. The returned object is also a
  // TestWebContents. The caller owns the returned object.
  std::unique_ptr<WebContents> Clone() override;

  // Allow mocking of the RenderViewHostDelegateView.
  RenderViewHostDelegateView* GetDelegateView() override;
  void set_delegate_view(RenderViewHostDelegateView* view) {
    delegate_view_override_ = view;
  }

  // Allows us to simulate that a contents was created via CreateNewWindow.
  void AddPendingContents(std::unique_ptr<WebContentsImpl> contents,
                          const GURL& target_url);

  bool GetPauseSubresourceLoadingCalled() override;

  void ResetPauseSubresourceLoadingCalled() override;

  void SetLastActiveTimeTicks(base::TimeTicks last_active_time_ticks) override;
  void SetLastActiveTime(base::Time last_active_time) override;

  void TestIncrementUsbActiveFrameCount() override;
  void TestDecrementUsbActiveFrameCount() override;

  void TestIncrementHidActiveFrameCount() override;
  void TestDecrementHidActiveFrameCount() override;

  void TestIncrementSerialActiveFrameCount() override;
  void TestDecrementSerialActiveFrameCount() override;

  void TestIncrementBluetoothConnectedDeviceCount() override;
  void TestDecrementBluetoothConnectedDeviceCount() override;

  base::UnguessableToken GetAudioGroupId() override;

  void OnWebPreferencesChanged() override;

  // If set, *web_preferences_changed_counter_ is incremented when
  // OnWebPreferencesChanged() is called.
  void set_web_preferences_changed_counter(int* counter) {
    web_preferences_changed_counter_ = counter;
  }

  void SetBackForwardCacheSupported(bool supported);

  bool IsPageFrozen() override;

  TestRenderFrameHost* GetSpeculativePrimaryMainFrame();

  FrameTreeNodeId AddPrerender(const GURL& url) override;
  TestRenderFrameHost* AddPrerenderAndCommitNavigation(
      const GURL& url) override;
  std::unique_ptr<NavigationSimulator> AddPrerenderAndStartNavigation(
      const GURL& url) override;
  void ActivatePrerenderedPage(const GURL& url) override;
  // This is equivalent to ActivatePrerenderedPage() except that this activates
  // a prerendered page by navigation initiated by the address bar.
  void ActivatePrerenderedPageFromAddressBar(const GURL& url);

  base::TimeTicks GetTabSwitchStartTime() final;

  void SetPictureInPictureOptions(
      std::optional<blink::mojom::PictureInPictureWindowOptions> options)
      override;

  void SetOverscrollNavigationEnabled(bool enabled) override;
  bool GetOverscrollNavigationEnabled() override;

  void SetSafeAreaInsetsHost(
      std::unique_ptr<SafeAreaInsetsHost> safe_area_insets_host);

  void GetMediaCaptureRawDeviceIdsOpened(
      blink::mojom::MediaStreamType type,
      base::OnceCallback<void(std::vector<std::string>)> callback) override;

  void SetMediaCaptureRawDeviceIdsOpened(blink::mojom::MediaStreamType type,
                                         std::vector<std::string> ids) override;

 protected:
  // The deprecated WebContentsTester still needs to subclass this.
  explicit TestWebContents(BrowserContext* browser_context);

 private:
  // WebContentsImpl overrides
  FrameTree* CreateNewWindow(
      RenderFrameHostImpl* opener,
      const mojom::CreateNewWindowParams& params,
      bool is_new_browsing_instance,
      bool has_user_gesture,
      SessionStorageNamespace* session_storage_namespace) override;
  RenderWidgetHostImpl* CreateNewPopupWidget(
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t route_id,
      mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
          blink_popup_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget)
      override;
  void ShowCreatedWindow(RenderFrameHostImpl* opener,
                         int route_id,
                         WindowOpenDisposition disposition,
                         const blink::mojom::WindowFeatures& window_features,
                         bool user_gesture) override;
  void ShowCreatedWidget(int process_id,
                         int route_id,
                         const gfx::Rect& initial_rect,
                         const gfx::Rect& initial_anchor_rect) override;
  void SaveFrameWithHeaders(const GURL& url,
                            const Referrer& referrer,
                            const std::string& headers,
                            const std::u16string& suggested_filename,
                            RenderFrameHost* rfh,
                            bool is_subresource) override;
  void ReattachToOuterWebContentsFrame() override {}
  void SetPageFrozen(bool frozen) override;
  bool IsBackForwardCacheSupported() override;
  const std::optional<blink::mojom::PictureInPictureWindowOptions>&
  GetPictureInPictureOptions() const override;

  raw_ptr<RenderViewHostDelegateView> delegate_view_override_;

  // See set_web_preferences_changed_counter() above. May be nullptr.
  raw_ptr<int> web_preferences_changed_counter_;
  std::string save_frame_headers_;
  std::u16string suggested_filename_;
  // Map keyed by image URL. Values are <id, callback> pairs.
  std::map<GURL, std::list<std::pair<int, ImageDownloadCallback>>>
      pending_image_downloads_;
  GURL last_committed_url_;
  std::optional<std::u16string> title_;
  bool pause_subresource_loading_called_;
  base::UnguessableToken audio_group_id_;
  bool is_page_frozen_;
  bool back_forward_cache_supported_ = true;
  base::TimeTicks tab_switch_start_time_;
  std::optional<blink::mojom::PictureInPictureWindowOptions>
      picture_in_picture_options_;
  bool overscroll_enabled_ = true;
  base::flat_map<blink::mojom::MediaStreamType, std::vector<std::string>>
      media_capture_raw_device_ids_opened_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_WEB_CONTENTS_H_
