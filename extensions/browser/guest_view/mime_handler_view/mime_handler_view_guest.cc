// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/mime_handler_private.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

using content::WebContents;
using guest_view::GuestViewBase;

namespace extensions {

StreamContainer::StreamContainer(
    int tab_id,
    bool embedded,
    const GURL& handler_url,
    const ExtensionId& extension_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url)
    : embedded_(embedded),
      tab_id_(tab_id),
      handler_url_(handler_url),
      extension_id_(extension_id),
      transferrable_loader_(std::move(transferrable_loader)),
      mime_type_(transferrable_loader_->head->mime_type),
      original_url_(original_url),
      stream_url_(transferrable_loader_->url),
      response_headers_(transferrable_loader_->head->headers) {}

StreamContainer::~StreamContainer() {
}

base::WeakPtr<StreamContainer> StreamContainer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

blink::mojom::TransferrableURLLoaderPtr
StreamContainer::TakeTransferrableURLLoader() {
  return std::move(transferrable_loader_);
}

// static
const char MimeHandlerViewGuest::Type[] = "mimehandler";
const guest_view::GuestViewHistogramValue MimeHandlerViewGuest::HistogramValue =
    guest_view::GuestViewHistogramValue::kMimeHandler;

// static
std::unique_ptr<GuestViewBase> MimeHandlerViewGuest::Create(
    content::RenderFrameHost* owner_rfh) {
  return base::WrapUnique(new MimeHandlerViewGuest(owner_rfh));
}

MimeHandlerViewGuest::MimeHandlerViewGuest(content::RenderFrameHost* owner_rfh)
    : GuestView<MimeHandlerViewGuest>(owner_rfh),
      delegate_(ExtensionsAPIClient::Get()->CreateMimeHandlerViewGuestDelegate(
          this)) {
  auto owner_type = owner_rfh ? owner_rfh->GetFrameOwnerElementType()
                              : blink::FrameOwnerElementType::kNone;
  // If the embedder frame is the ContentFrame() of a plugin element, then there
  // could be a MimeHandlerViewFrameContainer in the parent frame. Note that
  // the MHVFC is only created through HTMLPlugInElement::UpdatePlugin (manually
  // navigating a plugin element's window would create a MHVFC).
  maybe_has_frame_container_ =
      owner_type == blink::FrameOwnerElementType::kEmbed ||
      owner_type == blink::FrameOwnerElementType::kObject;
}

MimeHandlerViewGuest::~MimeHandlerViewGuest() {
  // Before attaching is complete, the instance ID is not valid.
  if (element_instance_id() != guest_view::kInstanceIDNone) {
    // If we are awaiting attaching to outer WebContents
    if (GetEmbedderFrame() && GetEmbedderFrame()->GetParent()) {
      // TODO(ekaramad): This should only be needed if the embedder frame is in
      // a plugin element (https://crbug.com/957373).
      mojo::AssociatedRemote<mojom::MimeHandlerViewContainerManager>
          container_manager;
      GetEmbedderFrame()
          ->GetParent()
          ->GetRemoteAssociatedInterfaces()
          ->GetInterface(&container_manager);
      container_manager->DestroyFrameContainer(element_instance_id());
    }
  }
}

bool MimeHandlerViewGuest::CanBeEmbeddedInsideCrossProcessFrames() const {
  return true;
}

void MimeHandlerViewGuest::SetBeforeUnloadController(
    mojo::PendingRemote<mime_handler::BeforeUnloadControl>
        pending_before_unload_control) {
  pending_before_unload_control_ = std::move(pending_before_unload_control);
}

const char* MimeHandlerViewGuest::GetAPINamespace() const {
  return mime_handler_view::kAPINamespace;
}

int MimeHandlerViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_MIMEHANDLERVIEW_TAG_PREFIX;
}

void MimeHandlerViewGuest::CreateWebContents(
    std::unique_ptr<GuestViewBase> owned_this,
    const base::Value::Dict& create_params,
    WebContentsCreatedCallback callback) {
  const std::string* stream_id =
      create_params.FindString(mime_handler_view::kStreamId);
  if (!stream_id || stream_id->empty()) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  stream_ = MimeHandlerStreamManager::Get(browser_context())
                ->ReleaseStream(*stream_id);
  if (!stream_) {
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }
  mime_type_ = stream_->mime_type();
  const Extension* mime_handler_extension =
      // TODO(lazyboy): Do we need handle the case where the extension is
      // terminated (ExtensionRegistry::TERMINATED)?
      ExtensionRegistry::Get(browser_context())
          ->enabled_extensions()
          .GetByID(stream_->extension_id());
  if (!mime_handler_extension) {
    LOG(ERROR) << "Extension for mime_type not found, mime_type = "
               << stream_->mime_type();
    RejectGuestCreation(std::move(owned_this), std::move(callback));
    return;
  }

  delegate_->RecordLoadMetric(
      /*is_full_page=*/!GetEmbedderFrame()->GetParentOrOuterDocument(),
      mime_type_, browser_context());

  // Compute the mime handler extension's `SiteInstance`. This must match the
  // `SiteInstance` for the navigation in `DidAttachToEmbedder()`, otherwise the
  // wrong `HostZoomMap` will be used, and the `RenderFrameHost` for the guest
  // `WebContents` will need to be swapped.
  scoped_refptr<content::SiteInstance> guest_site_instance;
#if BUILDFLAG(ENABLE_PDF)
  // TODO(crbug.com/40216386): Using `SiteInstance::CreateForURL()` creates a
  // new `BrowsingInstance`, which causes problems for features like background
  // pages. Remove one of these branches either when `ProcessManager` correctly
  // handles the multiple `StoragePartitionConfig` case, or when no
  // `MimeHandlerView` extension depends on background pages.
  if (mime_handler_extension->id() == extension_misc::kPdfExtensionId) {
    guest_site_instance = content::SiteInstance::CreateForURL(
        browser_context(), stream_->handler_url());
  } else {
#endif  // BUILDFLAG(ENABLE_PDF)
    ProcessManager* process_manager = ProcessManager::Get(browser_context());
    guest_site_instance =
        process_manager->GetSiteInstanceForURL(stream_->handler_url());
#if BUILDFLAG(ENABLE_PDF)
  }
#endif  // BUILDFLAG_ENABLE_PDF)

  // Clear the zoom level for the mime handler extension. The extension is
  // responsible for managing its own zoom. This is necessary for OOP PDF, as
  // otherwise the UI is zoomed and the calculations to determine the PDF size
  // mix zoomed and unzoomed units.
  content::HostZoomMap::Get(guest_site_instance.get())
      ->SetZoomLevelForHostAndScheme(kExtensionScheme, stream_->extension_id(),
                                     0);

  WebContents::CreateParams params(browser_context(),
                                   guest_site_instance.get());
  params.guest_delegate = this;
  std::move(callback).Run(std::move(owned_this),
                          WebContents::CreateWithSessionStorage(
                              params, owner_web_contents()
                                          ->GetController()
                                          .GetSessionStorageNamespaceMap()));
}

void MimeHandlerViewGuest::DidAttachToEmbedder() {
  DCHECK(stream_->handler_url().SchemeIs(extensions::kExtensionScheme));
  GetController().LoadURL(stream_->handler_url(), content::Referrer(),
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  web_contents()->GetMutableRendererPrefs()->can_accept_load_drops = true;
  web_contents()->SyncRendererPrefs();
}

void MimeHandlerViewGuest::DidInitialize(
    const base::Value::Dict& create_params) {
  ExtensionsAPIClient::Get()->AttachWebContentsHelpers(web_contents());
}

void MimeHandlerViewGuest::MaybeRecreateGuestContents(
    content::RenderFrameHost* outer_contents_frame) {
  // This situation is not possible for MimeHandlerView.
  NOTREACHED_IN_MIGRATION();
}

void MimeHandlerViewGuest::EmbedderFullscreenToggled(bool entered_fullscreen) {
  is_embedder_fullscreen_ = entered_fullscreen;
  if (entered_fullscreen)
    return;

  SetFullscreenState(false);
}

bool MimeHandlerViewGuest::ZoomPropagatesFromEmbedderToGuest() const {
  return false;
}

content::RenderFrameHost* MimeHandlerViewGuest::GetProspectiveOuterDocument() {
  DCHECK(!attached());
  return GetEmbedderFrame();
}

WebContents* MimeHandlerViewGuest::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* delegate = embedder_web_contents()->GetDelegate();
  return delegate
             ? delegate->OpenURLFromTab(embedder_web_contents(), params,
                                        std::move(navigation_handle_callback))
             : nullptr;
}

void MimeHandlerViewGuest::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (!(changed_flags & content::INVALIDATE_TYPE_TITLE))
    return;

  // Only consider title changes not triggered by URL changes. Otherwise, the
  // URL of the mime handler will be displayed.
  if (changed_flags & content::INVALIDATE_TYPE_URL)
    return;

  if (!is_full_page_plugin())
    return;

  content::NavigationEntry* last_committed_entry =
      embedder_web_contents()->GetController().GetLastCommittedEntry();
  if (last_committed_entry) {
    embedder_web_contents()->UpdateTitleForEntry(last_committed_entry,
                                                 source->GetTitle());
    auto* delegate = embedder_web_contents()->GetDelegate();
    if (delegate)
      delegate->NavigationStateChanged(embedder_web_contents(), changed_flags);
  }
}

bool MimeHandlerViewGuest::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(&render_frame_host));

  return delegate_ && delegate_->HandleContextMenu(render_frame_host, params);
}

bool MimeHandlerViewGuest::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  if (blink::WebInputEvent::IsPinchGestureEventType(event.GetType())) {
    // If we're an embedded plugin we drop pinch-gestures to avoid zooming the
    // guest.
    return !is_full_page_plugin();
  }
  return false;
}

content::JavaScriptDialogManager*
MimeHandlerViewGuest::GetJavaScriptDialogManager(
    WebContents* source) {
  // WebContentsDelegates often service multiple WebContentses, and use the
  // WebContents* parameter to tell which WebContents made the request. If we
  // pass in our own pointer to the delegate call, the delegate will be asked,
  // "What's the JavaScriptDialogManager of this WebContents for which you are
  // not a delegate?" And it won't be able to answer that.
  //
  // So we pretend to be our owner WebContents, but only for the request to
  // obtain the JavaScriptDialogManager. During calls to the
  // JavaScriptDialogManager we will be honest about who we are.
  auto* delegate = owner_web_contents()->GetDelegate();
  return delegate ? delegate->GetJavaScriptDialogManager(owner_web_contents())
                  : nullptr;
}

bool MimeHandlerViewGuest::PluginDoSave() {
  if (!attached() || !plugin_can_save_)
    return false;

  base::Value::List args;
  args.Append(stream_->stream_url().spec());

  auto event =
      std::make_unique<Event>(events::MIME_HANDLER_PRIVATE_SAVE,
                              api::mime_handler_private::OnSave::kEventName,
                              std::move(args), browser_context());
  EventRouter* event_router = EventRouter::Get(browser_context());
  event_router->DispatchEventToExtension(extension_misc::kPdfExtensionId,
                                         std::move(event));
  return true;
}

bool MimeHandlerViewGuest::GuestSaveFrame(
    content::WebContents* guest_web_contents) {
  MimeHandlerViewGuest* guest_view = FromWebContents(guest_web_contents);
  return guest_view == this && PluginDoSave();
}

bool MimeHandlerViewGuest::SaveFrame(
    const GURL& url,
    const content::Referrer& referrer,
    content::RenderFrameHost* render_frame_host) {
  if (!attached())
    return false;

  embedder_web_contents()->SaveFrame(stream_->original_url(), referrer,
                                     render_frame_host);
  return true;
}

void MimeHandlerViewGuest::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  if (SetFullscreenState(true)) {
    if (auto* delegate = embedder_web_contents()->GetDelegate()) {
      delegate->EnterFullscreenModeForTab(
          embedder_web_contents()->GetPrimaryMainFrame(), options);
    }
  }
}

void MimeHandlerViewGuest::ExitFullscreenModeForTab(content::WebContents*) {
  if (SetFullscreenState(false)) {
    if (auto* delegate = embedder_web_contents()->GetDelegate())
      delegate->ExitFullscreenModeForTab(embedder_web_contents());
  }
}

bool MimeHandlerViewGuest::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_guest_fullscreen_;
}

bool MimeHandlerViewGuest::ShouldResumeRequestsForCreatedWindow() {
  // Not reached due to the use of `CreateCustomWebContents`.
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool MimeHandlerViewGuest::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  return true;
}

content::WebContents* MimeHandlerViewGuest::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_renderer_initiated,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const content::StoragePartitionConfig& partition_config,
    content::SessionStorageNamespace* session_storage_namespace) {
  content::OpenURLParams open_params(target_url, content::Referrer(),
                                     WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                     ui::PAGE_TRANSITION_LINK, true);
  open_params.initiator_origin = opener->GetLastCommittedOrigin();
  open_params.source_site_instance = source_site_instance;

  // Extensions are allowed to open popups under circumstances covered by
  // running as a mime handler.
  open_params.user_gesture = true;
  auto* delegate = embedder_web_contents()->GetDelegate();
  if (delegate) {
    delegate->OpenURLFromTab(embedder_web_contents(), open_params,
                             /*navigation_handle_callback=*/{});
  }
  return nullptr;
}

bool MimeHandlerViewGuest::SetFullscreenState(bool is_fullscreen) {
  // Disallow fullscreen for embedded plugins.
  if (!is_full_page_plugin() || is_fullscreen == is_guest_fullscreen_)
    return false;

  is_guest_fullscreen_ = is_fullscreen;
  if (is_guest_fullscreen_ == is_embedder_fullscreen_)
    return false;

  is_embedder_fullscreen_ = is_fullscreen;
  return true;
}

void MimeHandlerViewGuest::DocumentOnLoadCompletedInPrimaryMainFrame() {
  DCHECK(GetEmbedderFrame());
  DCHECK_NE(element_instance_id(), guest_view::kInstanceIDNone);

  // For plugin elements, the embedder should be notified so that the queued
  // messages (postMessage) are forwarded to the guest page. Otherwise we
  // just send the update to the embedder (full page  MHV).
  auto* render_frame_host = maybe_has_frame_container_
                                ? GetEmbedderFrame()->GetParent()
                                : GetEmbedderFrame();
  mojo::AssociatedRemote<mojom::MimeHandlerViewContainerManager>
      container_manager;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &container_manager);
  container_manager->DidLoad(element_instance_id(), original_resource_url_);
}

void MimeHandlerViewGuest::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
#if BUILDFLAG(ENABLE_PDF)
  const GURL& url = navigation_handle->GetURL();
  if (url.SchemeIs(kExtensionScheme) &&
      url.host_piece() == extension_misc::kPdfExtensionId) {
    // The PDF viewer will navigate to the stream URL (using
    // PdfNavigtionThrottle), rather than using it as a subresource.
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  navigation_handle->RegisterSubresourceOverride(
      stream_->TakeTransferrableURLLoader());
}

void MimeHandlerViewGuest::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  guest_view::GuestView<MimeHandlerViewGuest>::DidFinishNavigation(
      navigation_handle);

  if (navigation_handle->IsInMainFrame()) {
    // We should not navigate the guest away from the handling extension.
    const GURL& new_url = navigation_handle->GetURL();
    CHECK(url::IsSameOriginWith(new_url, stream_->handler_url()) ||
          new_url.IsAboutBlank());

#if BUILDFLAG(ENABLE_PDF)
    if (stream_->extension_id() == extension_misc::kPdfExtensionId) {
      // Host zoom level should match the override set in `CreateWebContents()`.
      DCHECK_EQ(0, content::HostZoomMap::GetZoomLevel(web_contents()));
    }
#endif  // BUILDFLAG(ENABLE_PDF)
  }
}

void MimeHandlerViewGuest::FuseBeforeUnloadControl(
    mojo::PendingReceiver<mime_handler::BeforeUnloadControl> receiver) {
  if (!pending_before_unload_control_)
    return;

  mojo::FusePipes(std::move(receiver),
                  std::move(pending_before_unload_control_));
}

content::RenderFrameHost* MimeHandlerViewGuest::GetEmbedderFrame() {
  return owner_rfh();
}

base::WeakPtr<MimeHandlerViewGuest> MimeHandlerViewGuest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<StreamContainer> MimeHandlerViewGuest::GetStreamWeakPtr() {
  return stream_->GetWeakPtr();
}

}  // namespace extensions
