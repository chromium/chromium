// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager_delegate.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"

using content::BrowserContext;

namespace extensions {

namespace {

// Delay between an extension becoming idle and sending a ShouldSuspend message.
// Can be modified in tests.
base::TimeDelta g_event_page_idle_time = base::Seconds(10);

// Delay between sending a ShouldSuspend message and sending a Suspend message.
unsigned g_event_page_suspending_time_msec = 5000;

ExtensionId GetExtensionID(content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  return util::GetExtensionIdForSiteInstance(
      *render_frame_host->GetSiteInstance());
}

bool IsFrameInExtensionHost(ExtensionHost* extension_host,
                            content::RenderFrameHost* render_frame_host) {
  return content::WebContents::FromRenderFrameHost(render_frame_host) ==
      extension_host->host_contents();
}

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
// TODO(devlin): Given how little this does and the amount of cruft it adds to
// the .h file (in the form of protected members), we should consider just
// moving the incognito logic into the base class.
class IncognitoProcessManager : public ProcessManager {
 public:
  IncognitoProcessManager(BrowserContext* incognito_context,
                          BrowserContext* original_context,
                          ExtensionRegistry* extension_registry);

  IncognitoProcessManager(const IncognitoProcessManager&) = delete;
  IncognitoProcessManager& operator=(const IncognitoProcessManager&) = delete;

  ~IncognitoProcessManager() override {}
  bool CreateBackgroundHost(const Extension* extension,
                            const GURL& url) override;
  scoped_refptr<content::SiteInstance> GetSiteInstanceForURL(const GURL& url)
      override;
};

static void CreateBackgroundHostForExtensionLoad(
    ProcessManager* manager, const Extension* extension) {
  if (BackgroundInfo::HasPersistentBackgroundPage(extension))
    manager->CreateBackgroundHost(extension,
                                  BackgroundInfo::GetBackgroundURL(extension));
}

void PropagateExtensionWakeResult(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  std::move(callback).Run(context_info != nullptr);
}

}  // namespace

struct ProcessManager::BackgroundPageData {
  // The count of things keeping the lazy background page alive.
  // TODO(crbug.com://695711): Work on a plan to remove this and rely
  // on activities.size() instead.
  int lazy_keepalive_count = 0;

  // True if the page responded to the ShouldSuspend message and is currently
  // dispatching the suspend event. During this time any events that arrive will
  // cancel the suspend process and an onSuspendCanceled event will be
  // dispatched to the page.
  bool is_closing = false;

  // Stores the value of the incremented
  // ProcessManager::last_background_close_sequence_id_ whenever the extension
  // is active. A copy of the ID is also passed in the callbacks and IPC
  // messages leading up to CloseLazyBackgroundPageNow. The process is aborted
  // if the IDs ever differ due to new activity.
  uint64_t close_sequence_id = 0ull;

  ActivitiesMultiset activities;
};

// Data of a RenderFrameHost associated with an extension.
struct ProcessManager::ExtensionRenderFrameData {
  // The type of the view.
  extensions::mojom::ViewType view_type = extensions::mojom::ViewType::kInvalid;

  // Whether the view is keeping the lazy background page alive or not.
  bool has_keepalive = false;

  // Returns whether the view can keep the lazy background page alive or not.
  bool CanKeepalive() const {
    switch (view_type) {
      case extensions::mojom::ViewType::kAppWindow:
      case extensions::mojom::ViewType::kBackgroundContents:
      case extensions::mojom::ViewType::kComponent:
      case extensions::mojom::ViewType::kExtensionGuest:
      case extensions::mojom::ViewType::kExtensionPopup:
      case extensions::mojom::ViewType::kTabContents:
      case extensions::mojom::ViewType::kDeveloperTools:
        return true;

      case extensions::mojom::ViewType::kInvalid:
      case extensions::mojom::ViewType::kExtensionBackgroundPage:
      case extensions::mojom::ViewType::kOffscreenDocument:
      case extensions::mojom::ViewType::kExtensionSidePanel:
        return false;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

//
// ProcessManager
//

// static
ProcessManager* ProcessManager::Get(BrowserContext* context) {
  return ProcessManagerFactory::GetForBrowserContext(context);
}

// static
ProcessManager* ProcessManager::Create(BrowserContext* context) {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(context);
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (client->IsGuestSession(context)) {
    // In the guest session, there is a single off-the-record context.  Unlike
    // a regular incognito mode, background pages of extensions must be
    // created regardless of whether extensions use "spanning" or "split"
    // incognito behavior.
    BrowserContext* original_context = client->GetContextRedirectedToOriginal(
        context, /*force_guest_profile=*/true);
    return new ProcessManager(context, original_context, extension_registry);
  }

  if (context->IsOffTheRecord()) {
    BrowserContext* original_context = client->GetContextRedirectedToOriginal(
        context, /*force_guest_profile=*/true);
    return new IncognitoProcessManager(
        context, original_context, extension_registry);
  }

  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateForTesting(
    BrowserContext* context,
    ExtensionRegistry* extension_registry) {
  DCHECK(!context->IsOffTheRecord());
  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateIncognitoForTesting(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ExtensionRegistry* extension_registry) {
  DCHECK(incognito_context->IsOffTheRecord());
  DCHECK(!original_context->IsOffTheRecord());
  return new IncognitoProcessManager(incognito_context,
                                     original_context,
                                     extension_registry);
}

ProcessManager::ProcessManager(BrowserContext* context,
                               BrowserContext* original_context,
                               ExtensionRegistry* extension_registry)
    : extension_registry_(extension_registry),
      site_instance_(content::SiteInstance::Create(context)),
      browser_context_(context),
      startup_background_hosts_created_(false),
      last_background_close_sequence_id_(0) {
  // ExtensionRegistry is shared between incognito and regular contexts.
  DCHECK_EQ(original_context, extension_registry_->browser_context());
  extension_registry_->AddObserver(this);

  // Only the original profile needs to listen for ready to create background
  // pages for all spanning extensions.
  if (!context->IsOffTheRecord()) {
    ExtensionSystem::Get(context)->ready().Post(
        FROM_HERE,
        base::BindOnce(&ProcessManager::MaybeCreateStartupBackgroundHosts,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  content::DevToolsAgentHost::AddObserver(this);
}

ProcessManager::~ProcessManager() {
  content::DevToolsAgentHost::RemoveObserver(this);
}

void ProcessManager::Shutdown() {
  extension_registry_->RemoveObserver(this);
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
  content::DevToolsAgentHost::RemoveObserver(this);
  site_instance_ = nullptr;

  for (auto& observer : observer_list_)
    observer.OnProcessManagerShutdown(this);
}

void ProcessManager::RegisterRenderFrameHost(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(render_frame_host->IsRenderFrameLive());
  ExtensionRenderFrameData* data = &all_extension_frames_[render_frame_host];
  data->view_type = GetViewType(web_contents);

  // Keep the lazy background page alive as long as any non-background-page
  // extension views are visible. Keepalive count balanced in
  // UnregisterRenderFrame.
  AcquireLazyKeepaliveCountForFrame(render_frame_host);

  for (auto& observer : observer_list_)
    observer.OnExtensionFrameRegistered(extension->id(), render_frame_host);
}

void ProcessManager::UnregisterRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  auto frame = all_extension_frames_.find(render_frame_host);

  if (frame != all_extension_frames_.end()) {
    ExtensionId extension_id = GetExtensionID(render_frame_host);
    // Keepalive count, balanced in RegisterRenderFrame.
    ReleaseLazyKeepaliveCountForFrame(render_frame_host);
    all_extension_frames_.erase(frame);

    for (auto& observer : observer_list_)
      observer.OnExtensionFrameUnregistered(extension_id, render_frame_host);
  }
}

scoped_refptr<content::SiteInstance> ProcessManager::GetSiteInstanceForURL(
    const GURL& url) {
  return site_instance_->GetRelatedSiteInstance(url);
}

const ProcessManager::FrameSet ProcessManager::GetAllFrames() const {
  FrameSet result;
  for (const auto& key_value : all_extension_frames_)
    result.insert(key_value.first);
  return result;
}

ProcessManager::FrameSet ProcessManager::GetRenderFrameHostsForExtension(
    const ExtensionId& extension_id) {
  FrameSet result;
  for (const auto& key_value : all_extension_frames_) {
    if (GetExtensionID(key_value.first) == extension_id)
      result.insert(key_value.first);
  }
  return result;
}

bool ProcessManager::IsRenderFrameHostRegistered(
    content::RenderFrameHost* render_frame_host) {
  return base::Contains(all_extension_frames_, render_frame_host);
}

void ProcessManager::AddObserver(ProcessManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ProcessManager::RemoveObserver(ProcessManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ProcessManager::CreateBackgroundHost(const Extension* extension,
                                          const GURL& url) {
  DCHECK(!BackgroundInfo::IsServiceWorkerBased(extension))
      << "CreateBackgroundHostForExtensionLoad called for service worker based"
         "background page";
  // Hosted apps are taken care of from BackgroundContentsService. Ignore them
  // here.
  if (extension->is_hosted_app())
    return false;

  // Don't create hosts if the embedder doesn't allow it.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate &&
      !delegate->IsExtensionBackgroundPageAllowed(browser_context_, *extension))
    return false;

  // Don't create multiple background hosts for an extension.
  if (GetBackgroundHostForExtension(extension->id()))
    return true;  // TODO(kalman): return false here? It might break things...

  // Don't create a background host when the BrowserContext is shutting down.
  if (browser_context_->ShutdownStarted()) {
    return false;
  }

  DVLOG(1) << "CreateBackgroundHost " << extension->id();
  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url).get(), url,
                        mojom::ViewType::kExtensionBackgroundPage);
  host->SetCloseHandler(
      base::BindOnce(&ProcessManager::HandleCloseExtensionHost,
                     weak_ptr_factory_.GetWeakPtr()));
  host->CreateRendererSoon();
  OnBackgroundHostCreated(host);
  return true;
}

void ProcessManager::MaybeCreateStartupBackgroundHosts() {
  if (startup_background_hosts_created_)
    return;

  if (!ExtensionSystem::Get(browser_context_)->ready().is_signaled())
    return;

  // The embedder might disallow background pages entirely.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate &&
      !delegate->AreBackgroundPagesAllowedForContext(browser_context_))
    return;

  // The embedder might want to defer background page loading. For example,
  // Chrome defers background page loading when it is launched to show the app
  // list, then triggers a load later when a browser window opens.
  if (delegate &&
      delegate->DeferCreatingStartupBackgroundHosts(browser_context_))
    return;

  CreateStartupBackgroundHosts();
  startup_background_hosts_created_ = true;
}

ExtensionHost* ProcessManager::GetBackgroundHostForExtension(
    const ExtensionId& extension_id) {
  for (ExtensionHost* host : background_hosts_) {
    if (host->extension_id() == extension_id)
      return host;
  }
  return nullptr;
}

ExtensionHost* ProcessManager::GetBackgroundHostForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame())
    return nullptr;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  for (ExtensionHost* extension_host : background_hosts_) {
    if (extension_host->host_contents() == web_contents)
      return extension_host;
  }
  return nullptr;
}

bool ProcessManager::IsEventPageSuspended(const ExtensionId& extension_id) {
  return GetBackgroundHostForExtension(extension_id) == nullptr;
}

bool ProcessManager::WakeEventPage(const ExtensionId& extension_id,
                                   base::OnceCallback<void(bool)> callback) {
  if (GetBackgroundHostForExtension(extension_id)) {
    // The extension is already awake.
    return false;
  }

  const auto context_id =
      LazyContextId::ForBackgroundPage(browser_context_, extension_id);
  context_id.GetTaskQueue()->AddPendingTask(
      context_id,
      base::BindOnce(&PropagateExtensionWakeResult, std::move(callback)));
  return true;
}

bool ProcessManager::IsBackgroundHostClosing(const ExtensionId& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  return (host && background_page_data_[extension_id].is_closing);
}

const Extension* ProcessManager::GetExtensionForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return extension_registry_->enabled_extensions().GetByID(
      GetExtensionID(render_frame_host));
}

const Extension* ProcessManager::GetExtensionForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents->GetSiteInstance())
    return nullptr;
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(
          util::GetExtensionIdForSiteInstance(
              *web_contents->GetSiteInstance()));
  if (extension && extension->is_hosted_app()) {
    // For hosted apps, be sure to exclude URLs outside of the app that might
    // be loaded in the same SiteInstance (extensions guarantee that only
    // extension urls are loaded in that SiteInstance).
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetLastCommittedEntry();
    // If the "last committed" entry is the initial entry, check the pending
    // entry. This can happen in cases where we query this before any entry is
    // fully committed, such as when attributing a WebContents for the
    // TaskManager. If there is a committed navigation, use that instead.
    if (entry->IsInitialEntry())
      entry = controller.GetPendingEntry();
    if (!entry ||
        extension_registry_->enabled_extensions().GetExtensionOrAppByURL(
            entry->GetURL()) != extension) {
      return nullptr;
    }
  }

  return extension;
}

int ProcessManager::GetLazyKeepaliveCount(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return -1;

  return background_page_data_[extension->id()].lazy_keepalive_count;
}

void ProcessManager::IncrementLazyKeepaliveCount(
    const Extension* extension,
    Activity::Type activity_type,
    const std::string& extra_data) {
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    BackgroundPageData& data = background_page_data_[extension->id()];
    if (++data.lazy_keepalive_count == 1)
      OnLazyBackgroundPageActive(extension->id());
    data.activities.insert(std::make_pair(activity_type, extra_data));
  }
}

void ProcessManager::DecrementLazyKeepaliveCount(
    const Extension* extension,
    Activity::Type activity_type,
    const std::string& extra_data) {
  if (BackgroundInfo::HasLazyBackgroundPage(extension))
    DecrementLazyKeepaliveCount(extension->id(), activity_type, extra_data);
}

void ProcessManager::NotifyExtensionProcessTerminated(
    const Extension* extension) {
  DCHECK(extension);
  for (auto& observer : observer_list_)
    observer.OnExtensionProcessTerminated(extension);
}

ProcessManager::ActivitiesMultiset ProcessManager::GetLazyKeepaliveActivities(
    const Extension* extension) {
  ProcessManager::ActivitiesMultiset result;
  if (BackgroundInfo::HasLazyBackgroundPage(extension))
    result = background_page_data_[extension->id()].activities;
  return result;
}

void ProcessManager::OnShouldSuspendAck(const ExtensionId& extension_id,
                                        uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    mojom::Renderer* renderer =
        RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
            ->GetRenderer(host->render_process_host());
    if (renderer) {
      renderer->SuspendExtension(
          extension_id,
          base::BindOnce(&ProcessManager::OnSuspendAck,
                         weak_ptr_factory_.GetWeakPtr(), extension_id));
    }
  }
}

void ProcessManager::OnSuspendAck(const ExtensionId& extension_id) {
  background_page_data_[extension_id].is_closing = true;
  uint64_t sequence_id = background_page_data_[extension_id].close_sequence_id;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ProcessManager::CloseLazyBackgroundPageNow,
                     weak_ptr_factory_.GetWeakPtr(), extension_id, sequence_id),
      base::Milliseconds(g_event_page_suspending_time_msec));
}

void ProcessManager::NetworkRequestStarted(
    content::RenderFrameHost* render_frame_host,
    uint64_t request_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionID(render_frame_host));
  if (!host || !IsFrameInExtensionHost(host, render_frame_host))
    return;

  auto result =
      pending_network_requests_.insert(std::make_pair(request_id, host));
  DCHECK(result.second) << "Duplicate network request IDs.";

  IncrementLazyKeepaliveCount(host->extension(), Activity::NETWORK,
                              base::NumberToString(request_id));
  host->OnNetworkRequestStarted(request_id);
}

void ProcessManager::NetworkRequestDone(
    content::RenderFrameHost* render_frame_host,
    uint64_t request_id) {
  auto result = pending_network_requests_.find(request_id);
  if (result == pending_network_requests_.end())
    return;

  // The cached |host| can be invalid, if it was deleted between the time it
  // was inserted in the map and the look up. It is checked to ensure it is in
  // the list of existing background_hosts_.
  ExtensionHost* host = result->second;
  pending_network_requests_.erase(result);

  if (!base::Contains(background_hosts_, host)) {
    return;
  }

  DCHECK(IsFrameInExtensionHost(host, render_frame_host));

  host->OnNetworkRequestDone(request_id);
  DecrementLazyKeepaliveCount(host->extension(), Activity::NETWORK,
                              base::NumberToString(request_id));
}

void ProcessManager::CancelSuspend(const Extension* extension) {
  bool& is_closing = background_page_data_[extension->id()].is_closing;
  ExtensionHost* host = GetBackgroundHostForExtension(extension->id());
  if (host && is_closing) {
    is_closing = false;
    mojom::Renderer* renderer =
        RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
            ->GetRenderer(host->render_process_host());
    if (renderer)
      renderer->CancelSuspendExtension(extension->id());
    // This increment / decrement is to simulate an instantaneous event. This
    // has the effect of invalidating close_sequence_id, preventing any in
    // progress closes from completing and starting a new close process if
    // necessary.
    IncrementLazyKeepaliveCount(extension, Activity::PROCESS_MANAGER,
                                Activity::kCancelSuspend);
    DecrementLazyKeepaliveCount(extension, Activity::PROCESS_MANAGER,
                                Activity::kCancelSuspend);
  }
}

void ProcessManager::CloseBackgroundHosts() {
  // Delete from a copy because deletion of the ExtensionHosts will trigger
  // callbacks to modify the |background_hosts_| set.
  ExtensionHostSet hosts_copy = background_hosts_;
  for (ExtensionHost* host : hosts_copy) {
    // Deleting the host will cause a OnExtensionHostDestroyed which will cause
    // the removal of the host from the |background_hosts_| set in the
    // OnExtensionHostDestroyed() method below.
    delete host;
    DCHECK_EQ(0u, background_hosts_.count(host));
  }

  // At this point there should be nothing left in |background_hosts_|.
  DCHECK(background_hosts_.empty());
}

// static
void ProcessManager::SetEventPageIdleTimeForTesting(unsigned idle_time_msec) {
  CHECK_GT(idle_time_msec, 0u);
  g_event_page_idle_time = base::Milliseconds(idle_time_msec);
}

// static
void ProcessManager::SetEventPageSuspendingTimeForTesting(
    unsigned suspending_time_msec) {
  g_event_page_suspending_time_msec = suspending_time_msec;
}

////////////////////////////////////////////////////////////////////////////////
// Private

void ProcessManager::OnExtensionLoaded(BrowserContext* browser_context,
                                       const Extension* extension) {
  if (ExtensionSystem::Get(browser_context)->ready().is_signaled()) {
    // The extension system is ready, so create the background host.
    CreateBackgroundHostForExtensionLoad(this, extension);
  }
}

void ProcessManager::OnExtensionUnloaded(BrowserContext* browser_context,
                                         const Extension* extension,
                                         UnloadedExtensionReason reason) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension->id());
  if (host != nullptr)
    CloseBackgroundHost(host);
  UnregisterExtension(extension->id());
}

void ProcessManager::CreateStartupBackgroundHosts() {
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.ProcessManagerStartupHostsTime2");
  DCHECK(!startup_background_hosts_created_);
  for (const scoped_refptr<const Extension>& extension :
           extension_registry_->enabled_extensions()) {
    CreateBackgroundHostForExtensionLoad(this, extension.get());
    for (auto& observer : observer_list_)
      observer.OnBackgroundHostStartup(extension.get());
  }
}

void ProcessManager::OnBackgroundHostCreated(ExtensionHost* host) {
  DCHECK_EQ(browser_context_, host->browser_context());
  background_hosts_.insert(host);
  host->AddObserver(this);

  for (auto& observer : observer_list_)
    observer.OnBackgroundHostCreated(host);
}

void ProcessManager::CloseBackgroundHost(ExtensionHost* host) {
  ExtensionId extension_id = host->extension_id();
  CHECK(host->extension_host_type() ==
        mojom::ViewType::kExtensionBackgroundPage);
  delete host;
  // |host| should deregister itself from our structures.
  CHECK(!base::Contains(background_hosts_, host));

  for (auto& observer : observer_list_)
    observer.OnBackgroundHostClose(extension_id);
}

void ProcessManager::AcquireLazyKeepaliveCountForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto it = all_extension_frames_.find(render_frame_host);
  if (it == all_extension_frames_.end())
    return;

  ExtensionRenderFrameData& data = it->second;
  if (data.CanKeepalive() && !data.has_keepalive) {
    const Extension* extension =
        GetExtensionForRenderFrameHost(render_frame_host);
    if (extension) {
      IncrementLazyKeepaliveCount(extension, Activity::PROCESS_MANAGER,
                                  Activity::kRenderFrame);
      data.has_keepalive = true;
    }
  }
}

void ProcessManager::ReleaseLazyKeepaliveCountForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto iter = all_extension_frames_.find(render_frame_host);
  if (iter == all_extension_frames_.end())
    return;

  ExtensionRenderFrameData& data = iter->second;
  if (data.CanKeepalive() && data.has_keepalive) {
    const Extension* extension =
        GetExtensionForRenderFrameHost(render_frame_host);
    if (extension) {
      DecrementLazyKeepaliveCount(extension, Activity::PROCESS_MANAGER,
                                  Activity::kRenderFrame);
      data.has_keepalive = false;
    }
  }
}

base::Uuid ProcessManager::IncrementServiceWorkerKeepaliveCount(
    const WorkerId& worker_id,
    content::ServiceWorkerExternalRequestTimeoutType timeout_type,
    Activity::Type activity_type,
    const std::string& extra_data) {
  int64_t service_worker_version_id = worker_id.version_id;
  DCHECK(!worker_id.extension_id.empty());
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(worker_id.extension_id);

  DCHECK(extension);
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));

  base::Uuid request_uuid = base::Uuid::GenerateRandomV4();


  content::ServiceWorkerContext* service_worker_context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser_context_);

  content::ServiceWorkerExternalRequestResult start_result =
      service_worker_context->StartingExternalRequest(
          service_worker_version_id, timeout_type, request_uuid);

  service_worker_keepalives_[request_uuid] = ServiceWorkerKeepaliveData{
      worker_id, activity_type, extra_data, timeout_type, start_result};

  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground."
      "ProcessManagerStartingExternalRequestResult",
      start_result);

  return request_uuid;
}

void ProcessManager::DecrementLazyKeepaliveCount(
    const ExtensionId& extension_id,
    Activity::Type activity_type,
    const std::string& extra_data) {
  BackgroundPageData& data = background_page_data_[extension_id];

  DCHECK(data.lazy_keepalive_count > 0 ||
         !extension_registry_->enabled_extensions().Contains(extension_id));
  --data.lazy_keepalive_count;
  const auto it =
      data.activities.find(std::make_pair(activity_type, extra_data));
  if (it != data.activities.end()) {
    data.activities.erase(it);
  }

  // If we reach a zero keepalive count when the lazy background page is about
  // to be closed, incrementing close_sequence_id will cancel the close
  // sequence and cause the background page to linger. So check is_closing
  // before initiating another close sequence.
  if (data.lazy_keepalive_count == 0) {
    // Clear any leftover activities.
    data.activities.clear();
    if (!background_page_data_[extension_id].is_closing) {
      data.close_sequence_id = ++last_background_close_sequence_id_;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ProcessManager::OnLazyBackgroundPageIdle,
                         weak_ptr_factory_.GetWeakPtr(), extension_id,
                         last_background_close_sequence_id_),
          g_event_page_idle_time);
    }
  }
}

void ProcessManager::DecrementServiceWorkerKeepaliveCount(
    const WorkerId& worker_id,
    const base::Uuid& request_uuid,
    Activity::Type activity_type,
    const std::string& extra_data) {
  DCHECK(!worker_id.extension_id.empty());
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(worker_id.extension_id);
  if (!extension)
    return;

  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));

  // Find and remove the entry from `service_worker_keepalives_`.
  auto iter = service_worker_keepalives_.find(request_uuid);
  CHECK(iter != service_worker_keepalives_.end());
  CHECK_EQ(iter->second.worker_id, worker_id);
  CHECK_EQ(iter->second.activity_type, activity_type);
  CHECK_EQ(iter->second.extra_data, extra_data);
  content::ServiceWorkerExternalRequestResult start_result =
      iter->second.start_result;
  service_worker_keepalives_.erase(iter);

  int64_t service_worker_version_id = worker_id.version_id;
  content::ServiceWorkerContext* service_worker_context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser_context_);

  content::ServiceWorkerExternalRequestResult finish_result =
      service_worker_context->FinishedExternalRequest(service_worker_version_id,
                                                      request_uuid);

  if (start_result == content::ServiceWorkerExternalRequestResult::kOk) {
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground."
        "ProcessManagerFinishedExternalRequestResultWithSuccessfulStart",
        finish_result);
  } else {
    base::UmaHistogramEnumeration(
        "Extensions.ServiceWorkerBackground."
        "ProcessManagerFinishedExternalRequestResultWithUnsuccessfulStart",
        finish_result);
  }

  // Example of when kWorkerNotRunning can happen is when the renderer process
  // is killed while handling a service worker request (e.g. because of a bad
  // IPC message).
  // kNullContext can occur if the keepalive is being removed during browser
  // context tear-down, since the ServiceWorkerContext can shut down before
  // the ProcessManager.
  DCHECK((finish_result == content::ServiceWorkerExternalRequestResult::kOk) ||
         (finish_result ==
          content::ServiceWorkerExternalRequestResult::kWorkerNotRunning) ||
         (finish_result ==
          content::ServiceWorkerExternalRequestResult::kNullContext))
      << "; result = " << static_cast<int>(finish_result);
}

void ProcessManager::OnLazyBackgroundPageIdle(const ExtensionId& extension_id,
                                              uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host && !background_page_data_[extension_id].is_closing &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Tell the renderer we are about to close. This is a simple ping that the
    // renderer will respond to. The purpose is to control sequencing: if the
    // extension remains idle until the renderer responds, then we know that the
    // extension process is ready to shut down. If our close_sequence_id has
    // already changed, then we would ignore the reply to this message, so we
    // don't send the ping.
    mojom::Renderer* renderer =
        RendererStartupHelperFactory::GetForBrowserContext(browser_context())
            ->GetRenderer(host->render_process_host());
    if (renderer) {
      renderer->ShouldSuspend(base::BindOnce(
          &ProcessManager::OnShouldSuspendAck, weak_ptr_factory_.GetWeakPtr(),
          extension_id, sequence_id));
    }
  }
}

void ProcessManager::OnLazyBackgroundPageActive(
    const ExtensionId& extension_id) {
  if (!background_page_data_[extension_id].is_closing) {
    // Cancel the current close sequence by changing the close_sequence_id,
    // which causes us to ignore the next ShouldSuspendAck.
    background_page_data_[extension_id].close_sequence_id =
        ++last_background_close_sequence_id_;
  }
}

void ProcessManager::CloseLazyBackgroundPageNow(const ExtensionId& extension_id,
                                                uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Handle the case where the keepalive count was increased after the
    // OnSuspend event was sent.
    if (background_page_data_[extension_id].lazy_keepalive_count > 0) {
      CancelSuspend(host->extension());
      return;
    }

    // Close remaining views.
    std::vector<content::RenderFrameHost*> frames_to_close;
    for (const auto& key_value : all_extension_frames_) {
      if (key_value.second.CanKeepalive() &&
          GetExtensionID(key_value.first) == extension_id) {
        DCHECK(!key_value.second.has_keepalive);
        frames_to_close.push_back(key_value.first);
      }
    }
    for (content::RenderFrameHost* frame : frames_to_close) {
      content::WebContents::FromRenderFrameHost(frame)->ClosePage();
      // WebContents::ClosePage() may result in calling
      // UnregisterRenderFrameHost() asynchronously and may cause race
      // conditions when the background page is reloaded.
      // To avoid this, unregister the view now.
      UnregisterRenderFrameHost(frame);
    }

    host = GetBackgroundHostForExtension(extension_id);
    if (host)
      CloseBackgroundHost(host);
  }
}

const Extension* ProcessManager::GetExtensionForAgentHost(
    content::DevToolsAgentHost* agent_host) {
  content::WebContents* web_contents = agent_host->GetWebContents();
  // Ignore unrelated notifications.
  if (!web_contents || web_contents->GetBrowserContext() != browser_context_)
    return nullptr;
  if (GetViewType(web_contents) != mojom::ViewType::kExtensionBackgroundPage)
    return nullptr;
  return GetExtensionForWebContents(web_contents);
}

void ProcessManager::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  if (const Extension* extension = GetExtensionForAgentHost(agent_host)) {
    // Keep the lazy background page alive while it's being inspected.
    CancelSuspend(extension);
    IncrementLazyKeepaliveCount(extension, Activity::DEV_TOOLS, std::string());
  }
}

void ProcessManager::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  if (const Extension* extension = GetExtensionForAgentHost(agent_host))
    DecrementLazyKeepaliveCount(extension, Activity::DEV_TOOLS, "");
}

void ProcessManager::UnregisterExtension(const ExtensionId& extension_id) {
  // The lazy_keepalive_count may be greater than zero at this point because
  // RenderFrameHosts are still alive. During extension reloading, they will
  // decrement the lazy_keepalive_count to negative for the new extension
  // instance when they are destroyed. Since we are erasing the background page
  // data for the unloaded extension, unregister the RenderFrameHosts too.
  for (auto it = all_extension_frames_.begin();
       it != all_extension_frames_.end();) {
    content::RenderFrameHost* host = it->first;
    if (GetExtensionID(host) == extension_id) {
      all_extension_frames_.erase(it++);
      for (auto& observer : observer_list_)
        observer.OnExtensionFrameUnregistered(extension_id, host);
    } else {
      ++it;
    }
  }

  background_page_data_.erase(extension_id);

  for (const WorkerId& worker_id :
       all_running_extension_workers_.GetAllForExtension(extension_id)) {
    StopTrackingServiceWorkerRunningInstance(worker_id);
  }
#if DCHECK_IS_ON()
  // Sanity check: No worker entry should exist for |extension_id|.
  DCHECK(
      all_running_extension_workers_.GetAllForExtension(extension_id).empty());
#endif
}

void ProcessManager::StartTrackingServiceWorkerRunningInstance(
    const WorkerId& worker_id) {
  all_running_extension_workers_.Add(worker_id, browser_context_);
  worker_context_ids_[worker_id] = base::Uuid::GenerateRandomV4();

  // Observe the RenderProcessHost for cleaning up on process shutdown.
  int render_process_id = worker_id.render_process_id;
  bool inserted = worker_process_to_extension_ids_[render_process_id]
                      .insert(worker_id.extension_id)
                      .second;
  if (inserted) {
    content::RenderProcessHost* render_process_host =
        content::RenderProcessHost::FromID(render_process_id);
    DCHECK(render_process_host);
    if (!process_observations_.IsObservingSource(render_process_host)) {
      // These will be cleaned up in RenderProcessExited().
      process_observations_.AddObservation(render_process_host);
    }
    for (auto& observer : observer_list_)
      observer.OnStartedTrackingServiceWorkerInstance(worker_id);
  }
}

void ProcessManager::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK(process_observations_.IsObservingSource(host));
  process_observations_.RemoveObservation(host);
  const int render_process_id = host->GetID();
  // Look up and then clean up the entries that are affected by
  // |render_process_id| destruction.
  //
  // TODO(lazyboy): Revisit this once incognito is tested for extension SWs, as
  // the cleanup below only works because regular and OTR ProcessManagers are
  // separate. The conclusive approach would be to have a
  // all_running_extension_workers_.RemoveAllForProcess(render_process_id)
  // method:
  //   Pros: We won't need worker_process_to_extension_ids_ anymore.
  //   Cons: We would require traversing all workers within
  //         |all_running_extension_workers_| (slow) as things stand right now.
  auto iter = worker_process_to_extension_ids_.find(render_process_id);
  if (iter == worker_process_to_extension_ids_.end())
    return;
  for (const ExtensionId& extension_id : iter->second) {
    for (const WorkerId& worker_id :
         all_running_extension_workers_.GetAllForExtension(extension_id,
                                                           render_process_id)) {
      StopTrackingServiceWorkerRunningInstance(worker_id);
    }
  }
#if DCHECK_IS_ON()
  // Sanity check: No worker entry should exist for any |extension_id| running
  // inside the RenderProcessHost that died.
  for (const ExtensionId& extension_id : iter->second)
    DCHECK(all_running_extension_workers_.GetAllForExtension(extension_id)
               .empty());
#endif
  worker_process_to_extension_ids_.erase(iter);
}

void ProcessManager::OnExtensionHostDestroyed(ExtensionHost* host) {
  TRACE_EVENT0("browser,startup", "ProcessManager::OnExtensionHostDestroyed");
  host->RemoveObserver(this);

  DCHECK(base::Contains(background_hosts_, host));
  background_hosts_.erase(host);
  // Note: |host->extension()| may be null at this point.
  ClearBackgroundPageData(host->extension_id());
}

void ProcessManager::HandleCloseExtensionHost(ExtensionHost* host) {
  TRACE_EVENT0("browser,startup", "ProcessManager::OnExtensionHostShouldClose");
  DCHECK_EQ(mojom::ViewType::kExtensionBackgroundPage,
            host->extension_host_type());
  CloseBackgroundHost(host);
  // WARNING: `host` is deleted at this point!
}

void ProcessManager::StopTrackingServiceWorkerRunningInstance(
    const WorkerId& worker_id) {
  // TODO(crbug.com/40936639): Remove the ability for multiple workers to be
  // stored for an extension and then remove all these similar checks and loops
  // that are assuming there can be more than one.
  if (!all_running_extension_workers_.Contains(worker_id)) {
    // Multiple notifications can try to remove a worker when the worker
    // stops (DidStopServiceWorkerContext(),
    // ProcessManager::RenderProcessExit(), or extension uninstall/disable).
    return;
  }

  all_running_extension_workers_.Remove(worker_id);
  worker_context_ids_.erase(worker_id);
  for (auto& observer : observer_list_)
    observer.OnStoppedTrackingServiceWorkerInstance(worker_id);
}

// TODO(crbug.com/40936639): Deduplicate this method with it's other overload
// once multi workers per extension is fixed.
void ProcessManager::StopTrackingServiceWorkerRunningInstance(
    const ExtensionId& extension_id,
    int64_t worker_version_id) {
  // We need the specific version because an extension could be
  // re-activated before StopTrackingServiceWorkerRunningInstance() is called.
  // In that case we might try to stop tracking the new version instance of the
  // running worker if we don't check the version id.
  std::vector<WorkerId> worker_ids_for_extension =
      all_running_extension_workers_.GetAllForExtension(extension_id,
                                                        worker_version_id);

  if (worker_ids_for_extension.empty()) {
    // Multiple notifications can try to remove a worker when the worker
    // stops (DidStopServiceWorkerContext(),
    // ProcessManager::RenderProcessExit(), or extension uninstall/disable).
    return;
  }

  // TODO(crbug.com/40936639): After the fix releases there should only be one
  // worker instance tracked for each extension at any time. If there is still
  // more than one then do not delete it so we will count it and know about it.
  // Confirm more thoroughly with DUMP_WILL_BE_CHECK() if metrics look
  // promising.
  if (worker_ids_for_extension.size() > 1u) {
    return;
  }

  StopTrackingServiceWorkerRunningInstance(worker_ids_for_extension[0]);
}

bool ProcessManager::HasServiceWorker(const WorkerId& worker_id) const {
  return all_running_extension_workers_.Contains(worker_id);
}

std::vector<WorkerId> ProcessManager::GetServiceWorkersForExtension(
    const ExtensionId& extension_id) const {
  return all_running_extension_workers_.GetAllForExtension(extension_id);
}

base::Uuid ProcessManager::GetContextIdForWorker(
    const WorkerId& worker_id) const {
  auto iter = worker_context_ids_.find(worker_id);
  return iter != worker_context_ids_.end() ? iter->second : base::Uuid();
}

std::vector<ProcessManager::ServiceWorkerKeepaliveData>
ProcessManager::GetServiceWorkerKeepaliveDataForRecords(
    const ExtensionId& extension_id) const {
  std::vector<ServiceWorkerKeepaliveData> result;
  for (const auto& entry : service_worker_keepalives_) {
    if (entry.second.worker_id.extension_id == extension_id) {
      result.push_back(entry.second);
    }
  }

  return result;
}

std::vector<WorkerId> ProcessManager::GetAllWorkersIdsForTesting() {
  return all_running_extension_workers_.GetAllForTesting();  // IN-TEST
}

void ProcessManager::ClearBackgroundPageData(const ExtensionId& extension_id) {
  background_page_data_.erase(extension_id);

  // Re-register all RenderFrames for this extension. We do this to restore
  // the lazy_keepalive_count (if any) to properly reflect the number of open
  // views.
  for (const auto& key_value : all_extension_frames_) {
    // Do not increment the count when |has_keepalive| is false
    // (i.e. ReleaseLazyKeepaliveCountForView() was called).
    if (GetExtensionID(key_value.first) == extension_id &&
        key_value.second.has_keepalive) {
      const Extension* extension =
          GetExtensionForRenderFrameHost(key_value.first);
      if (extension)
        IncrementLazyKeepaliveCount(extension, Activity::PROCESS_MANAGER,
                                    Activity::kRenderFrame);
    }
  }
}

//
// IncognitoProcessManager
//

IncognitoProcessManager::IncognitoProcessManager(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ExtensionRegistry* extension_registry)
    : ProcessManager(incognito_context, original_context, extension_registry) {
  DCHECK(incognito_context->IsOffTheRecord());
}

bool IncognitoProcessManager::CreateBackgroundHost(const Extension* extension,
                                                   const GURL& url) {
  if (IncognitoInfo::IsSplitMode(extension)) {
    if (ExtensionsBrowserClient::Get()->IsExtensionIncognitoEnabled(
            extension->id(), browser_context()))
      return ProcessManager::CreateBackgroundHost(extension, url);
  } else {
    // Do nothing. If an extension is spanning, then its original-profile
    // background page is shared with incognito, so we don't create another.
  }
  return false;
}

scoped_refptr<content::SiteInstance>
IncognitoProcessManager::GetSiteInstanceForURL(const GURL& url) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetExtensionOrAppByURL(url);
  if (extension && !IncognitoInfo::IsSplitMode(extension)) {
    BrowserContext* original_context =
        ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
            browser_context(), /*force_guest_profile=*/true);
    return ProcessManager::Get(original_context)->GetSiteInstanceForURL(url);
  }

  return ProcessManager::GetSiteInstanceForURL(url);
}

}  // namespace extensions
