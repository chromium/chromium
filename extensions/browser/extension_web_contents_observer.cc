// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_web_contents_observer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extension_frame_host.h"
#include "extensions/browser/extension_mojo_binder_registry.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/browser/extension_navigation_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace extensions {

namespace {

// Returns whether `origin` belongs to `extension`. This eliminates origins that
// are not within a hosted app's web extent, and sandboxed extension frames with
// an opaque origin.
// TODO(crbug.com/40725839) See if this check is still needed after the bug is
// fixed.
bool IsOriginOfExtension(content::BrowserContext* browser_context,
                         const url::Origin& origin,
                         const Extension& extension) {
  return !origin.opaque() &&
         ExtensionRegistry::Get(browser_context)
                 ->enabled_extensions()
                 .GetExtensionOrAppByURL(origin.GetURL()) == &extension;
}

// Holds the receiving end of the MojoJS broker pipe of a navigation that is
// ready to commit, along with the ID of the extension that the document belongs
// to, until the document commits and gets its ExtensionFrameInterfaceBroker.
// Dropped along with the navigation if it does not commit.
class PendingMojoJsBroker
    : public content::NavigationHandleUserData<PendingMojoJsBroker> {
 public:
  PendingMojoJsBroker(const PendingMojoJsBroker&) = delete;
  PendingMojoJsBroker& operator=(const PendingMojoJsBroker&) = delete;
  ~PendingMojoJsBroker() override = default;

  const ExtensionId& extension_id() const { return extension_id_; }

  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> TakeReceiver() {
    return std::move(receiver_);
  }

 private:
  friend content::NavigationHandleUserData<PendingMojoJsBroker>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

  PendingMojoJsBroker(
      content::NavigationHandle& navigation_handle,
      ExtensionId extension_id,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver)
      : extension_id_(std::move(extension_id)),
        receiver_(std::move(receiver)) {}

  const ExtensionId extension_id_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver_;
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(PendingMojoJsBroker);

// A BrowserInterfaceBroker that serves only the frame binders registered by an
// extension's `ExtensionMojoBinderProvider`, for a single extension document.
// `Mojo.bindInterface()` requests for any other interface, including ones the
// frame's own BrowserInterfaceBroker serves, are reported as bad messages.
// Created once the document has committed, so that binders run against it
// rather than against the previous document of the same RenderFrameHost.
// Requests that the document sends before then are queued in the pipe.
// Destroyed along with the document.
class ExtensionFrameInterfaceBroker
    : public content::DocumentUserData<ExtensionFrameInterfaceBroker>,
      public blink::mojom::BrowserInterfaceBroker {
 public:
  ExtensionFrameInterfaceBroker(const ExtensionFrameInterfaceBroker&) = delete;
  ExtensionFrameInterfaceBroker& operator=(
      const ExtensionFrameInterfaceBroker&) = delete;
  ~ExtensionFrameInterfaceBroker() override = default;

  // blink::mojom::BrowserInterfaceBroker:
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    if (!binder_map_.TryBind(&render_frame_host(), &receiver)) {
      // Treat requests for unregistered interfaces as a bad message.
      receiver_.ReportBadMessage(
          base::StrCat({"No binder found for interface ",
                        receiver.interface_name().value_or(""),
                        " in the extension MojoJS broker"}));
    }
  }

 private:
  friend content::DocumentUserData<ExtensionFrameInterfaceBroker>;
  DOCUMENT_USER_DATA_KEY_DECL();

  ExtensionFrameInterfaceBroker(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver)
      : DocumentUserData(render_frame_host),
        binder_map_(std::move(binder_map)),
        receiver_(this, std::move(receiver)) {}

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map_;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_;
};

DOCUMENT_USER_DATA_KEY_IMPL(ExtensionFrameInterfaceBroker);

}  // namespace

// static
ExtensionWebContentsObserver* ExtensionWebContentsObserver::GetForWebContents(
    content::WebContents* web_contents) {
  return ExtensionsBrowserClient::Get()->GetExtensionWebContentsObserver(
      web_contents);
}

// static
void ExtensionWebContentsObserver::BindLocalFrameHost(
    mojo::PendingAssociatedReceiver<mojom::LocalFrameHost> receiver,
    content::RenderFrameHost* render_frame_host) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }
  auto* observer = GetForWebContents(web_contents);
  if (!observer) {
    return;
  }
  auto* efh = observer->extension_frame_host_.get();
  if (!efh) {
    return;
  }
  efh->BindLocalFrameHost(std::move(receiver), render_frame_host);
}

std::unique_ptr<ExtensionFrameHost>
ExtensionWebContentsObserver::CreateExtensionFrameHost(
    content::WebContents* web_contents) {
  return std::make_unique<ExtensionFrameHost>(web_contents);
}

void ExtensionWebContentsObserver::ListenToWindowIdChangesFrom(
    sessions::SessionTabHelper* helper) {
  if (!window_id_subscription_) {
    // We use an unretained receiver here: the callback is inside the
    // subscription, which is a member of |this|, so it can't be run after the
    // destruction of |this|.
    window_id_subscription_ = helper->RegisterForWindowIdChanged(
        base::BindRepeating(&ExtensionWebContentsObserver::OnWindowIdChanged,
                            base::Unretained(this)));
  }
}

void ExtensionWebContentsObserver::Initialize() {
  if (initialized_) {
    return;
  }

  initialized_ = true;

  extension_frame_host_ = CreateExtensionFrameHost(web_contents());

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  // We only initialize the frame if the renderer counterpart is live;
  // otherwise we wait for the RenderFrameCreated notification.
  if (main_frame->IsRenderFrameLive()) {
    InitializeRenderFrame(main_frame);
  }

  // At the point of initialization, the *only* frame that can exist is the
  // main frame.
  web_contents()->ForEachRenderFrameHost(
      [main_frame](content::RenderFrameHost* render_frame_host) {
        CHECK_EQ(render_frame_host, main_frame);
      });

  // It would be ideal if SessionTabHelper was created before this object,
  // because then we could start observing it here instead of needing to be
  // externally notified when it is created, but it isn't. If that ordering ever
  // changes, this code can be restructured and ListenToWindowIdChangesFrom()
  // can become private.
  DCHECK(!sessions::SessionTabHelper::FromWebContents(web_contents()));
}

ExtensionWebContentsObserver::ExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      dispatcher_(browser_context_),
      initialized_(false) {}

ExtensionWebContentsObserver::~ExtensionWebContentsObserver() {
}

void ExtensionWebContentsObserver::InitializeRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsRenderFrameLive());

  // At the initialization of the render frame, the last committed URL is not
  // reliable, so do not take it into account in determining whether it is an
  // extension frame.
  const Extension* frame_extension =
      GetExtensionFromFrame(render_frame_host, false);
  // This observer is attached to every WebContents, so we are also notified of
  // frames that are not in an extension process.
  if (!frame_extension) {
    return;
  }

  // |render_frame_host->GetProcess()| is an extension process. Grant permission
  // to request pages from the extension's origin.
  content::ChildProcessSecurityPolicy* security_policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int process_id = render_frame_host->GetProcess()->GetDeprecatedID();
  security_policy->GrantRequestOrigin(process_id, frame_extension->origin());

  // Notify the render frame of the view type.
  GetLocalFrameChecked(render_frame_host)
      .NotifyRenderViewType(GetViewType(render_frame_host));

  ProcessManager::Get(browser_context_)
      ->RegisterRenderFrameHost(render_frame_host, frame_extension);
}

void ExtensionWebContentsObserver::SetUpRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  InitializeRenderFrame(render_frame_host);

  const Extension* extension = GetExtensionFromFrame(render_frame_host, false);
  if (!extension) {
    return;
  }

  Manifest::Type type = extension->GetType();

  // Some extensions use file:// URLs.
  //
  // Note: this particular grant isn't relevant for hosted apps, but in the
  // future we should be careful about granting privileges to hosted app
  // subframes in places like this, since they currently stay in process with
  // their parent. A malicious site shouldn't be able to gain a hosted app's
  // privileges just by embedding a subframe to a popular hosted app.
  //
  // Note: Keep this logic in sync with related logic in
  // ChromeContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories.
  if (type == Manifest::Type::kExtension ||
      type == Manifest::Type::kLegacyPackagedApp) {
    util::InitializeFileSchemeAccessForExtension(
        render_frame_host->GetProcess()->GetID(), extension->id(),
        browser_context_);
  }

  // Tells the new frame that it's hosted in an extension process.
  //
  // This will often be a redundant IPC, because activating extensions happens
  // at the process level, not at the frame level. However, without some mild
  // refactoring this isn't trivial to do, and this way is simpler.
  //
  // Plus, we can delete the concept of activating an extension once site
  // isolation is turned on.
  RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
      ->ActivateExtensionInProcess(*extension, render_frame_host->GetProcess());

  if (IsJsErrorReportingEnabled(*extension)) {
    render_frame_host->SetWantErrorMessageStackTrace();
  }
}

void ExtensionWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  extension_frame_host_->RenderFrameDeleted(render_frame_host);
  local_frame_map_.erase(render_frame_host);
  ProcessManager::Get(browser_context_)
      ->UnregisterRenderFrameHost(render_frame_host);
  ExtensionApiFrameIdMap::Get()->OnRenderFrameDeleted(render_frame_host);
}

void ExtensionWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation is for the TopChrome WebUI, we can skip extension
  // initialization for optimization. Otherwise, we ensure the renderer process
  // is initialized including the non-TopChrome WebUI navigation.
  content::RenderProcessHost* process =
      navigation_handle->GetRenderFrameHost()->GetProcess();
  if (process->IsForTopChromeWebUI() &&
      base::FeatureList::IsEnabled(
          extensions_features::kInitialWebUIWithoutExtensions)) {
    if (!navigation_handle->IsInitialWebUINavigation()) {
      RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
          ->InitializeProcess(process);
    }
  }

  SetUpRenderFrameHost(navigation_handle->GetRenderFrameHost());
  MaybeEnableMojoJsBindings(navigation_handle);

  ScriptInjectionTracker::ReadyToCommitNavigation(PassKey(), navigation_handle);

  // We don't force autoplay to allow while prerendering.
  if (navigation_handle->GetRenderFrameHost()->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPrerendering &&
      !navigation_handle->IsPrerenderedPageActivation()) {
    return;
  }

  const ExtensionRegistry* const registry =
      ExtensionRegistry::Get(browser_context_);

  content::RenderFrameHost* parent_or_outerdoc =
      navigation_handle->GetParentFrameOrOuterDocument();

  content::RenderFrameHost* outermost_main_render_frame_host =
      parent_or_outerdoc ? parent_or_outerdoc->GetOutermostMainFrame()
                         : navigation_handle->GetRenderFrameHost();

  const Extension* const extension =
      GetExtensionFromFrame(outermost_main_render_frame_host, false);
  KioskDelegate* const kiosk_delegate =
      ExtensionsBrowserClient::Get()->GetKioskDelegate();
  DCHECK(kiosk_delegate);
  bool is_kiosk =
      extension && kiosk_delegate->IsAutoLaunchedKioskApp(extension->id());

  // If the top most frame is an extension, packaged app, hosted app, etc. then
  // the main frame and all iframes should be able to autoplay without
  // restriction. <webview> should still have autoplay blocked though.
  GURL url =
      parent_or_outerdoc
          ? parent_or_outerdoc->GetOutermostMainFrame()->GetLastCommittedURL()
          : navigation_handle->GetURL();
  if (is_kiosk || registry->enabled_extensions().GetExtensionOrAppByURL(url)) {
    mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
    navigation_handle->GetRenderFrameHost()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&client);
    client->AddAutoplayFlags(url::Origin::Create(navigation_handle->GetURL()),
                             blink::mojom::kAutoplayFlagForceAllow);
  }
}

void ExtensionWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  DCHECK(initialized_);
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  MaybeCreateMojoJsBroker(navigation_handle);

  ProcessManager* pm = ProcessManager::Get(browser_context_);

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  DCHECK(render_frame_host);

  const Extension* frame_extension =
      GetExtensionFromFrame(render_frame_host, true);
  if (pm->IsRenderFrameHostRegistered(render_frame_host)) {
    if (!frame_extension) {
      pm->UnregisterRenderFrameHost(render_frame_host);
    }
  } else if (frame_extension && render_frame_host->IsRenderFrameLive()) {
    pm->RegisterRenderFrameHost(render_frame_host, frame_extension);
  }

  // Delete the navigation id from ExtensionNavigationRegistry if it exists.
  ExtensionNavigationRegistry::Get(web_contents()->GetBrowserContext())
      ->Erase(navigation_handle->GetNavigationId());

  ScriptInjectionTracker::DidFinishNavigation(PassKey(), navigation_handle);
}

void ExtensionWebContentsObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  DCHECK(initialized_);
  if (GetViewType(web_contents()) ==
      mojom::ViewType::kExtensionBackgroundPage) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension == nullptr) {
      return;
    }
    if (is_picture_in_picture) {
      process_manager->IncrementLazyKeepaliveCount(extension, Activity::MEDIA,
                                                   Activity::kPictureInPicture);
    } else {
      process_manager->DecrementLazyKeepaliveCount(extension, Activity::MEDIA,
                                                   Activity::kPictureInPicture);
    }
  }
}

const Extension* ExtensionWebContentsObserver::GetExtensionFromFrame(
    content::RenderFrameHost* render_frame_host,
    bool verify_url) const {
  DCHECK(initialized_);
  ExtensionId extension_id = util::GetExtensionIdFromFrame(render_frame_host);
  if (extension_id.empty()) {
    return nullptr;
  }

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    return nullptr;
  }

  if (verify_url &&
      !IsOriginOfExtension(browser_context,
                           render_frame_host->GetLastCommittedOrigin(),
                           *extension)) {
    return nullptr;
  }

  return extension;
}

mojom::LocalFrame* ExtensionWebContentsObserver::GetLocalFrame(
    content::RenderFrameHost* render_frame_host) {
  // Attempting to get a remote interface before IsRenderFrameLive() will fail,
  // leaving a broken pipe that will block all further messages. Return nullptr
  // instead. Callers should try again after RenderFrameCreated().
  if (!render_frame_host->IsRenderFrameLive()) {
    return nullptr;
  }

  // Do not return a LocalFrame object for frames that do not immediately belong
  // to this WebContents. For example frames belonging to inner WebContents will
  // have their own ExtensionWebContentsObserver.
  if (content::WebContents::FromRenderFrameHost(render_frame_host) !=
      web_contents()) {
    return nullptr;
  }

  mojo::AssociatedRemote<mojom::LocalFrame>& remote =
      local_frame_map_[render_frame_host];
  if (!remote.is_bound()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        remote.BindNewEndpointAndPassReceiver());
  }
  return remote.get();
}

mojo::AssociatedRemote<mojom::LocalFrame>
ExtensionWebContentsObserver::SetLocalFrameForTesting(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedRemote<mojom::LocalFrame> remote) {
  // Match the conditions under which `GetLocalFrame()` hands out a remote. A
  // remote stored for a frame that isn't live would never be erased by
  // `RenderFrameDeleted()`, and `GetLocalFrame()` would return it instead of
  // binding a working one.
  CHECK(render_frame_host);
  CHECK(render_frame_host->IsRenderFrameLive());
  CHECK_EQ(content::WebContents::FromRenderFrameHost(render_frame_host),
           web_contents());

  mojo::AssociatedRemote<mojom::LocalFrame>& entry =
      local_frame_map_[render_frame_host];
  mojo::AssociatedRemote<mojom::LocalFrame> previous =
      std::exchange(entry, mojo::AssociatedRemote<mojom::LocalFrame>());
  entry.Bind(std::move(remote));
  return previous;
}

mojom::LocalFrame& ExtensionWebContentsObserver::GetLocalFrameChecked(
    content::RenderFrameHost* render_frame_host) {
  auto* local_frame = GetLocalFrame(render_frame_host);
  CHECK(local_frame);
  return *local_frame;
}

void ExtensionWebContentsObserver::OnWindowIdChanged(SessionID id) {
  web_contents()->ForEachRenderFrameHost(
      [&id, this](content::RenderFrameHost* render_frame_host) {
        auto* local_frame = GetLocalFrame(render_frame_host);
        if (local_frame) {
          local_frame->UpdateBrowserWindowId(id.id());
        }
      });
}

void ExtensionWebContentsObserver::MaybeEnableMojoJsBindings(
    content::NavigationHandle* navigation_handle) {
  // An activated page keeps the broker of its existing document, and so does a
  // document that navigates within itself.
  if (navigation_handle->IsPageActivation() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  const Extension* extension =
      GetExtensionFromFrame(render_frame_host, /*verify_url=*/false);
  if (!extension) {
    return;
  }
  auto* registry = ExtensionMojoBinderRegistryFactory::GetForBrowserContext(
      browser_context_);
  if (!registry || !registry->IsMojoJsEnabledForFrame(*extension)) {
    return;
  }
  // The document has not committed yet, so check the origin that it will
  // commit, as GetExtensionFromFrame() does with the last committed origin.
  // MojoJS and its broker are only set up for a document that passes this
  // check, so it is not repeated once the document commits.
  std::optional<url::Origin> origin_to_commit =
      navigation_handle->GetOriginToCommit();
  if (!origin_to_commit ||
      !IsOriginOfExtension(browser_context_, *origin_to_commit, *extension)) {
    return;
  }
  // Use a dedicated broker rather than RenderFrameHost::EnableMojoJsBindings(),
  // which would expose the frame's entire BrowserInterfaceBroker (and the
  // process-scoped interfaces) to Mojo.bindInterface(). The broker is created
  // once the document commits; until then, requests queue in the pipe.
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver;
  render_frame_host->EnableMojoJsBindingsWithBroker(
      receiver.InitWithNewPipeAndPassRemote());
  PendingMojoJsBroker::CreateForNavigationHandle(
      *navigation_handle, extension->id(), std::move(receiver));
}

void ExtensionWebContentsObserver::MaybeCreateMojoJsBroker(
    content::NavigationHandle* navigation_handle) {
  auto* pending =
      PendingMojoJsBroker::GetForNavigationHandle(*navigation_handle);
  if (!pending) {
    return;
  }
  // MaybeEnableMojoJsBindings() already decided, from the origin to commit,
  // that this document may use MojoJS, and the browser makes sure that the
  // document commits that origin. So the origin is not checked again here.
  // However, the extension may have been unloaded, or may no longer be allowed
  // to use MojoJS, between ReadyToCommitNavigation and now, so check that
  // before populating its binders. If it fails, no broker is created and the
  // document's MojoJS requests go nowhere, as the renderer already has MojoJS
  // enabled with a broker pipe that is never bound.
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(pending->extension_id());
  auto* registry = ExtensionMojoBinderRegistryFactory::GetForBrowserContext(
      browser_context_);
  if (!extension || !registry ||
      !registry->IsMojoJsEnabledForFrame(*extension)) {
    return;
  }
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  // Populate the binders now that the document has committed, so that
  // providers see the new document rather than the previous one.
  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry->PopulateFrameBinders(&binder_map, render_frame_host, *extension);
  ExtensionFrameInterfaceBroker::CreateForCurrentDocument(
      render_frame_host, std::move(binder_map), pending->TakeReceiver());
}

bool ExtensionWebContentsObserver::IsJsErrorReportingEnabled(
    const Extension& extension) const {
  auto* config_map =
      ExtensionConfigMapFactory::GetForBrowserContext(browser_context_);
  auto* provider =
      config_map ? config_map->GetConfigProvider(extension) : nullptr;
  return provider && provider->IsJsErrorReportingEnabled();
}

void ExtensionWebContentsObserver::OnDidAddMessageToConsole(
    content::RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  if (log_level != blink::mojom::ConsoleMessageLevel::kError) {
    return;
  }

  if (!source_frame) {
    return;
  }

  // If source_id is not a valid extension script URL, fall back to the frame's
  // committed URL so errors originating from the extension frame are captured.
  GURL url(source_id);
  if (!url.is_valid() || !url.SchemeIs(kExtensionScheme)) {
    url = source_frame->GetLastCommittedURL();
  }
  if (!url.is_valid() || !url.SchemeIs(kExtensionScheme)) {
    return;
  }

  // Ignore console messages from frames that are pending deletion to avoid
  // reporting errors during frame teardown.
  if (source_frame->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPendingDeletion) {
    return;
  }

  const Extension* extension =
      GetExtensionFromFrame(source_frame, /*verify_url=*/false);
  if (!extension || !IsJsErrorReportingEnabled(*extension)) {
    return;
  }

  OnExtensionJsError(source_frame, *extension, message, line_no, url,
                     untrusted_stack_trace);
}

void ExtensionWebContentsObserver::OnExtensionJsError(
    content::RenderFrameHost* source_frame,
    const Extension& extension,
    const std::u16string& message,
    int32_t line_no,
    const GURL& url,
    const std::optional<std::u16string>& untrusted_stack_trace) {}

}  // namespace extensions
