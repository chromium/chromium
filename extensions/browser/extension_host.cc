// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_host_queue.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/load_monitoring_extension_host_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

ExtensionHost::ExtensionHost(const Extension* extension,
                             SiteInstance* site_instance,
                             const GURL& url,
                             ViewType host_type)
    : delegate_(ExtensionsBrowserClient::Get()->CreateExtensionHostDelegate()),
      extension_(extension),
      extension_id_(extension->id()),
      browser_context_(site_instance->GetBrowserContext()),
      render_view_host_(nullptr),
      is_render_view_creation_pending_(false),
      has_loaded_once_(false),
      document_element_available_(false),
      initial_url_(url),
      extension_host_type_(host_type) {
  DCHECK(host_type == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE ||
         host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);
  host_contents_ = WebContents::Create(
      WebContents::CreateParams(browser_context_, site_instance)),
  content::WebContentsObserver::Observe(host_contents_.get());
  host_contents_->SetDelegate(this);
  SetViewType(host_contents_.get(), host_type);

  render_view_host_ = host_contents_->GetRenderViewHost();

  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);

  // Set up web contents observers and pref observers.
  delegate_->OnExtensionHostCreated(host_contents());

  ExtensionWebContentsObserver::GetForWebContents(host_contents())->
      dispatcher()->set_delegate(this);
}

ExtensionHost::~ExtensionHost() {
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);

  if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE &&
      extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_) &&
      load_start_.get()) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageActiveTime2",
                             load_start_->Elapsed());
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
  for (auto& observer : observer_list_)
    observer.OnExtensionHostDestroyed(this);
  for (auto& observer : deferred_start_render_host_observer_list_)
    observer.OnDeferredStartRenderHostDestroyed(this);

  // Remove ourselves from the queue as late as possible (before effectively
  // destroying self, but after everything else) so that queues that are
  // monitoring lifetime get a chance to see stop-loading events.
  delegate_->GetExtensionHostQueue()->Remove(this);

  // Deliberately stop observing |host_contents_| because its destruction
  // events (like DidStopLoading, it turns out) can call back into
  // ExtensionHost re-entrantly, when anything declared after |host_contents_|
  // has already been destroyed.
  content::WebContentsObserver::Observe(nullptr);
}

content::RenderProcessHost* ExtensionHost::render_process_host() const {
  return render_view_host()->GetProcess();
}

RenderViewHost* ExtensionHost::render_view_host() const {
  // TODO(mpcomplete): This can be null. How do we handle that?
  return render_view_host_;
}

bool ExtensionHost::IsRenderViewLive() const {
  return render_view_host()->IsRenderViewLive();
}

void ExtensionHost::CreateRenderViewSoon() {
  if (render_process_host() &&
      render_process_host()->IsInitializedAndNotDead()) {
    // If the process is already started, go ahead and initialize the RenderView
    // synchronously. The process creation is the real meaty part that we want
    // to defer.
    CreateRenderViewNow();
  } else {
    delegate_->GetExtensionHostQueue()->Add(this);
  }
}

void ExtensionHost::CreateRenderViewNow() {
  if (!ExtensionRegistry::Get(browser_context_)
           ->ready_extensions()
           .Contains(extension_->id())) {
    is_render_view_creation_pending_ = true;
    return;
  }
  is_render_view_creation_pending_ = false;
  LoadInitialURL();
  if (IsBackgroundPage()) {
    DCHECK(IsRenderViewLive());
    // Connect orphaned dev-tools instances.
    delegate_->OnRenderViewCreatedForBackgroundPage(this);
  }
}

void ExtensionHost::AddDeferredStartRenderHostObserver(
    DeferredStartRenderHostObserver* observer) {
  deferred_start_render_host_observer_list_.AddObserver(observer);
}

void ExtensionHost::RemoveDeferredStartRenderHostObserver(
    DeferredStartRenderHostObserver* observer) {
  deferred_start_render_host_observer_list_.RemoveObserver(observer);
}

void ExtensionHost::Close() {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::AddObserver(ExtensionHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionHost::RemoveObserver(ExtensionHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionHost::OnBackgroundEventDispatched(const std::string& event_name,
                                                int event_id) {
  CHECK(IsBackgroundPage());
  unacked_messages_[event_id] = event_name;
  for (auto& observer : observer_list_)
    observer.OnBackgroundEventDispatched(this, event_name, event_id);
}

void ExtensionHost::OnNetworkRequestStarted(uint64_t request_id) {
  for (auto& observer : observer_list_)
    observer.OnNetworkRequestStarted(this, request_id);
}

void ExtensionHost::OnNetworkRequestDone(uint64_t request_id) {
  for (auto& observer : observer_list_)
    observer.OnNetworkRequestDone(this, request_id);
}

const GURL& ExtensionHost::GetURL() const {
  return host_contents()->GetURL();
}

void ExtensionHost::LoadInitialURL() {
  load_start_.reset(new base::ElapsedTimer());
  host_contents_->GetController().LoadURL(
      initial_url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

bool ExtensionHost::IsBackgroundPage() const {
  DCHECK_EQ(extension_host_type_, VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  return true;
}

void ExtensionHost::OnExtensionReady(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  if (is_render_view_creation_pending_)
    CreateRenderViewNow();
}

void ExtensionHost::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // The extension object will be deleted after this notification has been sent.
  // Null it out so that dirty pointer issues don't arise in cases when multiple
  // ExtensionHost objects pointing to the same Extension are present.
  if (extension_ == extension) {
    extension_ = nullptr;
  }
}

void ExtensionHost::RenderProcessGone(base::TerminationStatus status) {
  // During browser shutdown, we may use sudden termination on an extension
  // process, so it is expected to lose our connection to the render view.
  // Do nothing.
  RenderProcessHost* process_host =
      host_contents_->GetMainFrame()->GetProcess();
  if (process_host && process_host->FastShutdownStarted())
    return;

  // In certain cases, multiple ExtensionHost objects may have pointed to
  // the same Extension at some point (one with a background page and a
  // popup, for example). When the first ExtensionHost goes away, the extension
  // is unloaded, and any other host that pointed to that extension will have
  // its pointer to it null'd out so that any attempt to unload a dirty pointer
  // will be averted.
  if (!extension_)
    return;

  // TODO(aa): This is suspicious. There can be multiple views in an extension,
  // and they aren't all going to use ExtensionHost. This should be in someplace
  // more central, like EPM maybe.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::DidStartLoading() {
  if (!has_loaded_once_) {
    for (auto& observer : deferred_start_render_host_observer_list_)
      observer.OnDeferredStartRenderHostDidStartFirstLoad(this);
  }
}

void ExtensionHost::DidStopLoading() {
  // Only record UMA for the first load. Subsequent loads will likely behave
  // quite different, and it's first load we're most interested in.
  bool first_load = !has_loaded_once_;
  has_loaded_once_ = true;
  if (first_load) {
    RecordStopLoadingUMA();
    OnDidStopFirstLoad();
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
        content::Source<BrowserContext>(browser_context_),
        content::Details<ExtensionHost>(this));
    for (auto& observer : deferred_start_render_host_observer_list_)
      observer.OnDeferredStartRenderHostDidStopFirstLoad(this);
  }
}

void ExtensionHost::OnDidStopFirstLoad() {
  DCHECK_EQ(extension_host_type_, VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  // Nothing to do for background pages.
}

void ExtensionHost::DocumentAvailableInMainFrame() {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;
  document_element_available_ = true;

  if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    ExtensionSystem::Get(browser_context_)
        ->runtime_data()
        ->SetBackgroundPageReady(extension_->id(), true);
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::Source<const Extension>(extension_),
        content::NotificationService::NoDetails());
  }
}

void ExtensionHost::CloseContents(WebContents* contents) {
  Close();
}

bool ExtensionHost::OnMessageReceived(const IPC::Message& message,
                                      content::RenderFrameHost* host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHost, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_EventAck, OnEventAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_IncrementLazyKeepaliveCount,
                        OnIncrementLazyKeepaliveCount)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DecrementLazyKeepaliveCount,
                        OnDecrementLazyKeepaliveCount)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHost::OnEventAck(int event_id) {
  // This should always be true since event acks are only sent by extensions
  // with lazy background pages but it doesn't hurt to be extra careful.
  const bool is_background_page = IsBackgroundPage();
  // A compromised renderer could start sending out arbitrary event ids, which
  // may affect other renderers by causing downstream methods to think that
  // events for other extensions have been acked.  Make sure that the event id
  // sent by the renderer is one that this ExtensionHost expects to receive.
  // This way if a renderer _is_ compromised, it can really only affect itself.
  if (!is_background_page) {
    // Kill this renderer.
    DCHECK(render_process_host());
    LOG(ERROR) << "Killing renderer for extension " << extension_id()
               << " for sending an EventAck without a lazy background page.";
    bad_message::ReceivedBadMessage(render_process_host(),
                                    bad_message::EH_BAD_EVENT_ID);
    return;
  }

  const auto it = unacked_messages_.find(event_id);
  if (it == unacked_messages_.end()) {
    // Ideally, we'd be able to kill the renderer in the case of it sending an
    // ack for an event that we haven't seen. However, https://crbug.com/939279
    // demonstrates that there are cases in which this can happen in other
    // situations. We should track those down and fix them, but for now
    // log and gracefully exit.
    // bad_message::ReceivedBadMessage(render_process_host(),
    //                                 bad_message::EH_BAD_EVENT_ID);
    LOG(ERROR) << "Received EventAck for extension " << extension_id()
               << " for an unknown event.";
    return;
  }

  EventRouter* router = EventRouter::Get(browser_context_);
  if (router)
    router->OnEventAck(browser_context_, extension_id(), it->second);

  for (auto& observer : observer_list_)
    observer.OnBackgroundEventAcked(this, event_id);

  // Remove it.
  unacked_messages_.erase(it);
}

void ExtensionHost::OnIncrementLazyKeepaliveCount() {
  ProcessManager::Get(browser_context_)
      ->IncrementLazyKeepaliveCount(extension(), Activity::LIFECYCLE_MANAGEMENT,
                                    Activity::kIPC);
}

void ExtensionHost::OnDecrementLazyKeepaliveCount() {
  ProcessManager::Get(browser_context_)
      ->DecrementLazyKeepaliveCount(extension(), Activity::LIFECYCLE_MANAGEMENT,
                                    Activity::kIPC);
}

// content::WebContentsObserver

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;
}

void ExtensionHost::RenderViewDeleted(RenderViewHost* render_view_host) {
  // If our RenderViewHost is deleted, fall back to the host_contents' current
  // RVH. There is sometimes a small gap between the pending RVH being deleted
  // and RenderViewCreated being called, so we update it here.
  if (render_view_host == render_view_host_)
    render_view_host_ = host_contents_->GetRenderViewHost();
}

content::JavaScriptDialogManager* ExtensionHost::GetJavaScriptDialogManager(
    WebContents* source) {
  return delegate_->GetJavaScriptDialogManager();
}

void ExtensionHost::AddNewContents(WebContents* source,
                                   std::unique_ptr<WebContents> new_contents,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_rect,
                                   bool user_gesture,
                                   bool* was_blocked) {
  // First, if the creating extension view was associated with a tab contents,
  // use that tab content's delegate. We must be careful here that the
  // associated tab contents has the same profile as the new tab contents. In
  // the case of extensions in 'spanning' incognito mode, they can mismatch.
  // We don't want to end up putting a normal tab into an incognito window, or
  // vice versa.
  // Note that we don't do this for popup windows, because we need to associate
  // those with their extension_app_id.
  if (disposition != WindowOpenDisposition::NEW_POPUP) {
    WebContents* associated_contents = GetAssociatedWebContents();
    if (associated_contents &&
        associated_contents->GetBrowserContext() ==
            new_contents->GetBrowserContext()) {
      WebContentsDelegate* delegate = associated_contents->GetDelegate();
      if (delegate) {
        delegate->AddNewContents(associated_contents, std::move(new_contents),
                                 disposition, initial_rect, user_gesture,
                                 was_blocked);
        return;
      }
    }
  }

  delegate_->CreateTab(std::move(new_contents), extension_id_, disposition,
                       initial_rect, user_gesture);
}

void ExtensionHost::RenderViewReady() {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_HOST_CREATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  delegate_->ProcessMediaAccessRequest(web_contents, request,
                                       std::move(callback), extension());
}

bool ExtensionHost::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return delegate_->CheckMediaAccessPermission(
      render_frame_host, security_origin, type, extension());
}

bool ExtensionHost::IsNeverVisible(content::WebContents* web_contents) {
  ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

content::PictureInPictureResult ExtensionHost::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  return delegate_->EnterPictureInPicture(web_contents, surface_id,
                                          natural_size);
}

void ExtensionHost::ExitPictureInPicture() {
  delegate_->ExitPictureInPicture();
}

void ExtensionHost::RecordStopLoadingUMA() {
  CHECK(load_start_.get());
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    if (extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_)) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.EventPageLoadTime2",
                                 load_start_->Elapsed());
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.BackgroundPageLoadTime2",
                                 load_start_->Elapsed());
    }
  } else if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP) {
    UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.PopupLoadTime2",
                               load_start_->Elapsed());
    UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.PopupCreateTime",
                               create_start_.Elapsed());
  }
}

}  // namespace extensions
