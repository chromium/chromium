// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_web_contents_observer.h"

#include "base/logging.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/mojo/interface_registration.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/view_type.h"
#include "url/origin.h"

namespace extensions {

// static
ExtensionWebContentsObserver* ExtensionWebContentsObserver::GetForWebContents(
    content::WebContents* web_contents) {
  return ExtensionsBrowserClient::Get()->GetExtensionWebContentsObserver(
      web_contents);
}

void ExtensionWebContentsObserver::Initialize() {
  if (initialized_)
    return;

  initialized_ = true;
  for (content::RenderFrameHost* rfh : web_contents()->GetAllFrames()) {
    // We only initialize the frame if the renderer counterpart is live;
    // otherwise we wait for the RenderFrameCreated notification.
    if (!rfh->IsRenderFrameLive())
      continue;

    InitializeRenderFrame(rfh);
  }
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
  security_policy->GrantRequestOrigin(
      process_id, url::Origin::Create(frame_extension->url()));

  // Notify the render frame of the view type.
  render_frame_host->Send(new ExtensionMsg_NotifyRenderViewType(
      render_frame_host->GetRoutingID(), GetViewType(web_contents())));

  ExtensionsBrowserClient::Get()->RegisterExtensionInterfaces(
      &registry_, render_frame_host, frame_extension);
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
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
    if (prefs->AllowFileAccess(extension->id())) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
          render_frame_host->GetProcess()->GetID(), url::kFileScheme);
    }
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
  ProcessManager::Get(browser_context_)
      ->UnregisterRenderFrameHost(render_frame_host);
  ExtensionApiFrameIdMap::Get()->OnRenderFrameDeleted(render_frame_host);
}

void ExtensionWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  URLLoaderFactoryManager::ReadyToCommitNavigation(navigation_handle);
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
}

void ExtensionWebContentsObserver::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  DCHECK(initialized_);
  registry_.TryBindInterface(interface_name, interface_pipe, render_frame_host);
}

void ExtensionWebContentsObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  DCHECK(initialized_);
  if (GetViewType(web_contents()) == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
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

bool ExtensionWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(
      ExtensionWebContentsObserver, message, render_frame_host)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionWebContentsObserver::PepperInstanceCreated() {
  DCHECK(initialized_);
  if (GetViewType(web_contents()) == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
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
  if (GetViewType(web_contents()) == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension)
      process_manager->DecrementLazyKeepaliveCount(
          extension, Activity::PEPPER_API, std::string());
  }
}

std::string ExtensionWebContentsObserver::GetExtensionIdFromFrame(
    content::RenderFrameHost* render_frame_host) const {
  DCHECK(initialized_);
  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(kExtensionScheme))
    return std::string();

  return site.host();
}

const Extension* ExtensionWebContentsObserver::GetExtensionFromFrame(
    content::RenderFrameHost* render_frame_host,
    bool verify_url) const {
  DCHECK(initialized_);
  std::string extension_id = GetExtensionIdFromFrame(render_frame_host);
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
    // Without site isolation, this check is needed to eliminate non-extension
    // schemes. With site isolation, this is still needed to exclude sandboxed
    // extension frames with an opaque origin.
    const GURL site_url(render_frame_host->GetSiteInstance()->GetSiteURL());
    if (origin.opaque() || site_url != content::SiteInstance::GetSiteForURL(
                                           browser_context, origin.GetURL()))
      return nullptr;
  }

  return extension;
}

void ExtensionWebContentsObserver::OnRequest(
    content::RenderFrameHost* render_frame_host,
    const ExtensionHostMsg_Request_Params& params) {
  DCHECK(initialized_);
  dispatcher_.Dispatch(params, render_frame_host,
                       render_frame_host->GetProcess()->GetID());
}

}  // namespace extensions
