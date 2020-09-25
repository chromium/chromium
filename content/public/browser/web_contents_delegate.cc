// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include <memory>
#include <utility>

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
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() = default;

WebContents* WebContentsDelegate::OpenURLFromTab(WebContents* source,
                                                 const OpenURLParams& params) {
  return nullptr;
}

bool WebContentsDelegate::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  return true;
}

bool WebContentsDelegate::CanOverscrollContent() {
  return false;
}

bool WebContentsDelegate::ShouldSuppressDialogs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::ShouldPreserveAbortedURLs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
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

void WebContentsDelegate::CanDownload(const GURL& url,
                                      const std::string& request_method,
                                      base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool WebContentsDelegate::HandleContextMenu(RenderFrameHost* render_frame_host,
                                            const ContextMenuParams& params) {
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
    blink::DragOperationsMask operations_allowed) {
  return true;
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::IsWebContentsCreationOverridden(
    SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return false;
}

WebContents* WebContentsDelegate::CreateCustomWebContents(
    RenderFrameHost* opener,
    SiteInstance* source_site_instance,
    bool is_new_browsing_instance,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
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

void WebContentsDelegate::CreateSmsPrompt(
    RenderFrameHost* host,
    const url::Origin& origin,
    const std::string& one_time_code,
    base::OnceCallback<void()> on_confirm,
    base::OnceCallback<void()> on_cancel) {}

std::unique_ptr<BluetoothScanningPrompt>
WebContentsDelegate::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  return nullptr;
}

bool WebContentsDelegate::EmbedsFullscreenWidget() {
  return false;
}

bool WebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  return false;
}

blink::mojom::DisplayMode WebContentsDelegate::GetDisplayMode(
    const WebContents* web_contents) {
  return blink::mojom::DisplayMode::kBrowser;
}

ColorChooser* WebContentsDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return nullptr;
}

std::unique_ptr<EyeDropper> WebContentsDelegate::OpenEyeDropper(
    RenderFrameHost* frame,
    EyeDropperListener* listener) {
  return nullptr;
}

void WebContentsDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
}

void WebContentsDelegate::EnumerateDirectory(
    WebContents* web_contents,
    scoped_refptr<FileSelectListener> listener,
    const base::FilePath& path) {
  listener->FileSelectionCanceled();
}

void WebContentsDelegate::RequestMediaAccessPermission(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  LOG(ERROR) << "WebContentsDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  std::move(callback).Run(blink::MediaStreamDevices(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool WebContentsDelegate::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  LOG(ERROR) << "WebContentsDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string WebContentsDelegate::GetDefaultMediaDeviceID(
    WebContents* web_contents,
    blink::mojom::MediaStreamType type) {
  return std::string();
}

#if defined(OS_ANDROID)
bool WebContentsDelegate::ShouldBlockMediaRequest(const GURL& url) {
  return false;
}
#endif

void WebContentsDelegate::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
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
    WebContents* web_contents) {
  return gfx::Size();
}

bool WebContentsDelegate::IsNeverComposited(WebContents* web_contents) {
  return false;
}

bool WebContentsDelegate::GuestSaveFrame(WebContents* guest_web_contents) {
  return false;
}

bool WebContentsDelegate::SaveFrame(const GURL& url, const Referrer& referrer) {
  return false;
}

blink::SecurityStyle WebContentsDelegate::GetSecurityStyle(
    WebContents* web_contents,
    SecurityStyleExplanations* security_style_explanations) {
  return blink::SecurityStyle::kUnknown;
}

bool WebContentsDelegate::ShouldAllowRunningInsecureContent(
    WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  return allowed_per_prefs;
}

int WebContentsDelegate::GetTopControlsHeight() {
  return 0;
}

int WebContentsDelegate::GetTopControlsMinHeight() {
  return 0;
}

int WebContentsDelegate::GetBottomControlsHeight() {
  return 0;
}

int WebContentsDelegate::GetBottomControlsMinHeight() {
  return 0;
}

bool WebContentsDelegate::ShouldAnimateBrowserControlsHeightChanges() {
  return false;
}

bool WebContentsDelegate::DoBrowserControlsShrinkRendererSize(
    WebContents* web_contents) {
  return false;
}

bool WebContentsDelegate::OnlyExpandTopControlsAtPageTop() {
  return false;
}

PictureInPictureResult WebContentsDelegate::EnterPictureInPicture(
    WebContents* web_contents,
    const viz::SurfaceId&,
    const gfx::Size&) {
  return PictureInPictureResult::kNotSupported;
}

bool WebContentsDelegate::ShouldAllowLazyLoad() {
  return true;
}

std::unique_ptr<WebContents> WebContentsDelegate::ActivatePortalWebContents(
    WebContents* predecessor_contents,
    std::unique_ptr<WebContents> portal_contents) {
  return portal_contents;
}

void WebContentsDelegate::UpdateInspectedWebContentsIfNecessary(
    WebContents* old_contents,
    WebContents* new_contents,
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

bool WebContentsDelegate::ShouldShowStaleContentOnEviction(
    WebContents* source) {
  return false;
}

bool WebContentsDelegate::IsFrameLowPriority(
    const WebContents* web_contents,
    const RenderFrameHost* render_frame_host) {
  return false;
}

WebContents* WebContentsDelegate::GetResponsibleWebContents(
    WebContents* web_contents) {
  return web_contents;
}

device::mojom::GeolocationContext*
WebContentsDelegate::GetInstalledWebappGeolocationContext() {
  return nullptr;
}

base::WeakPtr<WebContentsDelegate> WebContentsDelegate::GetDelegateWeakPtr() {
  return nullptr;
}

}  // namespace content
