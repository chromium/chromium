// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_host_queue.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"

using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

namespace {

// NoOpColorProviderSource does not generate any color provider changes for
// the UI-less extension background page.
class NoOpColorProviderSource : public ui::ColorProviderSource {
 public:
  NoOpColorProviderSource() = default;
  NoOpColorProviderSource(const NoOpColorProviderSource&) = delete;
  NoOpColorProviderSource& operator=(const NoOpColorProviderSource&) = delete;
  ~NoOpColorProviderSource() override = default;

  static NoOpColorProviderSource* Get() {
    static base::NoDestructor<NoOpColorProviderSource> instance;
    return instance.get();
  }

 private:
  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    return ui::CreateRendererColorMap(color_provider_);
  }

  ui::ColorProviderKey GetColorProviderKey() const override {
    return ui::ColorProviderKey();
  }

  ui::ColorProvider color_provider_;
};

// Emit all event dispatch time related metrics.
void EmitDispatchTimeMetrics(const EventDispatchSource& dispatch_source,
                             base::TimeTicks dispatch_start_time,
                             bool lazy_background_active_on_dispatch,
                             const Extension* extension) {
  // Only emit events that use the EventRouter::DispatchEventToProcess() event
  // routing flow since EventRouter::DispatchEventToSender() uses a different
  // flow that doesn't include dispatch start and service worker start time.
  if (dispatch_source != EventDispatchSource::kDispatchEventToProcess) {
    return;
  }

  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Extensions.Events.DispatchToAckTime.ExtensionEventPage3",
        /*sample=*/base::TimeTicks::Now() - dispatch_start_time,
        /*min=*/base::Microseconds(1), /*max=*/base::Minutes(5),
        /*buckets=*/100);
    const char* active_metric_name =
        lazy_background_active_on_dispatch
            ? "Extensions.Events.DispatchToAckTime.ExtensionEventPage3."
              "Active"
            : "Extensions.Events.DispatchToAckTime.ExtensionEventPage3."
              "Inactive";
    base::UmaHistogramCustomMicrosecondsTimes(
        active_metric_name,
        /*sample=*/base::TimeTicks::Now() - dispatch_start_time,
        /*min=*/base::Microseconds(1), /*max=*/base::Minutes(5),
        /*buckets=*/100);
  } else if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Extensions.Events.DispatchToAckTime."
        "ExtensionPersistentBackgroundPage",
        /*sample=*/base::TimeTicks::Now() - dispatch_start_time,
        /*min=*/base::Microseconds(1), /*max=*/base::Minutes(5),
        /*buckets=*/100);
  }
}

}  // namespace

ExtensionHost::ExtensionHost(const Extension* extension,
                             SiteInstance* site_instance,
                             const GURL& url,
                             mojom::ViewType host_type)
    : delegate_(ExtensionsBrowserClient::Get()->CreateExtensionHostDelegate()),
      extension_(extension),
      extension_id_(extension->id()),
      browser_context_(site_instance->GetBrowserContext()),
      initial_url_(url),
      extension_host_type_(host_type) {
  DCHECK(host_type == mojom::ViewType::kExtensionBackgroundPage ||
         host_type == mojom::ViewType::kOffscreenDocument ||
         host_type == mojom::ViewType::kExtensionPopup ||
         host_type == mojom::ViewType::kExtensionSidePanel);
  host_contents_ = WebContents::Create(
      WebContents::CreateParams(browser_context_, site_instance));
  host_contents_->SetOwnerLocationForDebug(FROM_HERE);
  content::WebContentsObserver::Observe(host_contents_.get());
  host_contents_->SetDelegate(this);
  SetViewType(host_contents_.get(), host_type);
  main_frame_host_ = host_contents_->GetPrimaryMainFrame();

  if (host_type == mojom::ViewType::kExtensionBackgroundPage) {
    host_contents_->SetColorProviderSource(NoOpColorProviderSource::Get());
  }

  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  ExtensionRegistry::Get(browser_context_)->AddObserver(this);

  // Set up web contents observers and pref observers.
  delegate_->OnExtensionHostCreated(host_contents());

  ExtensionWebContentsObserver::GetForWebContents(host_contents())->
      dispatcher()->set_delegate(this);

  // Create password reuse detection manager when new extension web contents are
  // created.
  ExtensionsBrowserClient::Get()->CreatePasswordReuseDetectionManager(
      host_contents_.get());

  ExtensionHostRegistry::Get(browser_context_)->ExtensionHostCreated(this);
}

ExtensionHost::~ExtensionHost() {
  ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);

  if (extension_host_type_ == mojom::ViewType::kExtensionBackgroundPage &&
      extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_) &&
      load_start_.get()) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageActiveTime2",
                             load_start_->Elapsed());
  }

  for (auto& observer : observer_list_)
    observer.OnExtensionHostDestroyed(this);

  ExtensionHostRegistry::Get(browser_context_)->ExtensionHostDestroyed(this);

  // Remove ourselves from the queue as late as possible (before effectively
  // destroying self, but after everything else) so that queues that are
  // monitoring lifetime get a chance to see stop-loading events.
  ExtensionHostQueue::GetInstance().Remove(this);

  // Deliberately stop observing |host_contents_| because its destruction
  // events (like DidStopLoading, it turns out) can call back into
  // ExtensionHost re-entrantly, when anything declared after |host_contents_|
  // has already been destroyed.
  content::WebContentsObserver::Observe(nullptr);
}

content::RenderProcessHost* ExtensionHost::render_process_host() const {
  return main_frame_host_->GetProcess();
}

bool ExtensionHost::IsRendererLive() const {
  return main_frame_host_->IsRenderFrameLive();
}

void ExtensionHost::CreateRendererSoon() {
  if (render_process_host() &&
      render_process_host()->IsInitializedAndNotDead()) {
    // If the process is already started, go ahead and initialize the renderer
    // frame synchronously. The process creation is the real meaty part that we
    // want to defer.
    CreateRendererNow();
  } else {
    ExtensionHostQueue::GetInstance().Add(this);
  }
}

void ExtensionHost::CreateRendererNow() {
  if (!ExtensionRegistry::Get(browser_context_)
           ->ready_extensions()
           .Contains(extension_->id())) {
    is_renderer_creation_pending_ = true;
    return;
  }
  is_renderer_creation_pending_ = false;
  LoadInitialURL();
  if (IsBackgroundPage()) {
    DCHECK(IsRendererLive());
    // Connect orphaned dev-tools instances.
    delegate_->OnMainFrameCreatedForBackgroundPage(this);
  }
}

void ExtensionHost::Close() {
  // Some ways of closing the host may be asynchronous, which would allow the
  // contents to call Close() multiple times. If we've already called the
  // handler once, ignore subsequent calls. If we haven't called the handler
  // once, the handler should be present.
  DCHECK(close_handler_ || called_close_handler_);
  if (called_close_handler_)
    return;

  called_close_handler_ = true;
  std::move(close_handler_).Run(this);
  // NOTE: `this` may be deleted at this point!
}

void ExtensionHost::AddObserver(ExtensionHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionHost::RemoveObserver(ExtensionHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionHost::OnBackgroundEventDispatched(
    const std::string& event_name,
    base::TimeTicks dispatch_start_time,
    int event_id,
    EventDispatchSource dispatch_source,
    bool lazy_background_active_on_dispatch) {
  // TODO(crbug.com/40909770): Make IsBackgroundPage() a real CHECK. It's
  // effectively a DCHECK right now.
  CHECK(IsBackgroundPage());
  CHECK(BackgroundInfo::HasBackgroundPage(extension()));
  // See ExtensionHost::OnEventAck() for an explanation on the restriction to
  // this event flow.
  if (dispatch_source == EventDispatchSource::kDispatchEventToProcess) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionHost::EmitLateAckedEventTask,
                       weak_ptr_factory_.GetWeakPtr(), event_id),
        kEventAckMetricTimeLimit);
  }

  // We don't expect unacked_messages to be written more than once per event_id
  // since event_ids are supposed to be unique per event dispatch to each
  // extension.
  CHECK(unacked_messages_.count(event_id) == 0);
  unacked_messages_[event_id] =
      UnackedEventData{event_name, dispatch_start_time, dispatch_source,
                       lazy_background_active_on_dispatch};
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

void ExtensionHost::SetCloseHandler(CloseHandler close_handler) {
  DCHECK(!close_handler_);
  DCHECK(!called_close_handler_);
  close_handler_ = std::move(close_handler);
}

bool ExtensionHost::ShouldAllowNavigations() const {
  // Don't allow background pages or offscreen documents to navigate.
  return extension_host_type_ != mojom::ViewType::kExtensionBackgroundPage &&
         extension_host_type_ != mojom::ViewType::kOffscreenDocument;
}

const GURL& ExtensionHost::GetLastCommittedURL() const {
  return host_contents()->GetLastCommittedURL();
}

void ExtensionHost::LoadInitialURL() {
  load_start_ = std::make_unique<base::ElapsedTimer>();
  host_contents_->GetController().LoadURL(
      initial_url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

bool ExtensionHost::IsBackgroundPage() const {
  DCHECK_EQ(extension_host_type_, mojom::ViewType::kExtensionBackgroundPage);
  return true;
}

void ExtensionHost::OnExtensionReady(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  if (is_renderer_creation_pending_)
    CreateRendererNow();
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

void ExtensionHost::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // During browser shutdown, we may use sudden termination on an extension
  // process, so it is expected to lose our connection to the render view.
  // Do nothing.
  RenderProcessHost* process_host =
      host_contents_->GetPrimaryMainFrame()->GetProcess();
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
  ExtensionHostRegistry::Get(browser_context_)
      ->ExtensionHostRenderProcessGone(this);

  ProcessManager::Get(browser_context_)
      ->NotifyExtensionProcessTerminated(extension_);
}

void ExtensionHost::DidStopLoading() {
  // Only record UMA for the first load. Subsequent loads will likely behave
  // quite different, and it's first load we're most interested in.
  bool first_load = !has_loaded_once_;
  has_loaded_once_ = true;
  if (first_load) {
    RecordStopLoadingUMA();
    OnDidStopFirstLoad();
    ExtensionHostRegistry::Get(browser_context_)
        ->ExtensionHostCompletedFirstLoad(this);
    for (auto& observer : observer_list_)
      observer.OnExtensionHostDidStopFirstLoad(this);
  }
}

void ExtensionHost::OnDidStopFirstLoad() {
  DCHECK_EQ(extension_host_type_, mojom::ViewType::kExtensionBackgroundPage);
  // Nothing to do for background pages.
}

void ExtensionHost::PrimaryMainDocumentElementAvailable() {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;
  document_element_available_ = true;

  ExtensionHostRegistry::Get(browser_context_)
      ->ExtensionHostDocumentElementAvailable(this);
}

void ExtensionHost::CloseContents(WebContents* contents) {
  Close();
}

void ExtensionHost::EmitLateAckedEventTask(int event_id) {
  // If the event is still present then we haven't received the ack yet in
  // `ExtensionHost::OnEventAck()`.
  if (unacked_messages_.contains(event_id)) {
    const char* metric_name =
        BackgroundInfo::HasLazyBackgroundPage(extension())
            ? "Extensions.Events.DidDispatchToAckSucceed.ExtensionPage"
            : "Extensions.Events.DidDispatchToAckSucceed."
              "ExtensionPersistentPage";
    // TODO(crbug.com/40277737): Update this histogram once we have a way to
    // ack only for lazy background page events. Until then this could be
    // slightly inaccurate and not perfectly comparable to the service worker
    // version.
    base::UmaHistogramBoolean(metric_name, false);
  }
}

void ExtensionHost::OnEventAck(int event_id,
                               bool event_has_listener_in_background_context) {
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
               << " for sending an EventAck without a background page (lazy or "
                  "non-lazy).";
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

  const UnackedEventData& unacked_message_data = it->second;

  // From the browser side, we add an in-flight event (`unacked_messages_`) for
  // every event that goes to an extension process for an extension with a
  // (lazy or non-lazy; event page or persistent) background page, whether or
  // not the event is going to be received by the background page. On the
  // browser side, we can't easily determine if an event will be handled in the
  // background page. Instead, here we rely on a signal from the renderer that
  // the event ran in the background page and only emit background-related
  // metrics if that's the case.
  // TODO(crbug.com/40277737): Remove this condition once crbug.com/1470045
  // allows us to only ack for lazy background page events.
  if (event_has_listener_in_background_context) {
    EmitDispatchTimeMetrics(
        unacked_message_data.dispatch_source,
        unacked_message_data.dispatch_start_time,
        unacked_message_data.lazy_background_active_on_dispatch, extension());
  }

  // `DidDispatchToAckSucceed` is outside of the
  // `event_has_listener_in_background_context` condition because we
  // can't exclude non-script extensions page contexts (e.g. popup scripts) yet
  // so we have to emit this for all events to remain proportionate to the
  // `false` emits.
  bool late_ack =
      (base::TimeTicks::Now() - unacked_message_data.dispatch_start_time) >
      kEventAckMetricTimeLimit;
  if (!late_ack) {
    // Emit only if we're within the expected event ack time limit. We'll take
    // care of the emit for a late ack via a delayed task we started on event
    // dispatch.
    if (BackgroundInfo::HasLazyBackgroundPage(extension())) {
      base::UmaHistogramBoolean(
          "Extensions.Events.DidDispatchToAckSucceed.ExtensionPage", true);
    } else if (BackgroundInfo::HasPersistentBackgroundPage(extension())) {
      base::UmaHistogramBoolean(
          "Extensions.Events.DidDispatchToAckSucceed.ExtensionPersistentPage",
          true);
    }
  }

  EventRouter* router = EventRouter::Get(browser_context_);
  if (router)
    router->OnEventAck(browser_context_, extension_id(),
                       unacked_message_data.event_name);

  for (auto& observer : observer_list_)
    observer.OnBackgroundEventAcked(this, event_id);

  // Remove it.
  unacked_messages_.erase(it);
}

// content::WebContentsObserver

content::JavaScriptDialogManager* ExtensionHost::GetJavaScriptDialogManager(
    WebContents* source) {
  return delegate_->GetJavaScriptDialogManager();
}

content::WebContents* ExtensionHost::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
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
                                 target_url, disposition, window_features,
                                 user_gesture, was_blocked);
        return nullptr;
      }
    }
  }

  delegate_->CreateTab(std::move(new_contents), extension_id_, disposition,
                       window_features, user_gesture);

  return nullptr;
}

void ExtensionHost::RenderFrameCreated(content::RenderFrameHost* frame_host) {
  // Only consider the main frame. Ignore all other frames, including
  // speculative main frames (which might replace the main frame, but that
  // scenario is handled in `RenderFrameHostChanged`).
  if (frame_host != main_frame_host_)
    return;

  MaybeNotifyRenderProcessReady();
}

void ExtensionHost::RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                           content::RenderFrameHost* new_host) {
  // Only the primary main frame is tracked, so ignore any other frames.
  if (old_host != main_frame_host_)
    return;

  main_frame_host_ = new_host;

  // The RenderFrame already exists when this callback is fired. Try to notify
  // again in case we missed the `RenderFrameCreated` callback (e.g. when the
  // ExtensionHost is attached after the main frame started a navigation).
  MaybeNotifyRenderProcessReady();
}

void ExtensionHost::MaybeNotifyRenderProcessReady() {
  if (!has_creation_notification_already_fired_) {
    has_creation_notification_already_fired_ = true;

    // When the first renderer comes alive, we wait for the process to complete
    // its initialization then notify observers.
    render_process_host()->PostTaskWhenProcessIsReady(
        base::BindOnce(&ExtensionHost::NotifyRenderProcessReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ExtensionHost::NotifyRenderProcessReady() {
  ExtensionHostRegistry::Get(browser_context_)
      ->ExtensionHostRenderProcessReady(this);
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
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return delegate_->CheckMediaAccessPermission(
      render_frame_host, security_origin, type, extension());
}

bool ExtensionHost::IsNeverComposited(content::WebContents* web_contents) {
  mojom::ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == mojom::ViewType::kExtensionBackgroundPage ||
         view_type == mojom::ViewType::kOffscreenDocument;
}

content::PictureInPictureResult ExtensionHost::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return delegate_->EnterPictureInPicture(web_contents);
}

void ExtensionHost::ExitPictureInPicture() {
  delegate_->ExitPictureInPicture();
}

std::string ExtensionHost::GetTitleForMediaControls(
    content::WebContents* web_contents) {
  return extension() ? extension()->name() : std::string();
}

void ExtensionHost::RecordStopLoadingUMA() {
  CHECK(load_start_.get());
  if (extension_host_type_ == mojom::ViewType::kExtensionBackgroundPage) {
    if (extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_)) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.EventPageLoadTime2",
                                 load_start_->Elapsed());
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("Extensions.BackgroundPageLoadTime2",
                                 load_start_->Elapsed());
    }
  }
}

}  // namespace extensions
