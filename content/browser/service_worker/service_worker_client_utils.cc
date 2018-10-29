// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_client_utils.h"

#include <algorithm>
#include <tuple>

#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"
#include "url/gurl.h"

namespace content {
namespace service_worker_client_utils {

namespace {

using OpenURLCallback = base::OnceCallback<void(int, int)>;

// The OpenURLObserver class is a WebContentsObserver that will wait for a
// WebContents to be initialized, run the |callback| passed to its constructor
// then self destroy.
// The callback will receive the process and frame ids. If something went wrong
// those will be (kInvalidUniqueID, MSG_ROUTING_NONE).
// The callback will be called in the IO thread.
class OpenURLObserver : public WebContentsObserver {
 public:
  OpenURLObserver(WebContents* web_contents,
                  int frame_tree_node_id,
                  OpenURLCallback callback)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(frame_tree_node_id),
        callback_(std::move(callback)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    DCHECK(web_contents());
    if (!navigation_handle->HasCommitted()) {
      // Return error.
      RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
      return;
    }

    if (navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
      // Return error.
      RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
      return;
    }

    RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    RunCallback(render_frame_host->GetProcess()->GetID(),
                render_frame_host->GetRoutingID());
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

  void WebContentsDestroyed() override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

 private:
  void RunCallback(int render_process_id, int render_frame_id) {
    // After running the callback, |this| will stop observing, thus
    // web_contents() should return nullptr and |RunCallback| should no longer
    // be called. Then, |this| will self destroy.
    DCHECK(web_contents());
    DCHECK(callback_);

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback_), render_process_id,
                       render_frame_id));
    Observe(nullptr);
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  int frame_tree_node_id_;
  OpenURLCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OpenURLObserver);
};

blink::mojom::ServiceWorkerClientInfoPtr GetWindowClientInfoOnUI(
    int render_process_id,
    int render_frame_id,
    base::TimeTicks create_time,
    const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return nullptr;

  // TODO(mlamouri,michaeln): it is possible to end up collecting information
  // for a frame that is actually being navigated and isn't exactly what we are
  // expecting.
  return blink::mojom::ServiceWorkerClientInfo::New(
      render_frame_host->GetLastCommittedURL(), client_uuid,
      blink::mojom::ServiceWorkerClientType::kWindow,
      render_frame_host->GetVisibilityState(), render_frame_host->IsFocused(),
      render_frame_host->GetParent()
          ? network::mojom::RequestContextFrameType::kNested
          : network::mojom::RequestContextFrameType::kTopLevel,
      render_frame_host->frame_tree_node()->last_focus_time(), create_time);
}

blink::mojom::ServiceWorkerClientInfoPtr FocusOnUI(
    int render_process_id,
    int render_frame_id,
    base::TimeTicks create_time,
    const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));

  if (!render_frame_host || !web_contents)
    return nullptr;

  FrameTreeNode* frame_tree_node = render_frame_host->frame_tree_node();

  // Focus the frame in the frame tree node, in case it has changed.
  frame_tree_node->frame_tree()->SetFocusedFrame(
      frame_tree_node, render_frame_host->GetSiteInstance());

  // Focus the frame's view to make sure the frame is now considered as focused.
  render_frame_host->GetView()->Focus();

  // Move the web contents to the foreground.
  web_contents->Activate();

  return GetWindowClientInfoOnUI(render_process_id, render_frame_id,
                                 create_time, client_uuid);
}

// This is only called for main frame navigations in OpenWindowOnUI().
void DidOpenURLOnUI(WindowType type,
                    OpenURLCallback callback,
                    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback), ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  // ContentBrowserClient::OpenURL calls ui::BaseWindow::Show which
  // makes the destination window the main+key window, but won't make Chrome
  // the active application (https://crbug.com/470830). Since OpenWindow is
  // always called from a user gesture (e.g. notification click), we should
  // explicitly activate the window, which brings Chrome to the front.
  static_cast<WebContentsImpl*>(web_contents)->Activate();

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  new OpenURLObserver(web_contents,
                      rfhi->frame_tree_node()->frame_tree_node_id(),
                      std::move(callback));

  if (type == WindowType::PAYMENT_HANDLER_WINDOW) {
    // Set the opened web_contents to payment app provider to manage its life
    // cycle.
    PaymentAppProvider::GetInstance()->SetOpenedWindow(web_contents);
  }
}

void OpenWindowOnUI(
    const GURL& url,
    const GURL& script_url,
    int worker_process_id,
    const scoped_refptr<ServiceWorkerContextWrapper>& context_wrapper,
    WindowType type,
    OpenURLCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      context_wrapper->storage_partition()
          ? context_wrapper->storage_partition()->browser_context()
          : nullptr;
  // We are shutting down.
  if (!browser_context)
    return;

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(worker_process_id);
  if (render_process_host->IsForGuestsOnly()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback), ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  OpenURLParams params(
      url,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, network::mojom::ReferrerPolicy::kDefault)),
      type == WindowType::PAYMENT_HANDLER_WINDOW
          ? WindowOpenDisposition::NEW_POPUP
          : WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true /* is_renderer_initiated */);
  params.open_app_window_if_possible = type == WindowType::NEW_TAB_WINDOW;

  GetContentClient()->browser()->OpenURL(
      browser_context, params,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&DidOpenURLOnUI, type, std::move(callback))));
}

void NavigateClientOnUI(const GURL& url,
                        const GURL& script_url,
                        int process_id,
                        int frame_id,
                        OpenURLCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfhi = RenderFrameHostImpl::FromID(process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfhi);

  if (!rfhi || !web_contents) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback), ChildProcessHost::kInvalidUniqueID,
                       MSG_ROUTING_NONE));
    return;
  }

  ui::PageTransition transition = rfhi->GetParent()
                                      ? ui::PAGE_TRANSITION_AUTO_SUBFRAME
                                      : ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();

  OpenURLParams params(
      url,
      Referrer::SanitizeForRequest(
          url, Referrer(script_url, network::mojom::ReferrerPolicy::kDefault)),
      frame_tree_node_id, WindowOpenDisposition::CURRENT_TAB, transition,
      true /* is_renderer_initiated */);
  web_contents->OpenURL(params);
  new OpenURLObserver(web_contents, frame_tree_node_id, std::move(callback));
}

void AddWindowClient(
    ServiceWorkerProviderHost* host,
    std::vector<std::tuple<int, int, base::TimeTicks, std::string>>*
        client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (host->client_type() != blink::mojom::ServiceWorkerClientType::kWindow)
    return;
  client_info->push_back(std::make_tuple(host->process_id(), host->frame_id(),
                                         host->create_time(),
                                         host->client_uuid()));
}

void AddNonWindowClient(
    const ServiceWorkerProviderHost* host,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    ServiceWorkerClientPtrs* out_clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::mojom::ServiceWorkerClientType host_client_type = host->client_type();
  if (host_client_type == blink::mojom::ServiceWorkerClientType::kWindow)
    return;
  if (options->client_type != blink::mojom::ServiceWorkerClientType::kAll &&
      options->client_type != host_client_type)
    return;

  auto client_info = blink::mojom::ServiceWorkerClientInfo::New(
      host->document_url(), host->client_uuid(), host_client_type,
      blink::mojom::PageVisibilityState::kHidden,
      false,  // is_focused
      network::mojom::RequestContextFrameType::kNone, base::TimeTicks(),
      host->create_time());
  out_clients->push_back(std::move(client_info));
}

void OnGetWindowClientsOnUI(
    // The tuple contains process_id, frame_id, create_time, client_uuid.
    const std::vector<std::tuple<int, int, base::TimeTicks, std::string>>&
        clients_info,
    const GURL& script_url,
    ClientsCallback callback,
    std::unique_ptr<ServiceWorkerClientPtrs> out_clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& it : clients_info) {
    blink::mojom::ServiceWorkerClientInfoPtr info = GetWindowClientInfoOnUI(
        std::get<0>(it), std::get<1>(it), std::get<2>(it), std::get<3>(it));

    // If the request to the provider_host returned a null
    // ServiceWorkerClientInfo, that means that it wasn't possible to associate
    // it with a valid RenderFrameHost. It might be because the frame was killed
    // or navigated in between.
    if (!info)
      continue;
    DCHECK(!info->client_uuid.empty());

    // We can get info for a frame that was navigating end ended up with a
    // different URL than expected. In such case, we should make sure to not
    // expose cross-origin WindowClient.
    if (info->url.GetOrigin() != script_url.GetOrigin())
      continue;

    out_clients->push_back(std::move(info));
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(callback), std::move(out_clients)));
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

void DidGetClients(ClientsCallback callback,
                   std::unique_ptr<ServiceWorkerClientPtrs> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::sort(clients->begin(), clients->end(), ServiceWorkerClientInfoSort());

  std::move(callback).Run(std::move(clients));
}

void GetNonWindowClients(
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    ClientsCallback callback,
    std::unique_ptr<ServiceWorkerClientPtrs> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!options->include_uncontrolled) {
    for (auto& controllee : controller->controllee_map())
      AddNonWindowClient(controllee.second, std::move(options), clients.get());
  } else if (controller->context()) {
    GURL origin = controller->script_url().GetOrigin();
    for (auto it = controller->context()->GetClientProviderHostIterator(
             origin, false /* include_reserved_clients */);
         !it->IsAtEnd(); it->Advance()) {
      AddNonWindowClient(it->GetProviderHost(), std::move(options),
                         clients.get());
    }
  }
  DidGetClients(std::move(callback), std::move(clients));
}

void DidGetWindowClients(
    const base::WeakPtr<ServiceWorkerVersion>& controller,
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    ClientsCallback callback,
    std::unique_ptr<ServiceWorkerClientPtrs> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (options->client_type == blink::mojom::ServiceWorkerClientType::kAll) {
    GetNonWindowClients(controller, std::move(options), std::move(callback),
                        std::move(clients));
    return;
  }
  DidGetClients(std::move(callback), std::move(clients));
}

void GetWindowClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                      blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                      ClientsCallback callback,
                      std::unique_ptr<ServiceWorkerClientPtrs> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(options->client_type ==
             blink::mojom::ServiceWorkerClientType::kWindow ||
         options->client_type == blink::mojom::ServiceWorkerClientType::kAll);

  std::vector<std::tuple<int, int, base::TimeTicks, std::string>> clients_info;
  if (!options->include_uncontrolled) {
    for (auto& controllee : controller->controllee_map())
      AddWindowClient(controllee.second, &clients_info);
  } else if (controller->context()) {
    GURL origin = controller->script_url().GetOrigin();
    for (auto it = controller->context()->GetClientProviderHostIterator(
             origin, false /* include_reserved_clients */);
         !it->IsAtEnd(); it->Advance()) {
      AddWindowClient(it->GetProviderHost(), &clients_info);
    }
  }

  if (clients_info.empty()) {
    DidGetWindowClients(controller, std::move(options), std::move(callback),
                        std::move(clients));
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OnGetWindowClientsOnUI, clients_info,
                     controller->script_url(),
                     base::BindOnce(&DidGetWindowClients, controller,
                                    std::move(options), std::move(callback)),
                     std::move(clients)));
}

}  // namespace

void FocusWindowClient(ServiceWorkerProviderHost* provider_host,
                       ClientCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(blink::mojom::ServiceWorkerClientType::kWindow,
            provider_host->client_type());
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&FocusOnUI, provider_host->process_id(),
                     provider_host->frame_id(), provider_host->create_time(),
                     provider_host->client_uuid()),
      std::move(callback));
}

void OpenWindow(const GURL& url,
                const GURL& script_url,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                WindowType type,
                NavigationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &OpenWindowOnUI, url, script_url, worker_process_id,
          base::WrapRefCounted(context->wrapper()), type,
          base::BindOnce(&DidNavigate, context, script_url.GetOrigin(),
                         std::move(callback))));
}

void NavigateClient(const GURL& url,
                    const GURL& script_url,
                    int process_id,
                    int frame_id,
                    const base::WeakPtr<ServiceWorkerContextCore>& context,
                    NavigationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &NavigateClientOnUI, url, script_url, process_id, frame_id,
          base::BindOnce(&DidNavigate, context, script_url.GetOrigin(),
                         std::move(callback))));
}

void GetClient(const ServiceWorkerProviderHost* provider_host,
               ClientCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  blink::mojom::ServiceWorkerClientType client_type =
      provider_host->client_type();
  DCHECK(client_type == blink::mojom::ServiceWorkerClientType::kWindow ||
         client_type == blink::mojom::ServiceWorkerClientType::kSharedWorker)
      << client_type;

  if (client_type == blink::mojom::ServiceWorkerClientType::kWindow) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GetWindowClientInfoOnUI, provider_host->process_id(),
                       provider_host->route_id(), provider_host->create_time(),
                       provider_host->client_uuid()),
        std::move(callback));
    return;
  }

  auto client_info = blink::mojom::ServiceWorkerClientInfo::New(
      provider_host->document_url(), provider_host->client_uuid(),
      provider_host->client_type(), blink::mojom::PageVisibilityState::kHidden,
      false,  // is_focused
      network::mojom::RequestContextFrameType::kNone, base::TimeTicks(),
      provider_host->create_time());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(callback), std::move(client_info)));
}

void GetClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                ClientsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto clients = std::make_unique<ServiceWorkerClientPtrs>();
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
                 const GURL& origin,
                 NavigationCallback callback,
                 int render_process_id,
                 int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort,
                            nullptr /* client_info */);
    return;
  }

  if (render_process_id == ChildProcessHost::kInvalidUniqueID &&
      render_frame_id == MSG_ROUTING_NONE) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed,
                            nullptr /* client_info */);
    return;
  }

  for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetClientProviderHostIterator(
               origin, false /* include_reserved_clients */);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GetWindowClientInfoOnUI, provider_host->process_id(),
                       provider_host->route_id(), provider_host->create_time(),
                       provider_host->client_uuid()),
        base::BindOnce(std::move(callback),
                       blink::ServiceWorkerStatusCode::kOk));
    return;
  }

  // If here, it means that no provider_host was found, in which case, the
  // renderer should still be informed that the window was opened.
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk,
                          nullptr /* client_info */);
}

}  // namespace service_worker_client_utils
}  // namespace content
