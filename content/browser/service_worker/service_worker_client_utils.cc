// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_utils.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_security_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_visibility_state.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/40568315): Much of this file, which dealt with thread
// hops between UI and IO, can likely be simplified now that service worker code
// is on the UI thread.

namespace content {
namespace service_worker_client_utils {

namespace {

using OpenURLCallback = base::OnceCallback<void(GlobalRenderFrameHostId)>;

// The OpenURLObserver class is a WebContentsObserver that will wait for a
// WebContents to be initialized, run the |callback| passed to its constructor
// then self destroy.
// The callback will receive the GlobalRenderFrameHostId. If something went
// wrong it will have MSG_ROUTING_NONE.
class OpenURLObserver : public WebContentsObserver {
 public:
  OpenURLObserver(WebContents* web_contents,
                  FrameTreeNodeId frame_tree_node_id,
                  OpenURLCallback callback)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(frame_tree_node_id),
        callback_(std::move(callback)) {}

  OpenURLObserver(const OpenURLObserver&) = delete;
  OpenURLObserver& operator=(const OpenURLObserver&) = delete;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
      // This navigation is not for the frame this observer is interested in,
      // return and keeping observing.
      return;
    }

    if (!navigation_handle->HasCommitted()) {
      // Return error.
      RunCallback(GlobalRenderFrameHostId());
      return;
    }

    RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    RunCallback(render_frame_host->GetGlobalId());
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    RunCallback(GlobalRenderFrameHostId());
  }

  void WebContentsDestroyed() override {
    RunCallback(GlobalRenderFrameHostId());
  }

 private:
  void RunCallback(const GlobalRenderFrameHostId& rfh_id) {
    // After running the callback, |this| will stop observing, thus
    // web_contents() should return nullptr and |RunCallback| should no longer
    // be called. Then, |this| will self destroy.
    DCHECK(web_contents());
    DCHECK(callback_);

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    // TODO(falken): Does this need to be asynchronous?
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback_), rfh_id));
    Observe(nullptr);
    task_runner->DeleteSoon(FROM_HERE, this);
  }

  FrameTreeNodeId frame_tree_node_id_;
  OpenURLCallback callback_;
};

blink::mojom::ServiceWorkerClientInfoPtr GetWindowClientInfo(
    const GURL& creation_url,
    const GlobalRenderFrameHostId& rfh_id,
    base::TimeTicks create_time,
    const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* render_frame_host = RenderFrameHostImpl::FromID(rfh_id);
  if (!render_frame_host)
    return nullptr;

  // Treat items in backforward cache as not existing.
  if (render_frame_host->IsInBackForwardCache())
    return nullptr;

  // TODO(mlamouri,michaeln): it is possible to end up collecting information
  // for a frame that is actually being navigated and isn't exactly what we are
  // expecting.
  const auto& url = base::FeatureList::IsEnabled(
                        features::kServiceWorkerClientUrlIsCreationUrl)
                        ? creation_url
                        : render_frame_host->GetLastCommittedURL();

  PageVisibilityState visibility = render_frame_host->GetVisibilityState();
  bool page_hidden = visibility != PageVisibilityState::kVisible;
  return blink::mojom::ServiceWorkerClientInfo::New(
      url,
      render_frame_host->GetParent() && !render_frame_host->IsFencedFrameRoot()
          ? blink::mojom::RequestContextFrameType::kNested
          : blink::mojom::RequestContextFrameType::kTopLevel,
      client_uuid, blink::mojom::ServiceWorkerClientType::kWindow, page_hidden,
      render_frame_host->IsFocused(),
      render_frame_host->IsFrozen()
          ? blink::mojom::ServiceWorkerClientLifecycleState::kFrozen
          : blink::mojom::ServiceWorkerClientLifecycleState::kActive,
      render_frame_host->frame_tree_node()->last_focus_time(), create_time);
}

// This is only called for main frame navigations in OpenWindow().
void DidOpenURL(OpenURLCallback callback, WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents) {
    std::move(callback).Run(GlobalRenderFrameHostId());
    return;
  }

  // ContentBrowserClient::OpenURL calls ui::BaseWindow::Show which
  // makes the destination window the main+key window, but won't make Chrome
  // the active application (https://crbug.com/470830). Since OpenWindow is
  // always called from a user gesture (e.g. notification click), we should
  // explicitly activate the window, which brings Chrome to the front.
  static_cast<WebContentsImpl*>(web_contents)->Activate();

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  new OpenURLObserver(web_contents,
                      rfhi->frame_tree_node()->frame_tree_node_id(),
                      std::move(callback));
}

void AddWindowClient(
    const ServiceWorkerClient& service_worker_client,
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr>* clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_worker_client.IsContainerForWindowClient()) {
    return;
  }
  if (!service_worker_client.is_execution_ready()) {
    return;
  }

  blink::mojom::ServiceWorkerClientInfoPtr info = GetWindowClientInfo(
      service_worker_client.creation_url(),
      service_worker_client.GetRenderFrameHostId(),
      service_worker_client.create_time(), service_worker_client.client_uuid());

  // If the request to the service_worker_client returned a null
  // ServiceWorkerClientInfo, that means that it wasn't possible to associate
  // it with a valid RenderFrameHost. It might be because the frame was killed
  // or navigated in between.
  if (!info) {
    return;
  }
  DCHECK(!info->client_uuid.empty());

  auto* rfh =
      RenderFrameHostImpl::FromID(service_worker_client.GetRenderFrameHostId());
  // Workaround to avoid referring the last committed origin before the render
  // frame host initialize it. It is known to be updated when the last committed
  // url is updated in DidNavigate(), we assume the invalid last committed URL
  // means that the invalid last committed origin.
  //
  // See: crbug.com/396502398#comment19
  if (rfh->GetLastCommittedURL().is_empty()) {
    // TODO(crbug.com/396502398): Add a test for this.
    // The test is not trivial because the case only happens in the timing
    // between CommitNavigation() and DidNavigate(), though.
    return;
  }

  // TODO(crbug.com/385901567): Investigate/clarify the intention of this
  // check.
  // We can get info for a frame that was navigating and ended up with a
  // different URL than expected. In such case, we should make sure to not
  // expose cross-origin WindowClient.
  // Note that script_url is the service worker script URL, and so we are
  // calculating the origin of the service worker. A service worker can only be
  // registered from same origin frames and it controls same origin frames (not
  // sandboxed ones). The following CHECK is ensuring that same origin
  // relationship between the origin of the service worker script and frames.
  const url::Origin controller_origin =
      url::Origin::Create(controller->script_url());
  if (!controller_origin.IsSameOriginWith(rfh->GetLastCommittedOrigin())) {
    SCOPED_CRASH_KEY_STRING256("AddWindowClient", "ctrler_origin",
                               controller_origin.GetURL().spec());
    SCOPED_CRASH_KEY_STRING256("AddWindowClient", "rfh_origin",
                               rfh->GetLastCommittedOrigin().GetURL().spec());
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  clients->push_back(std::move(info));
}

void AddNonWindowClient(
    const ServiceWorkerClient& service_worker_client,
    blink::mojom::ServiceWorkerClientType client_type,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr>* out_clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_client.GetClientType() ==
      blink::mojom::ServiceWorkerClientType::kWindow) {
    return;
  }
  if (client_type != blink::mojom::ServiceWorkerClientType::kAll &&
      client_type != service_worker_client.GetClientType()) {
    return;
  }
  if (!service_worker_client.is_execution_ready()) {
    return;
  }

  const auto& url = base::FeatureList::IsEnabled(
                        features::kServiceWorkerClientUrlIsCreationUrl)
                        ? service_worker_client.creation_url()
                        : service_worker_client.url();

  // TODO(dtapuska): Need to get frozen state for dedicated workers from
  // DedicatedWorkerHost. crbug.com/968417
  auto client_info = blink::mojom::ServiceWorkerClientInfo::New(
      url, blink::mojom::RequestContextFrameType::kNone,
      service_worker_client.client_uuid(),
      service_worker_client.GetClientType(),
      /*page_hidden=*/true,
      /*is_focused=*/false,
      blink::mojom::ServiceWorkerClientLifecycleState::kActive,
      base::TimeTicks(), service_worker_client.create_time());
  out_clients->push_back(std::move(client_info));

  if (service_worker_client.GetClientType() ==
      blink::mojom::ServiceWorkerClientType::kSharedWorker) {
    // This is recorded per the SharedWorker client.
    base::UmaHistogramBoolean(
        "ServiceWorker.AddNonWindowClient.SharedWorkerScript.IsBlob",
        url.SchemeIsBlob());
  }
}

struct ServiceWorkerClientInfoSort {
  bool operator()(const blink::mojom::ServiceWorkerClientInfoPtr& a,
                  const blink::mojom::ServiceWorkerClientInfoPtr& b) const {
    // Clients for windows should be appeared earlier.
    if (a->client_type == blink::mojom::ServiceWorkerClientType::kWindow &&
        b->client_type != blink::mojom::ServiceWorkerClientType::kWindow) {
      return true;
    }
    if (a->client_type != blink::mojom::ServiceWorkerClientType::kWindow &&
        b->client_type == blink::mojom::ServiceWorkerClientType::kWindow) {
      return false;
    }

    // Clients focused recently should be appeared earlier.
    if (a->last_focus_time != b->last_focus_time)
      return a->last_focus_time > b->last_focus_time;

    // Clients created before should be appeared earlier.
    return a->creation_time < b->creation_time;
  }
};

void DidGetClients(
    blink::mojom::ServiceWorkerHost::GetClientsCallback callback,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::sort(clients.begin(), clients.end(), ServiceWorkerClientInfoSort());

  std::move(callback).Run(std::move(clients));
}

void GetNonWindowClients(
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    blink::mojom::ServiceWorkerHost::GetClientsCallback callback,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (options->include_uncontrolled) {
    if (controller->context()) {
      for (auto it =
               controller->context()
                   ->service_worker_client_owner()
                   .GetServiceWorkerClients(
                       controller->key(), false /* include_reserved_clients */,
                       false /* include_back_forward_cached_clients */);
           !it.IsAtEnd(); ++it) {
        AddNonWindowClient(*it, options->client_type, &clients);
      }
    }
  } else {
    for (const auto& controllee : controller->controllee_map()) {
      AddNonWindowClient(*controllee.second, options->client_type, &clients);
    }
  }
  if (controller->script_url().SchemeIs("chrome-extension")) {
    base::UmaHistogramCounts1000(
        "ServiceWorker.GetClients.ExtensionController.AllClients",
        clients.size());
  }
  DidGetClients(std::move(callback), std::move(clients));
}

void DidGetWindowClients(
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    blink::mojom::ServiceWorkerHost::GetClientsCallback callback,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (options->client_type == blink::mojom::ServiceWorkerClientType::kAll) {
    GetNonWindowClients(controller, std::move(options), std::move(callback),
                        std::move(clients));
    return;
  }
  if (controller->script_url().SchemeIs("chrome-extension")) {
    base::UmaHistogramCounts1000(
        "ServiceWorker.GetClients.ExtensionController.WindowClients",
        clients.size());
  }
  DidGetClients(std::move(callback), std::move(clients));
}

void GetWindowClients(
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    blink::mojom::ServiceWorkerHost::GetClientsCallback callback,
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(options->client_type ==
             blink::mojom::ServiceWorkerClientType::kWindow ||
         options->client_type == blink::mojom::ServiceWorkerClientType::kAll);

  if (options->include_uncontrolled) {
    if (controller->context()) {
      for (auto it =
               controller->context()
                   ->service_worker_client_owner()
                   .GetServiceWorkerClients(
                       controller->key(), false /* include_reserved_clients */,
                       false /* include_back_forward_cached_clients */);
           !it.IsAtEnd(); ++it) {
        AddWindowClient(*it, controller, &clients);
      }
    }
  } else {
    for (const auto& controllee : controller->controllee_map()) {
      AddWindowClient(*controllee.second, controller, &clients);
    }
  }

  DidGetWindowClients(controller, std::move(options), std::move(callback),
                      std::move(clients));
}

void DidGetExecutionReadyClient(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    const std::string& client_uuid,
    const GURL& script_url,
    const blink::StorageKey& key,
    NavigationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr /* client_info */);
    return;
  }

  ServiceWorkerClient* service_worker_client =
      context->service_worker_client_owner().GetServiceWorkerClientByClientID(
          client_uuid);
  if (!service_worker_client || !service_worker_client->is_execution_ready()) {
    // The page was destroyed before it became execution ready.  Tell the
    // renderer the page opened but it doesn't have access to it.
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                            nullptr /* client_info */);
    return;
  }

  // In a scenario where "--disable-web-security" is specified the |script_url|
  // may be cross-origin
  CHECK_EQ(
      service_worker_security_utils::GetCorrectStorageKeyForWebSecurityState(
          service_worker_client->key(), script_url),
      key);

  blink::mojom::ServiceWorkerClientInfoPtr info =
      GetWindowClientInfo(service_worker_client->creation_url(),
                          service_worker_client->GetRenderFrameHostId(),
                          service_worker_client->create_time(),
                          service_worker_client->client_uuid());
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk, std::move(info));
}

}  // namespace

void FocusWindowClient(
    ServiceWorkerClient* service_worker_client,
    blink::mojom::ServiceWorkerHost::FocusClientCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_worker_client->IsContainerForWindowClient());

  GlobalRenderFrameHostId rfh_id =
      service_worker_client->GetRenderFrameHostId();
  auto* render_frame_host = RenderFrameHostImpl::FromID(rfh_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));

  if (!render_frame_host || !web_contents) {
    auto result = blink::mojom::FocusResult::NewErrorCode(
        blink::mojom::FocusError::CLIENT_NOT_FOUND);
    std::move(callback).Run(std::move(result));
    return;
  }

  if (!render_frame_host->IsActive()) {
    auto result = blink::mojom::FocusResult::NewErrorCode(
        blink::mojom::FocusError::CLIENT_INACTIVE);
    std::move(callback).Run(std::move(result));
    return;
  }

  FrameTreeNode* frame_tree_node = render_frame_host->frame_tree_node();

  // Focus the frame in the frame tree node, in case it has changed.
  frame_tree_node->frame_tree().SetFocusedFrame(
      frame_tree_node, render_frame_host->GetSiteInstance()->group());

  // Focus the frame's view to make sure the frame is now considered as focused.
  render_frame_host->GetView()->Focus();

  // Move the web contents to the foreground.
  web_contents->Activate();

  blink::mojom::ServiceWorkerClientInfoPtr info =
      GetWindowClientInfo(service_worker_client->creation_url(), rfh_id,
                          service_worker_client->create_time(),
                          service_worker_client->client_uuid());
  auto result = blink::mojom::FocusResult::NewClient(std::move(info));
  std::move(callback).Run(std::move(result));
}

void OpenWindow(const GURL& url,
                const GURL& script_url,
                const blink::StorageKey& key,
                int worker_id,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                WindowType type,
                NavigationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(worker_process_id);
  if (render_process_host->IsForGuestsOnly()) {
    DidNavigate(context, script_url, key, std::move(callback),
                GlobalRenderFrameHostId());
    return;
  }

  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper =
      base::WrapRefCounted(context->wrapper());
  SiteInstance* site_instance =
      context_wrapper->process_manager()->GetSiteInstanceForWorker(worker_id);
  if (!site_instance) {
    // Worker isn't running anymore. Fail.
    DidNavigate(context, script_url, key, std::move(callback),
                GlobalRenderFrameHostId());
    return;
  }

  // The following code is a rough copy of Navigator::RequestOpenURL. That
  // function can't be used directly since there is no RenderFrameHost yet
  // that the navigation will occur in.

  OpenURLParams params(
      url,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, network::mojom::ReferrerPolicy::kDefault)),
      type == WindowType::PAYMENT_HANDLER_WINDOW
          ? WindowOpenDisposition::NEW_POPUP
          : WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true /* is_renderer_initiated */);
  // TODO(https://crbug.com/382542907): Remove `open_pwa_window_if_possible` or
  // make it IWA-specific.
  params.open_app_window_if_possible = type == WindowType::NEW_TAB_WINDOW;
  params.initiator_origin =
      url::Origin::Create(script_url.DeprecatedGetOriginAsURL());

  // End of RequestOpenURL copy.

  GetContentClient()->browser()->OpenURL(
      site_instance, params,
      base::BindOnce(&DidOpenURL,
                     base::BindOnce(&DidNavigate, context, script_url, key,
                                    std::move(callback))));
}

void NavigateClient(const GURL& url,
                    const GURL& script_url,
                    const blink::StorageKey& key,
                    const GlobalRenderFrameHostId& rfh_id,
                    const base::WeakPtr<ServiceWorkerContextCore>& context,
                    NavigationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfhi = RenderFrameHostImpl::FromID(rfh_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfhi);

  if (!rfhi || !web_contents) {
    DidNavigate(context, script_url, key, std::move(callback),
                GlobalRenderFrameHostId());
    return;
  }

  // Prerendered main frames are not allowed to navigate after their initial
  // navigation. We can't proceed with the navigation and rely on the usual
  // mechanism to disallow (PrerenderNavigationThrottle), because
  // RequestOpenURL() crashes if called by a prerendering main frame.
  if (rfhi->frame_tree_node()->GetFrameType() ==
      FrameType::kPrerenderMainFrame) {
    DidNavigate(context, script_url, key, std::move(callback),
                GlobalRenderFrameHostId());
    return;
  }

  // Reject the navigate() call if there is an ongoing browser-initiated
  // navigation. Not rejecting it would allow websites to prevent the user from
  // navigating away. See https://crbug.com/930154.
  NavigationRequest* ongoing_navigation_request =
      rfhi->frame_tree()->root()->navigation_request();
  if (ongoing_navigation_request &&
      ongoing_navigation_request->browser_initiated()) {
    DidNavigate(context, script_url, key, std::move(callback),
                GlobalRenderFrameHostId());
    return;
  }

  FrameTreeNodeId frame_tree_node_id =
      rfhi->frame_tree_node()->frame_tree_node_id();
  Navigator& navigator = rfhi->frame_tree_node()->navigator();
  // Service workers don't have documents, so it's ok to use nullopt for
  // `initiator_base_url` in the following call.
  navigator.RequestOpenURL(
      rfhi, url, nullptr /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      url::Origin::Create(script_url), /* initiator_base_url= */ std::nullopt,
      nullptr /* post_body */, std::string() /* extra_headers */,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, network::mojom::ReferrerPolicy::kDefault)),
      WindowOpenDisposition::CURRENT_TAB,
      false /* should_replace_current_entry */, false /* user_gesture */,
      blink::mojom::TriggeringEventInfo::kUnknown,
      std::string() /* href_translate */, nullptr /* blob_url_loader_factory */,
      std::nullopt, false /* has_rel_opener */);
  new OpenURLObserver(web_contents, frame_tree_node_id,
                      base::BindOnce(&DidNavigate, context, script_url, key,
                                     std::move(callback)));
}

void GetClient(ServiceWorkerClient* service_worker_client,
               ClientCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_worker_client->GetClientType() ==
      blink::mojom::ServiceWorkerClientType::kWindow) {
    blink::mojom::ServiceWorkerClientInfoPtr info =
        GetWindowClientInfo(service_worker_client->creation_url(),
                            service_worker_client->GetRenderFrameHostId(),
                            service_worker_client->create_time(),
                            service_worker_client->client_uuid());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(info)));
    return;
  }

  {
    const auto& url = base::FeatureList::IsEnabled(
                          features::kServiceWorkerClientUrlIsCreationUrl)
                          ? service_worker_client->creation_url()
                          : service_worker_client->url();

    // TODO(dtapuska): Need to get frozen state for dedicated workers from
    // DedicatedWorkerHost. crbug.com/968417
    auto client_info = blink::mojom::ServiceWorkerClientInfo::New(
        url, blink::mojom::RequestContextFrameType::kNone,
        service_worker_client->client_uuid(),
        service_worker_client->GetClientType(),
        /*page_hidden=*/true,
        /*is_focused=*/false,
        blink::mojom::ServiceWorkerClientLifecycleState::kActive,
        base::TimeTicks(), service_worker_client->create_time());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(client_info)));
    if (service_worker_client->GetClientType() ==
        blink::mojom::ServiceWorkerClientType::kSharedWorker) {
      base::UmaHistogramBoolean(
          "ServiceWorker.GetClient.SharedWorkerScript.IsBlob",
          url.SchemeIsBlob());
    }
  }
}

void GetClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                blink::mojom::ServiceWorkerHost::GetClientsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto clients = std::vector<blink::mojom::ServiceWorkerClientInfoPtr>();
  if (!controller->HasControllee() && !options->include_uncontrolled) {
    DidGetClients(std::move(callback), std::move(clients));
    return;
  }

  // For Window clients we want to query the info on the UI thread first.
  if (options->client_type == blink::mojom::ServiceWorkerClientType::kWindow ||
      options->client_type == blink::mojom::ServiceWorkerClientType::kAll) {
    GetWindowClients(controller, std::move(options), std::move(callback),
                     std::move(clients));
    return;
  }

  GetNonWindowClients(controller, std::move(options), std::move(callback),
                      std::move(clients));
}

void DidNavigate(const base::WeakPtr<ServiceWorkerContextCore>& context,
                 const GURL& script_url,
                 const blink::StorageKey& key,
                 NavigationCallback callback,
                 GlobalRenderFrameHostId rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr /* client_info */);
    return;
  }

  if (!rfh_id) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed,
                            nullptr /* client_info */);
    return;
  }

  for (auto it = context->service_worker_client_owner().GetServiceWorkerClients(
           key, true /* include_reserved_clients */,
           false /* include_back_forward_cached_clients */);
       !it.IsAtEnd(); ++it) {
    if (!it->IsContainerForWindowClient()) {
      continue;
    }

    if (it->GetRenderFrameHostId() != rfh_id) {
      continue;
    }

    // DidNavigate must be called with a preparation complete client (the
    // navigation was committed), but the client might not be execution ready
    // yet (Blink hasn't yet created the Document).
    DCHECK(it->is_response_committed());
    if (!it->is_execution_ready()) {
      it->AddExecutionReadyCallback(base::BindOnce(
          &DidGetExecutionReadyClient, context, it->client_uuid(), script_url,
          key, std::move(callback)));
      return;
    }

    DidGetExecutionReadyClient(context, it->client_uuid(), script_url, key,
                               std::move(callback));
    return;
  }

  // If here, it means that no service_worker_client was found, in which case,
  // the renderer should still be informed that the window was opened.
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                          nullptr /* client_info */);
}

}  // namespace service_worker_client_utils
}  // namespace content
