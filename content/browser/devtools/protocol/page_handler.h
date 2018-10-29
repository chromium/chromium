// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/common/javascript_dialog_type.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

class SkBitmap;

namespace base {
class UnguessableToken;
}

namespace gfx {
class Image;
}  // namespace gfx

namespace blink {
struct WebDeviceEmulationParams;
}

namespace content {

class DevToolsAgentHostImpl;
class NavigationRequest;
class RenderFrameHostImpl;
class WebContentsImpl;

namespace protocol {

class EmulationHandler;

class PageHandler : public DevToolsDomainHandler,
                    public Page::Backend,
                    public RenderWidgetHostObserver {
 public:
  PageHandler(EmulationHandler* handler, bool allow_set_download_behavior);
  ~PageHandler() override;

  static std::vector<PageHandler*> EnabledForWebContents(
      WebContentsImpl* contents);
  static std::vector<PageHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void OnSynchronousSwapCompositorFrame(
      viz::CompositorFrameMetadata frame_metadata);
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();
  bool screencast_enabled() const { return enabled_ && screencast_enabled_; }
  using JavaScriptDialogCallback =
      content::JavaScriptDialogManager::DialogClosedCallback;
  void DidRunJavaScriptDialog(const GURL& url,
                              const base::string16& message,
                              const base::string16& default_prompt,
                              JavaScriptDialogType dialog_type,
                              bool has_non_devtools_handlers,
                              JavaScriptDialogCallback callback);
  void DidRunBeforeUnloadConfirm(const GURL& url,
                                 bool has_non_devtools_handlers,
                                 JavaScriptDialogCallback callback);
  void DidCloseJavaScriptDialog(bool success, const base::string16& user_input);
  void NavigationReset(NavigationRequest* navigation_request);

  Response Enable() override;
  Response Disable() override;

  Response Crash() override;
  Response Close() override;
  void Reload(Maybe<bool> bypassCache,
              Maybe<std::string> script_to_evaluate_on_load,
              std::unique_ptr<ReloadCallback> callback) override;
  void Navigate(const std::string& url,
                Maybe<std::string> referrer,
                Maybe<std::string> transition_type,
                Maybe<std::string> frame_id,
                std::unique_ptr<NavigateCallback> callback) override;
  Response StopLoading() override;

  using NavigationEntries = protocol::Array<Page::NavigationEntry>;
  Response GetNavigationHistory(
      int* current_index,
      std::unique_ptr<NavigationEntries>* entries) override;
  Response NavigateToHistoryEntry(int entry_id) override;

  void CaptureScreenshot(
      Maybe<std::string> format,
      Maybe<int> quality,
      Maybe<Page::Viewport> clip,
      Maybe<bool> from_surface,
      std::unique_ptr<CaptureScreenshotCallback> callback) override;
  void PrintToPDF(Maybe<bool> landscape,
                  Maybe<bool> display_header_footer,
                  Maybe<bool> print_background,
                  Maybe<double> scale,
                  Maybe<double> paper_width,
                  Maybe<double> paper_height,
                  Maybe<double> margin_top,
                  Maybe<double> margin_bottom,
                  Maybe<double> margin_left,
                  Maybe<double> margin_right,
                  Maybe<String> page_ranges,
                  Maybe<bool> ignore_invalid_page_ranges,
                  Maybe<String> header_template,
                  Maybe<String> footer_template,
                  Maybe<bool> prefer_css_page_size,
                  std::unique_ptr<PrintToPDFCallback> callback) override;
  Response StartScreencast(Maybe<std::string> format,
                           Maybe<int> quality,
                           Maybe<int> max_width,
                           Maybe<int> max_height,
                           Maybe<int> every_nth_frame) override;
  Response StopScreencast() override;
  Response ScreencastFrameAck(int session_id) override;

  Response HandleJavaScriptDialog(bool accept,
                                  Maybe<std::string> prompt_text) override;

  Response RequestAppBanner() override;

  Response BringToFront() override;

  Response SetDownloadBehavior(const std::string& behavior,
                               Maybe<std::string> download_path) override;

  void GetAppManifest(
      std::unique_ptr<GetAppManifestCallback> callback) override;

  Response SetWebLifecycleState(const std::string& state) override;

 private:
  enum EncodingFormat { PNG, JPEG };

  WebContentsImpl* GetWebContents();
  void NotifyScreencastVisibility(bool visible);
  void InnerSwapCompositorFrame();
  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame);
  void ScreencastFrameCaptured(
      std::unique_ptr<Page::ScreencastFrameMetadata> metadata,
      const SkBitmap& bitmap);
  void ScreencastFrameEncoded(
      std::unique_ptr<Page::ScreencastFrameMetadata> metadata,
      const std::string& data);

  void ScreenshotCaptured(
      std::unique_ptr<CaptureScreenshotCallback> callback,
      const std::string& format,
      int quality,
      const gfx::Size& original_view_size,
      const gfx::Size& requested_image_size,
      const blink::WebDeviceEmulationParams& original_params,
      const gfx::Image& image);

  void GotManifest(std::unique_ptr<GetAppManifestCallback> callback,
                   const GURL& manifest_url,
                   blink::mojom::ManifestDebugInfoPtr debug_info);

  // RenderWidgetHostObserver overrides.
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  bool enabled_;

  bool screencast_enabled_;
  std::string screencast_format_;
  int screencast_quality_;
  int screencast_max_width_;
  int screencast_max_height_;
  int capture_every_nth_frame_;
  int capture_retry_count_;
  bool has_compositor_frame_metadata_;
  viz::CompositorFrameMetadata next_compositor_frame_metadata_;
  viz::CompositorFrameMetadata last_compositor_frame_metadata_;
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

  RenderFrameHostImpl* host_;
  EmulationHandler* emulation_handler_;
  bool allow_set_download_behavior_;
  std::unique_ptr<Page::Frontend> frontend_;
  ScopedObserver<RenderWidgetHost, RenderWidgetHostObserver> observer_;
  JavaScriptDialogCallback pending_dialog_;
  scoped_refptr<DevToolsDownloadManagerDelegate> download_manager_delegate_;
  base::flat_map<base::UnguessableToken, std::unique_ptr<NavigateCallback>>
      navigate_callbacks_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
