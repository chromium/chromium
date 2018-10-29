// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() {
}

WebContents* WebContentsDelegate::OpenURLFromTab(WebContents* source,
                                                 const OpenURLParams& params) {
  return nullptr;
}

bool WebContentsDelegate::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  return true;
}

bool WebContentsDelegate::CanOverscrollContent() const { return false; }

bool WebContentsDelegate::ShouldSuppressDialogs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::ShouldPreserveAbortedURLs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::DidAddMessageToConsole(
    WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return false;
}

void WebContentsDelegate::BeforeUnloadFired(WebContents* web_contents,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

bool WebContentsDelegate::ShouldFocusLocationBarByDefault(WebContents* source) {
  return false;
}

bool WebContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

bool WebContentsDelegate::ShouldResumeRequestsForCreatedWindow() {
  return true;
}

bool WebContentsDelegate::TakeFocus(WebContents* source, bool reverse) {
  return false;
}

void WebContentsDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

bool WebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

KeyboardEventProcessingResult WebContentsDelegate::PreHandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebContentsDelegate::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  return false;
}

bool WebContentsDelegate::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  return false;
}

bool WebContentsDelegate::CanDragEnter(
    WebContents* source,
    const DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  return true;
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::ShouldCreateWebContents(
    WebContents* web_contents,
    RenderFrameHost* opener,
    SiteInstance* source_site_instance,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  return true;
}

JavaScriptDialogManager* WebContentsDelegate::GetJavaScriptDialogManager(
    WebContents* source) {
  return nullptr;
}

std::unique_ptr<BluetoothChooser> WebContentsDelegate::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

bool WebContentsDelegate::EmbedsFullscreenWidget() const {
  return false;
}

bool WebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return false;
}

blink::WebDisplayMode WebContentsDelegate::GetDisplayMode(
    const WebContents* web_contents) const {
  return blink::kWebDisplayModeBrowser;
}

content::ColorChooser* WebContentsDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return nullptr;
}

void WebContentsDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    std::unique_ptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
}

void WebContentsDelegate::EnumerateDirectory(
    WebContents* web_contents,
    std::unique_ptr<FileSelectListener> listener,
    const base::FilePath& path) {
  listener->FileSelectionCanceled();
}

void WebContentsDelegate::RequestMediaAccessPermission(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    MediaResponseCallback callback) {
  LOG(ERROR) << "WebContentsDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  std::move(callback).Run(MediaStreamDevices(), MEDIA_DEVICE_NOT_SUPPORTED,
                          std::unique_ptr<MediaStreamUI>());
}

bool WebContentsDelegate::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    MediaStreamType type) {
  LOG(ERROR) << "WebContentsDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string WebContentsDelegate::GetDefaultMediaDeviceID(
    WebContents* web_contents,
    MediaStreamType type) {
  return std::string();
}

#if defined(OS_ANDROID)
bool WebContentsDelegate::ShouldBlockMediaRequest(const GURL& url) {
  return false;
}

void WebContentsDelegate::SetOverlayMode(bool use_overlay_mode) {}
#endif

bool WebContentsDelegate::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  return false;
}

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    WebContents* web_contents = *attached_contents_.begin();
    web_contents->SetDelegate(nullptr);
  }
  DCHECK(attached_contents_.empty());
}

void WebContentsDelegate::Attach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) == attached_contents_.end());
  attached_contents_.insert(web_contents);
}

void WebContentsDelegate::Detach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) != attached_contents_.end());
  attached_contents_.erase(web_contents);
}

gfx::Size WebContentsDelegate::GetSizeForNewRenderView(
    WebContents* web_contents) const {
  return gfx::Size();
}

bool WebContentsDelegate::IsNeverVisible(WebContents* web_contents) {
  return false;
}

bool WebContentsDelegate::SaveFrame(const GURL& url, const Referrer& referrer) {
  return false;
}

blink::WebSecurityStyle WebContentsDelegate::GetSecurityStyle(
    WebContents* web_contents,
    SecurityStyleExplanations* security_style_explanations) {
  return blink::kWebSecurityStyleUnknown;
}

void WebContentsDelegate::RequestAppBannerFromDevTools(
    content::WebContents* web_contents) {
}

bool WebContentsDelegate::ShouldAllowRunningInsecureContent(
    WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  return allowed_per_prefs;
}

int WebContentsDelegate::GetTopControlsHeight() const {
  return 0;
}

int WebContentsDelegate::GetBottomControlsHeight() const {
  return 0;
}

bool WebContentsDelegate::DoBrowserControlsShrinkRendererSize(
    const WebContents* web_contents) const {
  return false;
}

void WebContentsDelegate::SetTopControlsGestureScrollInProgress(
    bool in_progress) {}

gfx::Size WebContentsDelegate::EnterPictureInPicture(const viz::SurfaceId&,
                                                     const gfx::Size&) {
  return gfx::Size();
}

void WebContentsDelegate::ExitPictureInPicture() {}

std::unique_ptr<content::WebContents> WebContentsDelegate::SwapWebContents(
    content::WebContents* old_contents,
    std::unique_ptr<content::WebContents> new_contents,
    bool did_start_load,
    bool did_finish_load) {
  return new_contents;
}

}  // namespace content
