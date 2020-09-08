// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/renderer/loader/resource_dispatcher.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/public/renderer/request_peer.h"
#include "content/public/renderer/resource_dispatcher_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_load_stats.h"
#include "content/renderer/loader/sync_load_context.h"
#include "content/renderer/loader/sync_load_response.h"
#include "content/renderer/loader/url_loader_client_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {

namespace {

// Converts |time| from a remote to local TimeTicks, overwriting the original
// value.
void RemoteToLocalTimeTicks(
    const InterProcessTimeTicksConverter& converter,
    base::TimeTicks* time) {
  RemoteTimeTicks remote_time = RemoteTimeTicks::FromTimeTicks(*time);
  *time = converter.ToLocalTimeTicks(remote_time).ToTimeTicks();
}

void CheckSchemeForReferrerPolicy(const network::ResourceRequest& request) {
  if ((request.referrer_policy ==
           blink::ReferrerUtils::GetDefaultNetReferrerPolicy() ||
       request.referrer_policy ==
           net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE) &&
      request.referrer.SchemeIsCryptographic() &&
      !url::Origin::Create(request.url).opaque() &&
      !blink::network_utils::IsOriginSecure(request.url)) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "URL's Origin = "
               << url::Origin::Create(request.url).Serialize() << "\n"
               << "Referrer = " << request.referrer;
  }
}

int GetInitialRequestID() {
  // Starting with a random number speculatively avoids RDH_INVALID_REQUEST_ID
  // which are assumed to have been caused by restarting RequestID at 0 when
  // restarting a renderer after a crash - this would cause collisions if
  // requests from the previously crashed renderer are still active.  See
  // https://crbug.com/614281#c61 for more details about this hypothesis.
  //
  // To avoid increasing the likelyhood of overflowing the range of available
  // RequestIDs, kMax is set to a relatively low value of 2^20 (rather than
  // to something higher like 2^31).
  const int kMin = 0;
  const int kMax = 1 << 20;
  return base::RandInt(kMin, kMax);
}

// Determines if the loader should be restarted on a redirect using
// blink::ThrottlingURLLoader::FollowRedirectForcingRestart.
bool RedirectRequiresLoaderRestart(const GURL& original_url,
                                   const GURL& redirect_url) {
  // Restart is needed if the URL is no longer handled by network service.
  if (blink::network_utils::IsURLHandledByNetworkService(original_url))
    return !blink::network_utils::IsURLHandledByNetworkService(redirect_url);

  // If URL wasn't originally handled by network service, restart is needed if
  // schemes are different.
  return original_url.scheme_piece() != redirect_url.scheme_piece();
}

}  // namespace

// static
int ResourceDispatcher::MakeRequestID() {
  // NOTE: The resource_dispatcher_host also needs probably unique
  // request_ids, so they count down from -2 (-1 is a special "we're
  // screwed value"), while the renderer process counts up.
  static const int kInitialRequestID = GetInitialRequestID();
  static base::AtomicSequenceNumber sequence;
  return kInitialRequestID + sequence.GetNext();
}

ResourceDispatcher::ResourceDispatcher() : delegate_(nullptr) {}

ResourceDispatcher::~ResourceDispatcher() {
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return nullptr;
  return it->second.get();
}

void ResourceDispatcher::OnUploadProgress(int request_id,
                                          int64_t position,
                                          int64_t size) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnUploadProgress(position, size);
}

void ResourceDispatcher::OnReceivedResponse(
    int request_id,
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT0("loading", "ResourceDispatcher::OnReceivedResponse");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->local_response_start = base::TimeTicks::Now();
  request_info->remote_request_start = response_head->load_timing.request_start;
  // Now that response_start has been set, we can properly set the TimeTicks in
  // the URLResponseHead.
  ToLocalURLResponseHead(*request_info, *response_head);
  request_info->load_timing_info = response_head->load_timing;
  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnReceivedResponse(
        std::move(request_info->peer), response_head->mime_type,
        request_info->url);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  request_info->peer->OnReceivedResponse(response_head.Clone());
  if (!GetPendingRequestInfo(request_id))
    return;

  NotifyResourceResponseReceived(
      request_info->render_frame_id, request_info->resource_load_info.get(),
      std::move(response_head), request_info->previews_state);
}

void ResourceDispatcher::OnReceivedCachedMetadata(int request_id,
                                                  mojo_base::BigBuffer data) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (data.size()) {
    request_info->peer->OnReceivedCachedMetadata(std::move(data));
  }
}

void ResourceDispatcher::OnStartLoadingResponseBody(
    int request_id,
    mojo::ScopedDataPipeConsumerHandle body) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnStartLoadingResponseBody(std::move(body));
}

void ResourceDispatcher::OnReceivedRedirect(
    int request_id,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TRACE_EVENT0("loading", "ResourceDispatcher::OnReceivedRedirect");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  if (!request_info->url_loader && request_info->should_follow_redirect) {
    // This is a redirect that synchronously came as the loader is being
    // constructed, due to a URLLoaderThrottle that changed the starting
    // URL. Handle this in a posted task, as we don't have the loader
    // pointer yet.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ResourceDispatcher::OnReceivedRedirect,
                       weak_factory_.GetWeakPtr(), request_id, redirect_info,
                       std::move(response_head), task_runner));
    return;
  }

  request_info->local_response_start = base::TimeTicks::Now();
  request_info->remote_request_start = response_head->load_timing.request_start;
  request_info->redirect_requires_loader_restart =
      RedirectRequiresLoaderRestart(request_info->response_url,
                                    redirect_info.new_url);

  ToLocalURLResponseHead(*request_info, *response_head);
  std::vector<std::string> removed_headers;
  if (request_info->peer->OnReceivedRedirect(
          redirect_info, response_head.Clone(), &removed_headers)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info)
      return;
    // TODO(yoav): If request_info doesn't change above, we could avoid this
    // copy.
    request_info->removed_headers = removed_headers;

    request_info->response_url = redirect_info.new_url;
    request_info->has_pending_redirect = true;
    NotifyResourceRedirectReceived(request_info->render_frame_id,
                                   request_info->resource_load_info.get(),
                                   redirect_info, std::move(response_head));
    if (!request_info->is_deferred)
      FollowPendingRedirect(request_info);
  } else {
    Cancel(request_id, std::move(task_runner));
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    PendingRequestInfo* request_info) {
  if (request_info->has_pending_redirect &&
      request_info->should_follow_redirect) {
    request_info->has_pending_redirect = false;
    // net::URLRequest clears its request_start on redirect, so should we.
    request_info->local_request_start = base::TimeTicks::Now();
    // Redirect URL may not be handled by the network service, so force a
    // restart in case another URLLoaderFactory should handle the URL.
    if (request_info->redirect_requires_loader_restart) {
      request_info->url_loader->FollowRedirectForcingRestart();
    } else {
      request_info->url_loader->FollowRedirect(
          request_info->removed_headers, {} /* modified_headers */,
          {} /* modified_cors_exempt_headers */);
    }
  }
}

void ResourceDispatcher::OnRequestComplete(
    int request_id,
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0("loading", "ResourceDispatcher::OnRequestComplete");

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->net_error = status.error_code;

  NotifyResourceLoadCompleted(request_info->render_frame_id,
                              std::move(request_info->resource_load_info),
                              status);

  RequestPeer* peer = request_info->peer.get();

  if (delegate_) {
    delegate_->OnRequestComplete();
  }

  network::URLLoaderCompletionStatus renderer_status(status);
  if (status.completion_time.is_null()) {
    // No completion timestamp is provided, leave it as is.
  } else if (request_info->remote_request_start.is_null() ||
             request_info->load_timing_info.request_start.is_null()) {
    // We cannot convert the remote time to a local time, let's use the current
    // timestamp. This happens when
    //  - We get an error before OnReceivedRedirect or OnReceivedResponse is
    //    called, or
    //  - Somehow such a timestamp was missing in the LoadTimingInfo.
    renderer_status.completion_time = base::TimeTicks::Now();
  } else {
    // We have already converted the request start timestamp, let's use that
    // conversion information.
    // Note: We cannot create a InterProcessTimeTicksConverter with
    // (local_request_start, now, remote_request_start, remote_completion_time)
    // as that may result in inconsistent timestamps.
    renderer_status.completion_time =
        std::min(status.completion_time - request_info->remote_request_start +
                     request_info->load_timing_info.request_start,
                 base::TimeTicks::Now());
  }
  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call request_info->peer
  // but the past attempt to change it seems to have caused crashes.
  // (crbug.com/547047)
  peer->OnCompletedRequest(renderer_status);
}

bool ResourceDispatcher::RemovePendingRequest(
    int request_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PendingRequestInfo* info = it->second.get();
  if (info->net_error == net::ERR_IO_PENDING) {
    info->net_error = net::ERR_ABORTED;
    NotifyResourceLoadCanceled(info->render_frame_id,
                               std::move(info->resource_load_info),
                               info->net_error);
  }

  // Cancel loading.
  info->url_loader = nullptr;
  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  info->url_loader_client = nullptr;

  // Always delete the pending_request asyncly so that cancelling the request
  // doesn't delete the request context info while its response is still being
  // handled.
  task_runner->DeleteSoon(FROM_HERE, it->second.release());
  pending_requests_.erase(it);

  return true;
}

void ResourceDispatcher::Cancel(
    int request_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DLOG(ERROR) << "unknown request";
    return;
  }

  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  RemovePendingRequest(request_id, std::move(task_runner));
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  if (value) {
    request_info->is_deferred = value;
    request_info->url_loader_client->SetDefersLoading();
  } else if (request_info->is_deferred) {
    request_info->is_deferred = false;
    request_info->url_loader_client->UnsetDefersLoading();

    FollowPendingRedirect(request_info);
  }
}

void ResourceDispatcher::DidChangePriority(int request_id,
                                           net::RequestPriority new_priority,
                                           int intra_priority_value) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    DLOG(ERROR) << "unknown request";
    return;
  }

  request_info->url_loader->SetPriority(new_priority, intra_priority_value);
}

void ResourceDispatcher::OnTransferSizeUpdated(int request_id,
                                               int32_t transfer_size_diff) {
  DCHECK_GT(transfer_size_diff, 0);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  // TODO(yhirano): Consider using int64_t in
  // RequestPeer::OnTransferSizeUpdated.
  request_info->peer->OnTransferSizeUpdated(transfer_size_diff);
  if (!GetPendingRequestInfo(request_id))
    return;

  NotifyResourceTransferSizeUpdated(request_info->render_frame_id,
                                    request_info->resource_load_info.get(),
                                    transfer_size_diff);
}

void ResourceDispatcher::SetCorsExemptHeaderList(
    const std::vector<std::string>& list) {
  cors_exempt_header_list_ = list;
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo(
    std::unique_ptr<RequestPeer> peer,
    network::mojom::RequestDestination request_destination,
    int render_frame_id,
    const GURL& request_url)
    : peer(std::move(peer)),
      request_destination(request_destination),
      render_frame_id(render_frame_id),
      url(request_url),
      response_url(request_url),
      local_request_start(base::TimeTicks::Now()) {}

ResourceDispatcher::PendingRequestInfo::~PendingRequestInfo() {
}

void ResourceDispatcher::StartSync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    SyncLoadResponse* response,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    base::TimeDelta timeout,
    mojo::PendingRemote<blink::mojom::BlobRegistry> download_to_blob_registry,
    std::unique_ptr<RequestPeer> peer) {
  CheckSchemeForReferrerPolicy(*request);

  DCHECK(loader_options & network::mojom::kURLLoadOptionSynchronous);
  DCHECK(request->load_flags & net::LOAD_IGNORE_LIMITS);

  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory =
      url_loader_factory->Clone();
  base::WaitableEvent redirect_or_response_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Prepare the configured throttles for use on a separate thread.
  for (const auto& throttle : throttles)
    throttle->DetachFromCurrentSequence();

  // A task is posted to a separate thread to execute the request so that
  // this thread may block on a waitable event. It is safe to pass raw
  // pointers to |sync_load_response| and |event| as this stack frame will
  // survive until the request is complete.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncLoadContext::StartAsyncWithWaitableEvent, std::move(request),
          routing_id, task_runner, traffic_annotation, loader_options,
          std::move(pending_factory), std::move(throttles),
          base::Unretained(response),
          base::Unretained(&redirect_or_response_event),
          base::Unretained(terminate_sync_load_event_), timeout,
          std::move(download_to_blob_registry), cors_exempt_header_list_));

  // redirect_or_response_event will signal when each redirect completes, and
  // when the final response is complete.
  redirect_or_response_event.Wait();

  while (response->context_for_redirect) {
    DCHECK(response->redirect_info);
    bool follow_redirect = peer->OnReceivedRedirect(
        *response->redirect_info, response->head.Clone(),
        nullptr /* removed_headers */);
    redirect_or_response_event.Reset();
    if (follow_redirect) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&SyncLoadContext::FollowRedirect,
                         base::Unretained(response->context_for_redirect)));
    } else {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&SyncLoadContext::CancelRedirect,
                         base::Unretained(response->context_for_redirect)));
    }
    redirect_or_response_event.Wait();
  }
}

int ResourceDispatcher::StartAsync(
    std::unique_ptr<network::ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    uint32_t loader_options,
    std::unique_ptr<RequestPeer> peer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles) {
  CheckSchemeForReferrerPolicy(*request);

#if defined(OS_ANDROID)
  // Main frame shouldn't come here.
  DCHECK(!(request->is_main_frame &&
           blink::IsRequestDestinationFrame(request->destination)));
  if (request->has_user_gesture) {
    NotifyUpdateUserGestureCarryoverInfo(request->render_frame_id);
  }
#endif

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  pending_requests_[request_id] = std::make_unique<PendingRequestInfo>(
      std::move(peer), request->destination, request->render_frame_id,
      request->url);
  PendingRequestInfo* pending_request = pending_requests_[request_id].get();

  pending_request->resource_load_info = NotifyResourceLoadInitiated(
      request->render_frame_id, request_id, request->url, request->method,
      request->referrer, pending_request->request_destination,
      request->priority);

  pending_request->previews_state = request->previews_state;

  std::unique_ptr<URLLoaderClientImpl> client(new URLLoaderClientImpl(
      request_id, this, loading_task_runner,
      url_loader_factory->BypassRedirectChecks(), request->url));

  std::unique_ptr<blink::ThrottlingURLLoader> url_loader =
      blink::ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(url_loader_factory), std::move(throttles), routing_id,
          request_id, loader_options, request.get(), client.get(),
          traffic_annotation, std::move(loading_task_runner),
          base::make_optional(cors_exempt_header_list_));
  pending_request->url_loader = std::move(url_loader);
  pending_request->url_loader_client = std::move(client);

  return request_id;
}

void ResourceDispatcher::ToLocalURLResponseHead(
    const PendingRequestInfo& request_info,
    network::mojom::URLResponseHead& response_head) const {
  if (base::TimeTicks::IsConsistentAcrossProcesses() ||
      request_info.local_request_start.is_null() ||
      request_info.local_response_start.is_null() ||
      response_head.request_start.is_null() ||
      response_head.response_start.is_null() ||
      response_head.load_timing.request_start.is_null()) {
    return;
  }
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(request_info.local_request_start),
      LocalTimeTicks::FromTimeTicks(request_info.local_response_start),
      RemoteTimeTicks::FromTimeTicks(response_head.request_start),
      RemoteTimeTicks::FromTimeTicks(response_head.response_start));

  net::LoadTimingInfo* load_timing = &response_head.load_timing;
  RemoteToLocalTimeTicks(converter, &load_timing->request_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_end);
  RemoteToLocalTimeTicks(converter, &load_timing->send_start);
  RemoteToLocalTimeTicks(converter, &load_timing->send_end);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_start);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_end);
  RemoteToLocalTimeTicks(converter, &load_timing->push_start);
  RemoteToLocalTimeTicks(converter, &load_timing->push_end);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_start_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_ready_time);
  RemoteToLocalTimeTicks(converter, &load_timing->service_worker_fetch_start);
  RemoteToLocalTimeTicks(converter,
                         &load_timing->service_worker_respond_with_settled);
}

}  // namespace content
