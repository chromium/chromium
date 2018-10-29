// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_ukm_helper.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/byte_stream_input_stream.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "services/network/public/cpp/resource_response.h"

namespace content {

struct DownloadResourceHandler::DownloadTabInfo {
  GURL tab_url;
  GURL tab_referrer_url;
  ukm::SourceId ukm_source_id;
};

namespace {

// Static function in order to prevent any accidental accesses to
// DownloadResourceHandler members from the UI thread.
static void StartOnUIThread(
    std::unique_ptr<download::DownloadCreateInfo> info,
    std::unique_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info,
    std::unique_ptr<ByteStreamReader> stream,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    const download::DownloadUrlParameters::OnStartedCallback& started_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);

  // Navigations don't have associated RenderFrameHosts. Get the SiteInstance
  // from the FrameTreeNode.
  if (!frame_host) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    if (frame_tree_node)
      frame_host = frame_tree_node->current_frame_host();
  }

  DownloadManager* download_manager = nullptr;
  if (frame_host) {
    download_manager = BrowserContext::GetDownloadManager(
        frame_host->GetProcess()->GetBrowserContext());
  }

  if (!download_manager || !frame_host) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    if (!started_cb.is_null())
      started_cb.Run(nullptr,
                     download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

    if (stream)
      download::GetDownloadTaskRunner()->DeleteSoon(FROM_HERE,
                                                    stream.release());
    return;
  }

  info->tab_url = tab_info->tab_url;
  info->tab_referrer_url = tab_info->tab_referrer_url;
  info->ukm_source_id = tab_info->ukm_source_id;
  info->site_url = frame_host->GetSiteInstance()->GetSiteURL();
  info->render_process_id = frame_host->GetProcess()->GetID();
  info->render_frame_id = frame_host->GetRoutingID();

  download_manager->StartDownload(
      std::move(info),
      std::make_unique<ByteStreamInputStream>(std::move(stream)), nullptr,
      started_cb);
}

void InitializeDownloadTabInfoOnUIThread(
    const DownloadRequestHandle& request_handle,
    DownloadResourceHandler::DownloadTabInfo* tab_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = request_handle.GetWebContents();
  if (web_contents) {
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_info->tab_url = entry->GetURL();
      tab_info->tab_referrer_url = entry->GetReferrer().url;

      tab_info->ukm_source_id = static_cast<WebContentsImpl*>(web_contents)
                                    ->GetUkmSourceIdForLastCommittedSource();
    }
  }
}

void DeleteOnUIThread(
    std::unique_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info) {}

void NavigateOnUIThread(
    const GURL& url,
    const std::vector<GURL> url_chain,
    const Referrer& referrer,
    bool has_user_gesture,
    const ResourceRequestInfo::WebContentsGetter& wc_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = wc_getter.Run();
  if (web_contents) {
    NavigationController::LoadURLParams params(url);
    params.has_user_gesture = has_user_gesture;
    params.referrer = referrer;
    params.redirect_chain = url_chain;
    web_contents->GetController().LoadURLWithParams(params);
  }
}

}  // namespace

DownloadResourceHandler::DownloadResourceHandler(
    net::URLRequest* request,
    const std::string& request_origin,
    download::DownloadSource download_source,
    bool follow_cross_origin_redirects)
    : ResourceHandler(request),
      tab_info_(new DownloadTabInfo()),
      follow_cross_origin_redirects_(follow_cross_origin_redirects),
      first_origin_(url::Origin::Create(request->url())),
      core_(request, this, false, request_origin, download_source) {
  // Do UI thread initialization for tab_info_ asap after
  // DownloadResourceHandler creation since the tab could be navigated
  // before StartOnUIThread gets called.  This is safe because deletion
  // will occur via PostTask() as well, which will serialized behind this
  // PostTask()
  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &InitializeDownloadTabInfoOnUIThread,
          DownloadRequestHandle(AsWeakPtr(),
                                request_info->GetWebContentsGetterForRequest()),
          tab_info_.get()));
}

DownloadResourceHandler::~DownloadResourceHandler() {
  if (tab_info_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&DeleteOnUIThread, std::move(tab_info_)));
  }
}

// static
std::unique_ptr<ResourceHandler> DownloadResourceHandler::Create(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<ResourceHandler> handler(new DownloadResourceHandler(
      request, std::string(), download::DownloadSource::NAVIGATION, true));
  return handler;
}

// static
std::unique_ptr<ResourceHandler> DownloadResourceHandler::CreateForNewRequest(
    net::URLRequest* request,
    const std::string& request_origin,
    download::DownloadSource download_source,
    bool follow_cross_origin_redirects) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<ResourceHandler> handler(new DownloadResourceHandler(
      request, request_origin, download_source, follow_cross_origin_redirects));
  return handler;
}

void DownloadResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  url::Origin new_origin(url::Origin::Create(redirect_info.new_url));
  if (!follow_cross_origin_redirects_ &&
      !first_origin_.IsSameOriginWith(new_origin)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &NavigateOnUIThread, redirect_info.new_url, request()->url_chain(),
            Referrer(GURL(redirect_info.new_referrer),
                     Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                         redirect_info.new_referrer_policy)),
            GetRequestInfo()->HasUserGesture(),
            GetRequestInfo()->GetWebContentsGetterForRequest()));
    controller->Cancel();
    return;
  }
  if (core_.OnRequestRedirected()) {
    controller->Resume();
  } else {
    controller->Cancel();
  }
}

// Send the download creation information to the download thread.
void DownloadResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  // The MIME type in ResourceResponse is the product of
  // MimeTypeResourceHandler.
  if (core_.OnResponseStarted(response->head.mime_type)) {
    controller->Resume();
  } else {
    controller->Cancel();
  }
}

void DownloadResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  controller->Resume();
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
void DownloadResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  if (!core_.OnWillRead(buf, buf_size)) {
    controller->Cancel();
    return;
  }

  controller->Resume();
}

// Pass the buffer to the download file writer.
void DownloadResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  bool defer = false;
  if (!core_.OnReadCompleted(bytes_read, &defer)) {
    controller->Cancel();
    return;
  }

  if (defer) {
    HoldController(std::move(controller));
  } else {
    controller->Resume();
  }
}

void DownloadResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  core_.OnResponseCompleted(status);
  controller->Resume();
}

void DownloadResourceHandler::PauseRequest() {
  core_.PauseRequest();
}

void DownloadResourceHandler::ResumeRequest() {
  core_.ResumeRequest();
}

void DownloadResourceHandler::OnStart(
    std::unique_ptr<download::DownloadCreateInfo> create_info,
    std::unique_ptr<ByteStreamReader> stream_reader,
    const download::DownloadUrlParameters::OnStartedCallback& callback) {
  // If the user cancels the download, then don't call start. Instead ignore the
  // download entirely.
  if (create_info->result ==
          download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED &&
      create_info->is_new_download) {
    if (!callback.is_null())
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(callback, nullptr, create_info->result));
    return;
  }

  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  create_info->has_user_gesture = request_info->HasUserGesture();
  create_info->transition_type = request_info->GetPageTransition();

  create_info->request_handle.reset(new DownloadRequestHandle(
      AsWeakPtr(), request_info->GetWebContentsGetterForRequest()));

  int render_process_id = -1;
  int render_frame_id = -1;
  request_info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&StartOnUIThread, std::move(create_info),
                     std::move(tab_info_), std::move(stream_reader),
                     render_process_id, render_frame_id,
                     request_info->frame_tree_node_id(), callback));
}

void DownloadResourceHandler::OnReadyToRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Resume();
}

void DownloadResourceHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const ResourceRequestInfoImpl* info = GetRequestInfo();
  ResourceDispatcherHostImpl::Get()->CancelRequest(
      info->GetChildID(),
      info->GetRequestID());
  // This object has been deleted.
}

std::string DownloadResourceHandler::DebugString() const {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  return base::StringPrintf("{"
                            " url_ = " "\"%s\""
                            " info = {"
                            " child_id = " "%d"
                            " request_id = " "%d"
                            " route_id = " "%d"
                            " }"
                            " }",
                            request() ?
                                request()->url().spec().c_str() :
                                "<NULL request>",
                            info->GetChildID(),
                            info->GetRequestID(),
                            info->GetRouteID());
}

}  // namespace content
