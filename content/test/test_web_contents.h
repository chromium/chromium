// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_WEB_CONTENTS_H_
#define CONTENT_TEST_TEST_WEB_CONTENTS_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "ui/base/page_transition_types.h"

class GURL;
class Referrer;
class SkBitmap;

namespace gfx {
class Size;
}

namespace net {
class HttpResponseHeaders;
}

namespace content {

class NavigationHandle;
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
  TestRenderFrameHost* GetMainFrame() override;
  TestRenderViewHost* GetRenderViewHost() override;
  // Overrides to avoid establishing Mojo connection with renderer process.
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t preferred_size,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    ImageDownloadCallback callback) override;
  const GURL& GetLastCommittedURL() override;
  const base::string16& GetTitle() override;

  // WebContentsTester implementation.
  void CommitPendingNavigation() override;
  TestRenderFrameHost* GetPendingMainFrame() override;

  void NavigateAndCommit(
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) override;

  void NavigateAndFail(
      const GURL& url,
      int error_code,
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void TestSetIsLoading(bool value) override;
  void TestDidNavigate(RenderFrameHost* render_frame_host,
                       int nav_entry_id,
                       bool did_create_new_entry,
                       const GURL& url,
                       ui::PageTransition transition) override;
  void TestDidNavigateWithSequenceNumber(RenderFrameHost* render_frame_host,
                                         int nav_entry_id,
                                         bool did_create_new_entry,
                                         const GURL& url,
                                         const Referrer& referrer,
                                         ui::PageTransition transition,
                                         bool was_within_same_document,
                                         int item_sequence_number,
                                         int document_sequence_number);
  void SetHttpResponseHeaders(
      NavigationHandle* navigation_handle,
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void SetOpener(WebContents* opener) override;
  const std::string& GetSaveFrameHeaders() override;
  const base::string16& GetSuggestedFileName() override;
  bool HasPendingDownloadImage(const GURL& url) override;
  bool TestDidDownloadImage(
      const GURL& url,
      int http_status_code,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes) override;
  void SetLastCommittedURL(const GURL& url) override;
  void SetTitle(const base::string16& new_title) override;
  void SetMainFrameMimeType(const std::string& mime_type) override;
  void SetIsCurrentlyAudible(bool audible) override;
  void TestDidReceiveInputEvent(blink::WebInputEvent::Type type) override;
  void TestDidFinishLoad(const GURL& url) override;
  void TestDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description) override;

  // True if a cross-site navigation is pending.
  bool CrossProcessNavigationPending();

  // Prevent interaction with views.
  bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      int opener_frame_routing_id,
      int proxy_routing_id,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_frame_state) override;
  void UpdateRenderViewSizeForRenderManager(bool is_main_frame) override {}

  // Returns a clone of this TestWebContents. The returned object is also a
  // TestWebContents. The caller owns the returned object.
  std::unique_ptr<WebContents> Clone() override;

  // Allow mocking of the RenderViewHostDelegateView.
  RenderViewHostDelegateView* GetDelegateView() override;
  void set_delegate_view(RenderViewHostDelegateView* view) {
    delegate_view_override_ = view;
  }

  // Allows us to simulate that a contents was created via CreateNewWindow.
  void AddPendingContents(std::unique_ptr<WebContentsImpl> contents);

  // Establish expected arguments for |SetHistoryOffsetAndLength()|. When
  // |SetHistoryOffsetAndLength()| is called, the arguments are compared
  // with the expected arguments specified here.
  void ExpectSetHistoryOffsetAndLength(int history_offset,
                                       int history_length);

  // Compares the arguments passed in with the expected arguments passed in
  // to |ExpectSetHistoryOffsetAndLength()|.
  void SetHistoryOffsetAndLength(int history_offset,
                                 int history_length) override;

  // Records that this was called and returns and empty vector.
  std::vector<mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>>
  PauseSubresourceLoading() override;

  bool GetPauseSubresourceLoadingCalled() override;

  void ResetPauseSubresourceLoadingCalled() override;

  void SetPageImportanceSignals(PageImportanceSignals signals) override;

  void SetLastActiveTime(base::TimeTicks last_active_time) override;

  void SetIsConnectedToBluetoothDevice(
      bool is_connected_to_bluetooth_device) override;

  // Override IsConnectedToBluetoothDevice() to allow using the mocked value.
  bool IsConnectedToBluetoothDevice() override;

  base::UnguessableToken GetAudioGroupId() override;

 protected:
  // The deprecated WebContentsTester still needs to subclass this.
  explicit TestWebContents(BrowserContext* browser_context);

 private:
  // WebContentsImpl overrides
  RenderFrameHostDelegate* CreateNewWindow(
      RenderFrameHost* opener,
      const mojom::CreateNewWindowParams& params,
      bool is_new_browsing_instance,
      bool has_user_gesture,
      SessionStorageNamespace* session_storage_namespace) override;
  void CreateNewWidget(int32_t render_process_id,
                       int32_t route_id,
                       mojo::PendingRemote<mojom::Widget> widget,
                       RenderViewHostImpl* render_view_host) override;
  void CreateNewFullscreenWidget(int32_t render_process_id,
                                 int32_t route_id,
                                 mojo::PendingRemote<mojom::Widget> widget,
                                 RenderViewHostImpl* render_view_host) override;
  void ShowCreatedWindow(int process_id,
                         int route_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override;
  void ShowCreatedWidget(int process_id,
                         int route_id,
                         const gfx::Rect& initial_rect) override;
  void ShowCreatedFullscreenWidget(int process_id, int route_id) override;
  void SaveFrameWithHeaders(const GURL& url,
                            const Referrer& referrer,
                            const std::string& headers,
                            const base::string16& suggested_filename) override;

  RenderViewHostDelegateView* delegate_view_override_;

  // Expectations for arguments of |SetHistoryOffsetAndLength()|.
  bool expect_set_history_offset_and_length_;
  int expect_set_history_offset_and_length_history_offset_;
  int expect_set_history_offset_and_length_history_length_;
  std::string save_frame_headers_;
  base::string16 suggested_filename_;
  // Map keyed by image URL. Values are <id, callback> pairs.
  std::map<GURL, std::list<std::pair<int, ImageDownloadCallback>>>
      pending_image_downloads_;
  GURL last_committed_url_;
  base::Optional<base::string16> title_;
  bool pause_subresource_loading_called_;
  base::UnguessableToken audio_group_id_;
  bool is_connected_to_bluetooth_device_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_WEB_CONTENTS_H_
