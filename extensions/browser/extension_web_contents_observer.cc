// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_web_contents_observer.h"

#include "base/check.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_frame_host.h"
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
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "url/origin.h"

namespace extensions {

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
  if (!web_contents)
    return;
  auto* observer = GetForWebContents(web_contents);
  if (!observer)
    return;
  auto* efh = observer->extension_frame_host_.get();
  if (!efh)
    return;
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
  if (initialized_)
    return;

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
      initialized_(false) {
  dispatcher_.set_delegate(this);
}

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
  if (!frame_extension)
    return;

  // |render_frame_host->GetProcess()| is an extension process. Grant permission
  // to request pages from the extension's origin.
  content::ChildProcessSecurityPolicy* security_policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int process_id = render_frame_host->GetProcess()->GetID();
  security_policy->GrantRequestOrigin(process_id, frame_extension->origin());

  // Notify the render frame of the view type.
  GetLocalFrameChecked(render_frame_host)
      .NotifyRenderViewType(GetViewType(web_contents()));

  ProcessManager::Get(browser_context_)
      ->RegisterRenderFrameHost(web_contents(), render_frame_host,
                                frame_extension);
}

content::WebContents* ExtensionWebContentsObserver::GetAssociatedWebContents()
    const {
  DCHECK(initialized_);
  return web_contents();
}

void ExtensionWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  InitializeRenderFrame(render_frame_host);

  const Extension* extension = GetExtensionFromFrame(render_frame_host, false);
  if (!extension)
    return;

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
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
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
}

void ExtensionWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  local_frame_map_.erase(render_frame_host);
  ProcessManager::Get(browser_context_)
      ->UnregisterRenderFrameHost(render_frame_host);
  ExtensionApiFrameIdMap::Get()->OnRenderFrameDeleted(render_frame_host);
}

void ExtensionWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
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
  DCHECK(initialized_);
  if (!navigation_handle->HasCommitted())
    return;

  ProcessManager* pm = ProcessManager::Get(browser_context_);

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  DCHECK(render_frame_host);

  const Extension* frame_extension =
      GetExtensionFromFrame(render_frame_host, true);
  if (pm->IsRenderFrameHostRegistered(render_frame_host)) {
    if (!frame_extension)
      pm->UnregisterRenderFrameHost(render_frame_host);
  } else if (frame_extension && render_frame_host->IsRenderFrameLive()) {
    pm->RegisterRenderFrameHost(web_contents(), render_frame_host,
                                frame_extension);
  }

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
    if (extension == nullptr)
      return;
    if (is_picture_in_picture)
      process_manager->IncrementLazyKeepaliveCount(extension, Activity::MEDIA,
                                                   Activity::kPictureInPicture);
    else
      process_manager->DecrementLazyKeepaliveCount(extension, Activity::MEDIA,
                                                   Activity::kPictureInPicture);
  }
}

void ExtensionWebContentsObserver::PepperInstanceCreated() {
  DCHECK(initialized_);
  if (GetViewType(web_contents()) ==
      mojom::ViewType::kExtensionBackgroundPage) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension)
      process_manager->IncrementLazyKeepaliveCount(
          extension, Activity::PEPPER_API, std::string());
  }
}

void ExtensionWebContentsObserver::PepperInstanceDeleted() {
  DCHECK(initialized_);
  if (GetViewType(web_contents()) ==
      mojom::ViewType::kExtensionBackgroundPage) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension)
      process_manager->DecrementLazyKeepaliveCount(
          extension, Activity::PEPPER_API, std::string());
  }
}

const Extension* ExtensionWebContentsObserver::GetExtensionFromFrame(
    content::RenderFrameHost* render_frame_host,
    bool verify_url) const {
  DCHECK(initialized_);
  ExtensionId extension_id = util::GetExtensionIdFromFrame(render_frame_host);
  if (extension_id.empty())
    return nullptr;

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return nullptr;

  if (verify_url) {
    const url::Origin& origin(render_frame_host->GetLastCommittedOrigin());
    // This check is needed to eliminate origins that are not within a
    // hosted-app's web extent, and sandboxed extension frames with an opaque
    // origin.
    // TODO(crbug.com/40725839) See if extension check is still needed after bug
    // is fixed.
    auto* extension_for_origin = ExtensionRegistry::Get(browser_context)
                                     ->enabled_extensions()
                                     .GetExtensionOrAppByURL(origin.GetURL());
    if (origin.opaque() || extension_for_origin != extension)
      return nullptr;
  }

  return extension;
}

mojom::LocalFrame* ExtensionWebContentsObserver::GetLocalFrame(
    content::RenderFrameHost* render_frame_host) {
  // Attempting to get a remote interface before IsRenderFrameLive() will fail,
  // leaving a broken pipe that will block all further messages. Return nullptr
  // instead. Callers should try again after RenderFrameCreated().
  if (!render_frame_host->IsRenderFrameLive())
    return nullptr;

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
        if (local_frame)
          local_frame->UpdateBrowserWindowId(id.id());
      });
}

}  // namespace extensions
