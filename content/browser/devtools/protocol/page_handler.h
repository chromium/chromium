// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/common/javascript_dialog_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class SkBitmap;

namespace base {
class UnguessableToken;
}

namespace gfx {
class Image;
}  // namespace gfx

namespace content {

class BackForwardCacheCanStoreDocumentResult;
class DevToolsAgentHostImpl;
class FrameTreeNode;
class NavigationRequest;
class RenderFrameHostImpl;
class WebContentsImpl;

namespace protocol {

class BrowserHandler;
class EmulationHandler;

class PageHandler : public DevToolsDomainHandler,
                    public Page::Backend,
                    public RenderWidgetHostObserver,
                    public download::DownloadItem::Observer {
 public:
  PageHandler(EmulationHandler* emulation_handler,
              BrowserHandler* browser_handler,
              bool allow_unsafe_operations,
              bool is_trusted,
              std::optional<url::Origin> navigation_initiator_origin,
              bool may_read_local_files);

  PageHandler(const PageHandler&) = delete;
  PageHandler& operator=(const PageHandler&) = delete;

  ~PageHandler() override;

  static std::vector<PageHandler*> EnabledForWebContents(
      WebContentsImpl* contents);
  static std::vector<PageHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  // Instrumentation signals.
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();
  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;
  void DidRunJavaScriptDialog(const GURL& url,
                              const std::u16string& message,
                              const std::u16string& default_prompt,
                              JavaScriptDialogType dialog_type,
                              bool has_non_devtools_handlers,
                              JavaScriptDialogCallback callback);
  void DidRunBeforeUnloadConfirm(const GURL& url,
                                 bool has_non_devtools_handlers,
                                 JavaScriptDialogCallback callback);
  void DidCloseJavaScriptDialog(bool success, const std::u16string& user_input);
  void NavigationReset(NavigationRequest* navigation_request);
  void DownloadWillBegin(FrameTreeNode* ftn, download::DownloadItem* item);

  void OnFrameDetached(const base::UnguessableToken& frame_id);
  void DidChangeFrameLoadingState(const FrameTreeNode& ftn);

  bool ShouldBypassCSP();
  void BackForwardCacheNotUsed(
      const NavigationRequest* nav_request,
      const BackForwardCacheCanStoreDocumentResult* result,
      const BackForwardCacheCanStoreTreeResult* tree_result);

  void IsPrerenderingAllowed(bool& is_allowed);

  Response Enable() override;
  Response Disable() override;

  Response Crash() override;
  Response Close() override;
  void Reload(Maybe<bool> bypassCache,
              Maybe<std::string> script_to_evaluate_on_load,
              Maybe<std::string> loader_id,
              std::unique_ptr<ReloadCallback> callback) override;
  void Navigate(const std::string& url,
                Maybe<std::string> referrer,
                Maybe<std::string> transition_type,
                Maybe<std::string> frame_id,
                Maybe<std::string> referrer_policy,
                std::unique_ptr<NavigateCallback> callback) override;
  Response StopLoading() override;

  using NavigationEntries = protocol::Array<Page::NavigationEntry>;
  Response GetNavigationHistory(
      int* current_index,
      std::unique_ptr<NavigationEntries>* entries) override;
  Response NavigateToHistoryEntry(int entry_id) override;
  Response ResetNavigationHistory() override;

  void CaptureScreenshot(
      Maybe<std::string> format,
      Maybe<int> quality,
      Maybe<Page::Viewport> clip,
      Maybe<bool> from_surface,
      Maybe<bool> capture_beyond_viewport,
      Maybe<bool> optimize_for_speed,
      std::unique_ptr<CaptureScreenshotCallback> callback) override;
  void CaptureSnapshot(
      Maybe<std::string> format,
      std::unique_ptr<CaptureSnapshotCallback> callback) override;
  Response StartScreencast(Maybe<std::string> format,
                           Maybe<int> quality,
                           Maybe<int> max_width,
                           Maybe<int> max_height,
                           Maybe<int> every_nth_frame) override;
  Response StopScreencast() override;
  Response ScreencastFrameAck(int session_id) override;

  Response HandleJavaScriptDialog(bool accept,
                                  Maybe<std::string> prompt_text) override;

  Response BringToFront() override;

  Response SetDownloadBehavior(const std::string& behavior,
                               Maybe<std::string> download_path) override;

  void GetAppManifest(
      protocol::Maybe<std::string> manifest_id,
      std::unique_ptr<GetAppManifestCallback> callback) override;

  Response SetWebLifecycleState(const std::string& state) override;
  void GetInstallabilityErrors(
      std::unique_ptr<GetInstallabilityErrorsCallback> callback) override;

  void GetManifestIcons(
      std::unique_ptr<GetManifestIconsCallback> callback) override;

  void GetAppId(std::unique_ptr<GetAppIdCallback> callback) override;

  Response SetBypassCSP(bool enabled) override;
  Response AddCompilationCache(const std::string& url,
                               const Binary& data) override;

  Response SetPrerenderingAllowed(bool is_allowed) override;

  Response AssureTopLevelActiveFrame();

 private:
  struct PendingScreenshotRequest;

  using BitmapEncoder =
      base::RepeatingCallback<bool(const SkBitmap& bitmap,
                                   std::vector<uint8_t>& output)>;

  void CaptureFullPageScreenshot(
      Maybe<std::string> format,
      Maybe<int> quality,
      Maybe<bool> optimize_for_speed,
      std::unique_ptr<CaptureScreenshotCallback> callback,
      const gfx::Size& full_page_size);
  bool ShouldCaptureNextScreencastFrame();
  void NotifyScreencastVisibility(bool visible);
  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame);
  void ScreencastFrameCaptured(
      std::unique_ptr<Page::ScreencastFrameMetadata> metadata,
      const SkBitmap& bitmap);
  void ScreencastFrameEncoded(
      std::unique_ptr<Page::ScreencastFrameMetadata> metadata,
      std::vector<uint8_t> data);

  void ScreenshotCaptured(std::unique_ptr<PendingScreenshotRequest> request,
                          const gfx::Image& image);

  // RenderWidgetHostObserver overrides.
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  // DownloadItem::Observer overrides
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  // Returns WebContents only if `host_` is a top level frame. Otherwise, it
  // returns Response with an error.
  using ResponseOrWebContents = absl::variant<Response, WebContentsImpl*>;
  ResponseOrWebContents GetWebContentsForTopLevelActiveFrame();

  const bool allow_unsafe_operations_;
  const bool is_trusted_;
  const std::optional<url::Origin> navigation_initiator_origin_;
  const bool may_read_local_files_;

  bool enabled_;
  bool bypass_csp_ = false;

  BitmapEncoder screencast_encoder_;
  int screencast_max_width_;
  int screencast_max_height_;
  int capture_every_nth_frame_;
  int session_id_;
  int frame_counter_;
  int frames_in_flight_;

  // |video_consumer_| consumes video frames from FrameSinkVideoCapturerImpl,
  // and provides PageHandler with these frames via OnFrameFromVideoConsumer.
  // This is only used if Viz is enabled and if OS is not Android.
  std::unique_ptr<DevToolsVideoConsumer> video_consumer_;

  // The last surface size used to determine if frames with new sizes need
  // to be requested. This changes due to window resizing.
  gfx::Size last_surface_size_;

  raw_ptr<RenderFrameHostImpl> host_;
  raw_ptr<EmulationHandler> emulation_handler_;
  raw_ptr<BrowserHandler> browser_handler_;

  std::unique_ptr<Page::Frontend> frontend_;

  base::ScopedObservation<RenderWidgetHost, RenderWidgetHostObserver>
      observation_{this};
  JavaScriptDialogCallback pending_dialog_;
  // Maps DevTools navigation tokens to pending NavigateCallbacks.
  base::flat_map<base::UnguessableToken, std::unique_ptr<NavigateCallback>>
      navigate_callbacks_;
  base::flat_set<raw_ptr<download::DownloadItem, CtnExperimental>>
      pending_downloads_;

  bool is_prerendering_allowed_ = true;

  base::WeakPtrFactory<PageHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
