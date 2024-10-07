// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "content/public/browser/audio_stream_broker.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() = default;

WebContents* WebContentsDelegate::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params,
    base::OnceCallback<void(NavigationHandle&)> navigation_handle_callback) {
  return nullptr;
}

bool WebContentsDelegate::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_outermost_main_frame_navigation) {
  return true;
}

WebContents* WebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  return nullptr;
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
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
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

bool WebContentsDelegate::ShouldFocusPageAfterCrash(WebContents* source) {
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

bool WebContentsDelegate::HandleContextMenu(RenderFrameHost& render_frame_host,
                                            const ContextMenuParams& params) {
  return false;
}

KeyboardEventProcessingResult WebContentsDelegate::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebContentsDelegate::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
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
    mojom::WindowContainerType window_container_type,
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
    const StoragePartitionConfig& partition_config,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
}

JavaScriptDialogManager* WebContentsDelegate::GetJavaScriptDialogManager(
    WebContents* source) {
  return nullptr;
}

void WebContentsDelegate::CreateSmsPrompt(
    RenderFrameHost* host,
    const std::vector<url::Origin>& origin_list,
    const std::string& one_time_code,
    base::OnceCallback<void()> on_confirm,
    base::OnceCallback<void()> on_cancel) {}

bool WebContentsDelegate::CanUseWindowingControls(
    RenderFrameHost* requesting_frame) {
  return false;
}

bool WebContentsDelegate::GetCanResize() {
  return false;
}

ui::mojom::WindowShowState WebContentsDelegate::GetWindowShowState() const {
  return ui::mojom::WindowShowState::kDefault;
}

bool WebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  return false;
}

FullscreenState WebContentsDelegate::GetFullscreenState(
    const WebContents* web_contents) const {
  FullscreenState state;
  state.target_mode =
      const_cast<WebContentsDelegate*>(this)->IsFullscreenForTabOrPending(
          web_contents)
          ? FullscreenMode::kContent
          : FullscreenMode::kWindowed;
  return state;
}

bool WebContentsDelegate::CanEnterFullscreenModeForTab(
    RenderFrameHost* requesting_frame) {
  return true;
}

blink::mojom::DisplayMode WebContentsDelegate::GetDisplayMode(
    const WebContents* web_contents) {
  return blink::mojom::DisplayMode::kBrowser;
}

blink::ProtocolHandlerSecurityLevel
WebContentsDelegate::GetProtocolHandlerSecurityLevel(RenderFrameHost*) {
  return blink::ProtocolHandlerSecurityLevel::kStrict;
}

void WebContentsDelegate::RequestPointerLock(WebContents* web_contents,
                                             bool user_gesture,
                                             bool last_unlocked_by_target) {
  web_contents->GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult::kUnknownError);
}

void WebContentsDelegate::RequestKeyboardLock(WebContents* web_contents,
                                              bool esc_key_locked) {
  // Notify `web_contents` that the request is accepted and the JavaScript
  // promise for the request can be resolved. This can be overridden by a
  // subclass that wants to conditionally accept the request, e.g., depending
  // on the permissions state.
  web_contents->GotResponseToKeyboardLockRequest(true);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
std::unique_ptr<ColorChooser> WebContentsDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return nullptr;
}
#endif

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

#if BUILDFLAG(IS_ANDROID)
bool WebContentsDelegate::UseFileChooserForFileSystemAccess() const {
  return false;
}
#endif

void WebContentsDelegate::EnumerateDirectory(
    WebContents* web_contents,
    scoped_refptr<FileSelectListener> listener,
    const base::FilePath& path) {
  listener->FileSelectionCanceled();
}

void WebContentsDelegate::RequestMediaAccessPermission(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    MediaResponseCallback callback) {
  LOG(ERROR) << "WebContentsDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          std::unique_ptr<MediaStreamUI>());
}

bool WebContentsDelegate::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  LOG(ERROR) << "WebContentsDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

std::string WebContentsDelegate::GetTitleForMediaControls(
    WebContents* web_contents) {
  return {};
}

std::unique_ptr<AudioStreamBrokerFactory>
WebContentsDelegate::CreateAudioStreamBrokerFactory(WebContents* web_contents) {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
bool WebContentsDelegate::ShouldBlockMediaRequest(const GURL& url) {
  return false;
}
#endif

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    WebContents* web_contents = *attached_contents_.begin();
    web_contents->SetDelegate(nullptr);
  }
  DCHECK(attached_contents_.empty());
}

void WebContentsDelegate::Attach(WebContents* web_contents) {
  DCHECK(!base::Contains(attached_contents_, web_contents));
  attached_contents_.insert(web_contents);
}

void WebContentsDelegate::Detach(WebContents* web_contents) {
  DCHECK(base::Contains(attached_contents_, web_contents));
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

bool WebContentsDelegate::SaveFrame(const GURL& url,
                                    const Referrer& referrer,
                                    RenderFrameHost* rfh) {
  return false;
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

int WebContentsDelegate::GetVirtualKeyboardHeight(WebContents* web_contents) {
  return 0;
}

bool WebContentsDelegate::OnlyExpandTopControlsAtPageTop() {
  return false;
}

PictureInPictureResult WebContentsDelegate::EnterPictureInPicture(
    WebContents* web_contents) {
  return PictureInPictureResult::kNotSupported;
}

bool WebContentsDelegate::ShouldAllowLazyLoad() {
  return true;
}

bool WebContentsDelegate::IsBackForwardCacheSupported(
    WebContents& web_contents) {
  return false;
}

PreloadingEligibility WebContentsDelegate::IsPrerender2Supported(
    WebContents& web_contents) {
  return PreloadingEligibility::kPreloadingUnsupportedByWebContents;
}

NavigationController::UserAgentOverrideOption
WebContentsDelegate::ShouldOverrideUserAgentForPrerender2() {
  return NavigationController::UA_OVERRIDE_INHERIT;
}

bool WebContentsDelegate::ShouldAllowPartialParamMismatchOfPrerender2(
    NavigationHandle& navigation_handle) {
  return false;
}

bool WebContentsDelegate::ShouldShowStaleContentOnEviction(
    WebContents* source) {
  return false;
}

device::mojom::GeolocationContext*
WebContentsDelegate::GetInstalledWebappGeolocationContext() {
  return nullptr;
}

bool WebContentsDelegate::IsPrivileged() {
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
bool WebContentsDelegate::ShouldUseInstancedSystemMediaControls() const {
  return false;
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool WebContentsDelegate::MaybeCopyContentAreaAsBitmap(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  return false;
}

bool WebContentsDelegate::IsWaitingForPointerLockPrompt(
    WebContents* web_contents) {
  return false;
}

#if BUILDFLAG(IS_ANDROID)
SkBitmap WebContentsDelegate::MaybeCopyContentAreaAsBitmapSync() {
  return SkBitmap();
}

BackForwardTransitionAnimationManager::FallbackUXConfig
WebContentsDelegate::GetBackForwardTransitionFallbackUXConfig() {
  return BackForwardTransitionAnimationManager::FallbackUXConfig();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
