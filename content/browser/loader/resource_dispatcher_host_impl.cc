// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/browser/loader/resource_dispatcher_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/bad_message.h"
#include "content/browser/browsing_data/clear_site_data_throttle.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/cross_site_document_resource_handler.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/intercepting_resource_handler.h"
#include "content/browser/loader/loader_delegate.h"
#include "content/browser/loader/mime_sniffing_resource_handler.h"
#include "content/browser/loader/mojo_async_resource_handler.h"
#include "content/browser/loader/null_resource_controller.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/browser/loader/stream_resource_handler.h"
#include "content/browser/loader/throttling_resource_handler.h"
#include "content/browser/loader/upload_data_stream_builder.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/net/url_request_service_worker_data.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/resource_type.h"
#include "net/base/auth.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/upload_data_stream.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "services/network/resource_scheduler.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/url_loader_factory.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

// ----------------------------------------------------------------------------

namespace content {

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using storage::ShareableFileReference;

namespace {

static ResourceDispatcherHostImpl* g_resource_dispatcher_host;

// The interval for calls to ResourceDispatcherHostImpl::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 250;

// Maximum byte "cost" of all the outstanding requests for a renderer.
// See declaration of |max_outstanding_requests_cost_per_process_| for details.
// This bound is 25MB, which allows for around 6000 outstanding requests.
const int kMaxOutstandingRequestsCostPerProcess = 26214400;

// The number of milliseconds after noting a user gesture that we will
// tag newly-created URLRequest objects with the
// net::LOAD_MAYBE_USER_GESTURE load flag. This is a fairly arbitrary
// guess at how long to expect direct impact from a user gesture, but
// this should be OK as the load flag is a best-effort thing only,
// rather than being intended as fully accurate.
const int kUserGestureWindowMs = 3500;

// Ratio of |max_num_in_flight_requests_| that any one renderer is allowed to
// use. Arbitrarily chosen.
const double kMaxRequestsPerProcessRatio = 0.45;

// Aborts a request before an URLRequest has actually been created.
void AbortRequestBeforeItStarts(
    IPC::Sender* sender,
    int request_id,
    network::mojom::URLLoaderClientPtr url_loader_client) {
  // Tell the renderer that this request was disallowed.
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_ABORTED;
  status.exists_in_cache = false;
  // No security info needed, connection not established.
  status.completion_time = base::TimeTicks();
  status.encoded_data_length = 0;
  status.encoded_body_length = 0;
  url_loader_client->OnComplete(status);
}

bool ValidatePluginChildId(int plugin_child_id) {
  if (plugin_child_id == ChildProcessHost::kInvalidUniqueID)
    return true;

#if BUILDFLAG(ENABLE_PLUGINS)
  // TODO(nick): These checks could be stricter, since they enforce only that
  // |plugin_child_id| is a valid plugin process, and not that it has a plugin
  // instance owned by the renderer process making the resource request. Fix
  // this by eliminating |plugin_child_id| altogether, and stop proxying plugin
  // URL requests through the renderer (https://crbug.com/778711).
  auto* plugin_host = BrowserChildProcessHost::FromID(plugin_child_id);
  if (plugin_host) {
    int process_type = plugin_host->GetData().process_type;
    if (process_type == PROCESS_TYPE_PPAPI_PLUGIN) {
      return true;
    } else if (process_type >= PROCESS_TYPE_CONTENT_END) {
      if (GetContentClient()->browser()->GetExternalBrowserPpapiHost(
              plugin_child_id) != nullptr) {
        return true;
      }
    }
  }
#endif
  return false;
}

// Used to log the cache flags for back-forward navigation requests.
// Because this enum is used to back a histogrma, DO NOT REMOVE OR RENAME VALUES
// in this enum. Instead, add a new one at the end.
// TODO(clamy): Remove this once we know the reason behind PlzNavigate's
// regression on PLT for back forward navigations.
enum HistogramCacheFlag {
  HISTOGRAM_VALIDATE_CACHE,
  HISTOGRAM_BYPASS_CACHE,
  HISTOGRAM_SKIP_CACHE_VALIDATION,
  HISTOGRAM_ONLY_FROM_CACHE,
  HISTOGRAM_DISABLE_CACHE,
  HISTOGRAM_CACHE_FLAG_MAX = HISTOGRAM_DISABLE_CACHE,
};

void RecordCacheFlags(HistogramCacheFlag flag) {
  UMA_HISTOGRAM_ENUMERATION("Navigation.BackForward.CacheFlags", flag,
                            HISTOGRAM_CACHE_FLAG_MAX);
}

void LogBackForwardNavigationFlagsHistogram(int load_flags) {
  if (load_flags & net::LOAD_VALIDATE_CACHE)
    RecordCacheFlags(HISTOGRAM_VALIDATE_CACHE);

  if (load_flags & net::LOAD_BYPASS_CACHE)
    RecordCacheFlags(HISTOGRAM_BYPASS_CACHE);

  if (load_flags & net::LOAD_SKIP_CACHE_VALIDATION)
    RecordCacheFlags(HISTOGRAM_SKIP_CACHE_VALIDATION);

  if (load_flags & net::LOAD_ONLY_FROM_CACHE)
    RecordCacheFlags(HISTOGRAM_ONLY_FROM_CACHE);

  if (load_flags & net::LOAD_DISABLE_CACHE)
    RecordCacheFlags(HISTOGRAM_DISABLE_CACHE);
}

}  // namespace

class ResourceDispatcherHostImpl::ScheduledResourceRequestAdapter final
    : public ResourceThrottle {
 public:
  explicit ScheduledResourceRequestAdapter(
      std::unique_ptr<network::ResourceScheduler::ScheduledResourceRequest>
          request)
      : request_(std::move(request)) {
    request_->set_resume_callback(base::BindOnce(
        &ScheduledResourceRequestAdapter::Resume, base::Unretained(this)));
  }
  ~ScheduledResourceRequestAdapter() override {}

  // ResourceThrottle implementation
  void WillStartRequest(bool* defer) override {
    request_->WillStartRequest(defer);
  }
  const char* GetNameForLogging() const override { return "ResourceScheduler"; }

 private:
  std::unique_ptr<network::ResourceScheduler::ScheduledResourceRequest>
      request_;
};

ResourceDispatcherHostImpl::LoadInfo::LoadInfo() {}
ResourceDispatcherHostImpl::LoadInfo::LoadInfo(const LoadInfo& other) = default;
ResourceDispatcherHostImpl::LoadInfo::~LoadInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::HeaderInterceptorInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::~HeaderInterceptorInfo() {}

ResourceDispatcherHostImpl::HeaderInterceptorInfo::HeaderInterceptorInfo(
    const HeaderInterceptorInfo& other) {}

// static
ResourceDispatcherHost* ResourceDispatcherHost::Get() {
  return g_resource_dispatcher_host;
}

ResourceDispatcherHostImpl::ResourceDispatcherHostImpl(
    CreateDownloadHandlerIntercept download_handler_intercept,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner,
    bool enable_resource_scheduler)
    : request_id_(-1),
      is_shutdown_(false),
      enable_resource_scheduler_(enable_resource_scheduler),
      num_in_flight_requests_(0),
      max_num_in_flight_requests_(base::SharedMemory::GetHandleLimit()),
      max_num_in_flight_requests_per_process_(static_cast<int>(
          max_num_in_flight_requests_ * kMaxRequestsPerProcessRatio)),
      max_outstanding_requests_cost_per_process_(
          kMaxOutstandingRequestsCostPerProcess),
      delegate_(nullptr),
      loader_delegate_(nullptr),
      allow_cross_origin_auth_prompt_(false),
      create_download_handler_intercept_(download_handler_intercept),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_thread_task_runner_(io_thread_runner),
      weak_factory_on_io_(this) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!g_resource_dispatcher_host);
  g_resource_dispatcher_host = this;

  ANNOTATE_BENIGN_RACE(
      &last_user_gesture_time_,
      "We don't care about the precise value, see http://crbug.com/92889");

  io_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ResourceDispatcherHostImpl::OnInit,
                                base::Unretained(this)));

  update_load_info_timer_ = std::make_unique<base::OneShotTimer>();
}

// The default ctor is only used by unittests. It is reasonable to assume that
// the main thread and the IO thread are the same for unittests.
ResourceDispatcherHostImpl::ResourceDispatcherHostImpl()
    : ResourceDispatcherHostImpl(CreateDownloadHandlerIntercept(),
                                 base::ThreadTaskRunnerHandle::Get(),
                                 /* enable_resource_scheduler */ true) {}

ResourceDispatcherHostImpl::~ResourceDispatcherHostImpl() {
  DCHECK(outstanding_requests_stats_map_.empty());
  DCHECK(g_resource_dispatcher_host);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  g_resource_dispatcher_host = nullptr;
}

// static
ResourceDispatcherHostImpl* ResourceDispatcherHostImpl::Get() {
  return g_resource_dispatcher_host;
}

void ResourceDispatcherHostImpl::SetDelegate(
    ResourceDispatcherHostDelegate* delegate) {
  delegate_ = delegate;
}

void ResourceDispatcherHostImpl::SetAllowCrossOriginAuthPrompt(bool value) {
  allow_cross_origin_auth_prompt_ = value;
}

void ResourceDispatcherHostImpl::CancelRequestsForContext(
    ResourceContext* context) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(context);

  // Note that request cancellation has side effects. Therefore, we gather all
  // the requests to cancel first, and then we start cancelling. We assert at
  // the end that there are no more to cancel since the context is about to go
  // away.
  typedef std::vector<std::unique_ptr<ResourceLoader>> LoaderList;
  LoaderList loaders_to_cancel;

  for (auto i = pending_loaders_.begin(); i != pending_loaders_.end();) {
    ResourceLoader* loader = i->second.get();
    if (loader->GetRequestInfo()->GetContext() == context) {
      loaders_to_cancel.push_back(std::move(i->second));
      IncrementOutstandingRequestsMemory(-1, *loader->GetRequestInfo());
      if (loader->GetRequestInfo()->keepalive()) {
        keepalive_statistics_recorder_.OnLoadFinished(
            loader->GetRequestInfo()->GetChildID());
      }
      pending_loaders_.erase(i++);
    } else {
      ++i;
    }
  }

  for (auto i = blocked_loaders_map_.begin();
       i != blocked_loaders_map_.end();) {
    BlockedLoadersList* loaders = i->second.get();
    if (loaders->empty()) {
      // This can happen if BlockRequestsForRoute() has been called for a route,
      // but we haven't blocked any matching requests yet.
      ++i;
      continue;
    }
    ResourceRequestInfoImpl* info = loaders->front()->GetRequestInfo();
    if (info->GetContext() == context) {
      std::unique_ptr<BlockedLoadersList> deleter(std::move(i->second));
      blocked_loaders_map_.erase(i++);
      for (auto& loader : *loaders) {
        info = loader->GetRequestInfo();
        // We make the assumption that all requests on the list have the same
        // ResourceContext.
        DCHECK_EQ(context, info->GetContext());
        IncrementOutstandingRequestsMemory(-1, *info);
        loaders_to_cancel.push_back(std::move(loader));
      }
    } else {
      ++i;
    }
  }

#ifndef NDEBUG
  for (const auto& loader : loaders_to_cancel) {
    // There is no strict requirement that this be the case, but currently
    // downloads, streams, detachable requests, transferred requests, and
    // browser-owned requests are the only requests that aren't cancelled when
    // the associated processes go away. It may be OK for this invariant to
    // change in the future, but if this assertion fires without the invariant
    // changing, then it's indicative of a leak.
    DCHECK(
        loader->GetRequestInfo()->IsDownload() ||
        loader->GetRequestInfo()->is_stream() ||
        (loader->GetRequestInfo()->detachable_handler() &&
         loader->GetRequestInfo()->detachable_handler()->is_detached()) ||
        loader->GetRequestInfo()->requester_info()->IsBrowserSideNavigation() ||
        loader->GetRequestInfo()->GetResourceType() ==
            RESOURCE_TYPE_SERVICE_WORKER);
  }
#endif

  loaders_to_cancel.clear();
}

void ResourceDispatcherHostImpl::RegisterInterceptor(
    const std::string& http_header,
    const std::string& starts_with,
    const InterceptorCallback& interceptor) {
  DCHECK(!http_header.empty());
  DCHECK(interceptor);
  // Only one interceptor per header is supported.
  DCHECK(http_header_interceptor_map_.find(http_header) ==
         http_header_interceptor_map_.end());

  HeaderInterceptorInfo interceptor_info;
  interceptor_info.starts_with = starts_with;
  interceptor_info.interceptor = interceptor;

  http_header_interceptor_map_[http_header] = interceptor_info;
}

void ResourceDispatcherHostImpl::ReprioritizeRequest(
    net::URLRequest* request,
    net::RequestPriority priority) {
  scheduler_->ReprioritizeRequest(request, priority);
}

void ResourceDispatcherHostImpl::Shutdown() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ResourceDispatcherHostImpl::OnShutdown,
                                base::Unretained(this)));
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandlerForDownload(
    net::URLRequest* request,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request) {
  DCHECK(!create_download_handler_intercept_.is_null());
  // TODO(ananta)
  // Find a better way to create the download handler and notifying the
  // delegate of the download start.
  std::unique_ptr<ResourceHandler> handler =
      create_download_handler_intercept_.Run(request);
  handler =
      HandleDownloadStarted(request, std::move(handler), is_content_initiated,
                            must_download, is_new_request);
  return handler;
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::MaybeInterceptAsStream(
    net::URLRequest* request,
    network::ResourceResponse* response,
    std::string* payload) {
  payload->clear();
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  const std::string& mime_type = response->head.mime_type;

  GURL origin;
  if (!delegate_ || !delegate_->ShouldInterceptResourceAsStream(
                        request, mime_type, &origin, payload)) {
    return std::unique_ptr<ResourceHandler>();
  }

  StreamContext* stream_context =
      GetStreamContextForResourceContext(info->GetContext());

  std::unique_ptr<StreamResourceHandler> handler(new StreamResourceHandler(
      request, stream_context->registry(), origin, false));

  info->set_is_stream(true);
  std::unique_ptr<StreamInfo> stream_info(new StreamInfo);
  stream_info->handle = handler->stream()->CreateHandle();
  stream_info->original_url = request->url();
  stream_info->mime_type = mime_type;
  // Make a copy of the response headers so it is safe to pass across threads;
  // the old handler (AsyncResourceHandler) may modify it in parallel via the
  // ResourceDispatcherHostDelegate.
  if (response->head.headers.get()) {
    stream_info->response_headers =
        new net::HttpResponseHeaders(response->head.headers->raw_headers());
  }
  delegate_->OnStreamCreated(request, std::move(stream_info));
  return std::move(handler);
}

scoped_refptr<LoginDelegate> ResourceDispatcherHostImpl::CreateLoginDelegate(
    ResourceLoader* loader,
    net::AuthChallengeInfo* auth_info) {
  if (!delegate_)
    return nullptr;

  net::URLRequest* request = loader->request();

  ResourceRequestInfoImpl* resource_request_info =
      ResourceRequestInfoImpl::ForRequest(request);
  DCHECK(resource_request_info);
  bool is_request_for_main_frame =
      resource_request_info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME;
  GlobalRequestID request_id = resource_request_info->GetGlobalRequestID();

  GURL url = request->url();

  scoped_refptr<LoginDelegate> login_delegate =
      GetContentClient()->browser()->CreateLoginDelegate(
          auth_info, resource_request_info->GetWebContentsGetterForRequest(),
          request_id, is_request_for_main_frame, url,
          request->response_headers(),
          resource_request_info->first_auth_attempt(),
          base::BindOnce(&ResourceDispatcherHostImpl::RunAuthRequiredCallback,
                         base::Unretained(this), request_id));

  resource_request_info->set_first_auth_attempt(false);

  return login_delegate;
}

bool ResourceDispatcherHostImpl::HandleExternalProtocol(ResourceLoader* loader,
                                                        const GURL& url) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  if (!IsResourceTypeFrame(info->GetResourceType()))
    return false;

  const net::URLRequestJobFactory* job_factory =
      info->GetContext()->GetRequestContext()->job_factory();
  if (!url.is_valid() || job_factory->IsHandledProtocol(url.scheme()))
    return false;

  return GetContentClient()->browser()->HandleExternalProtocol(
      url, info->GetWebContentsGetterForRequest(), info->GetChildID(),
      info->GetNavigationUIData(), info->IsMainFrame(),
      info->GetPageTransition(), info->HasUserGesture());
}

void ResourceDispatcherHostImpl::DidStartRequest(ResourceLoader* loader) {
  // Make sure we have the load state monitors running.
  MaybeStartUpdateLoadInfoTimer();
}

void ResourceDispatcherHostImpl::DidReceiveRedirect(
    ResourceLoader* loader,
    const GURL& new_url,
    network::ResourceResponse* response) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  if (delegate_) {
    delegate_->OnRequestRedirected(
        new_url, loader->request(), info->GetContext(), response);
  }
}

void ResourceDispatcherHostImpl::DidReceiveResponse(
    ResourceLoader* loader,
    network::ResourceResponse* response) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();
  net::URLRequest* request = loader->request();
  if (delegate_)
    delegate_->OnResponseStarted(request, info->GetContext(), response);
}

void ResourceDispatcherHostImpl::DidFinishLoading(ResourceLoader* loader) {
  ResourceRequestInfoImpl* info = loader->GetRequestInfo();

  if (delegate_)
    delegate_->RequestComplete(loader->request());

  // Destroy the ResourceLoader.
  RemovePendingRequest(info->GetChildID(), info->GetRequestID());
}

void ResourceDispatcherHostImpl::OnInit() {
  scheduler_.reset(new network::ResourceScheduler(enable_resource_scheduler_));
}

void ResourceDispatcherHostImpl::OnShutdown() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());

  is_shutdown_ = true;

  // Explicitly invalidate while on the IO thread, where the associated WeakPtrs
  // are used.
  weak_factory_on_io_.InvalidateWeakPtrs();

  pending_loaders_.clear();

  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_info_timer_.reset();

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_loaders_map_, as it modifies it.
  std::set<GlobalFrameRoutingId> ids;
  for (const auto& blocked_loaders : blocked_loaders_map_) {
    std::pair<std::set<GlobalFrameRoutingId>::iterator, bool> result =
        ids.insert(blocked_loaders.first);
    // We should not have duplicates.
    DCHECK(result.second);
  }
  for (const auto& routing_id : ids) {
    CancelBlockedRequestsForRoute(routing_id);
  }

  scheduler_.reset();
}

void ResourceDispatcherHostImpl::OnRequestResourceInternal(
    ResourceRequesterInfo* requester_info,
    int routing_id,
    int request_id,
    bool is_sync_load,
    const network::ResourceRequest& request_data,
    uint32_t url_loader_options,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(requester_info->IsRenderer() ||
         requester_info->IsNavigationPreload() ||
         requester_info->IsCertificateFetcherForSignedExchange());
  BeginRequest(requester_info, request_id, request_data, is_sync_load,
               routing_id, url_loader_options, std::move(mojo_request),
               std::move(url_loader_client), traffic_annotation);
}

bool ResourceDispatcherHostImpl::IsRequestIDInUse(
    const GlobalRequestID& id) const {
  if (pending_loaders_.find(id) != pending_loaders_.end())
    return true;
  for (const auto& blocked_loaders : blocked_loaders_map_) {
    for (const auto& loader : *blocked_loaders.second.get()) {
      ResourceRequestInfoImpl* info = loader->GetRequestInfo();
      if (info->GetGlobalRequestID() == id)
        return true;
    }
  }
  return false;
}

void ResourceDispatcherHostImpl::BeginRequest(
    ResourceRequesterInfo* requester_info,
    int request_id,
    const network::ResourceRequest& request_data,
    bool is_sync_load,
    int route_id,
    uint32_t url_loader_options,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(requester_info->IsRenderer() ||
         requester_info->IsNavigationPreload() ||
         requester_info->IsCertificateFetcherForSignedExchange());

  int child_id = requester_info->child_id();

  // Reject request id that's currently in use.
  if (IsRequestIDInUse(GlobalRequestID(child_id, request_id))) {
    // Navigation preload requests have child_id's of -1 and monotonically
    // increasing request IDs allocated by MakeRequestID.
    DCHECK(requester_info->IsRenderer());
    bad_message::ReceivedBadMessage(requester_info->filter(),
                                    bad_message::RDH_INVALID_REQUEST_ID);
    return;
  }

  // Reject main resource request. They are handled by the browser process and
  // must not use this function.
  if (IsResourceTypeFrame(
          static_cast<ResourceType>(request_data.resource_type))) {
    // The resource_type of navigation preload requests must be SUB_RESOURCE.
    DCHECK(requester_info->IsRenderer());
    bad_message::ReceivedBadMessage(requester_info->filter(),
                                    bad_message::RDH_INVALID_URL);
    return;
  }

  // Reject invalid priority.
  if (request_data.priority < net::MINIMUM_PRIORITY ||
      request_data.priority > net::MAXIMUM_PRIORITY) {
    // The priority of navigation preload requests are copied from the original
    // request priority which must be checked beforehand.
    DCHECK(requester_info->IsRenderer());
    bad_message::ReceivedBadMessage(requester_info->filter(),
                                    bad_message::RDH_INVALID_PRIORITY);
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/91398
  DEBUG_ALIAS_FOR_GURL(url_buf, request_data.url);

  ResourceContext* resource_context = nullptr;
  net::URLRequestContext* request_context = nullptr;
  requester_info->GetContexts(
      static_cast<ResourceType>(request_data.resource_type), &resource_context,
      &request_context);

  // Parse the headers before calling ShouldServiceRequest, so that they are
  // available to be validated.
  if (is_shutdown_ ||
      !ShouldServiceRequest(child_id, request_data, request_data.headers,
                            requester_info, resource_context)) {
    AbortRequestBeforeItStarts(requester_info->filter(), request_id,
                               std::move(url_loader_client));
    return;
  }

  BlobHandles blob_handles;
  storage::BlobStorageContext* blob_context =
      GetBlobStorageContext(requester_info->blob_storage_context());
  // Resolve elements from request_body and prepare upload data.
  if (request_data.request_body.get()) {
    // |blob_context| could be null when the request is from the plugins
    // because ResourceMessageFilters created in PluginProcessHost don't have
    // the blob context.
    if (blob_context) {
      // Get BlobHandles to request_body to prevent blobs and any attached
      // shareable files from being freed until upload completion. These data
      // will be used in UploadDataStream and ServiceWorkerURLRequestJob.
      if (!GetBodyBlobDataHandles(request_data.request_body.get(),
                                  resource_context, &blob_handles)) {
        AbortRequestBeforeItStarts(requester_info->filter(), request_id,
                                   std::move(url_loader_client));
        return;
      }
    }
  }

  // Check if we have a registered interceptor for the headers passed in. If
  // yes then we need to mark the current request as pending and wait for the
  // interceptor to invoke the callback with a status code indicating whether
  // the request needs to be aborted or continued.
  for (net::HttpRequestHeaders::Iterator it(request_data.headers);
       it.GetNext();) {
    auto index = http_header_interceptor_map_.find(it.name());
    if (index != http_header_interceptor_map_.end()) {
      HeaderInterceptorInfo& interceptor_info = index->second;

      bool call_interceptor = true;
      if (!interceptor_info.starts_with.empty()) {
        call_interceptor =
            base::StartsWith(it.value(), interceptor_info.starts_with,
                             base::CompareCase::INSENSITIVE_ASCII);
      }
      if (call_interceptor) {
        interceptor_info.interceptor.Run(
            it.name(), it.value(), child_id, resource_context,
            base::Bind(
                &ResourceDispatcherHostImpl::ContinuePendingBeginRequest,
                base::Unretained(this), base::WrapRefCounted(requester_info),
                request_id, request_data, is_sync_load, route_id,
                request_data.headers, url_loader_options,
                base::Passed(std::move(mojo_request)),
                base::Passed(std::move(url_loader_client)),
                base::Passed(std::move(blob_handles)), traffic_annotation));
        return;
      }
    }
  }
  ContinuePendingBeginRequest(
      requester_info, request_id, request_data, is_sync_load, route_id,
      request_data.headers, url_loader_options, std::move(mojo_request),
      std::move(url_loader_client), std::move(blob_handles), traffic_annotation,
      HeaderInterceptorResult::CONTINUE);
}

void ResourceDispatcherHostImpl::ContinuePendingBeginRequest(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    int request_id,
    const network::ResourceRequest& request_data,
    bool is_sync_load,
    int route_id,
    const net::HttpRequestHeaders& headers,
    uint32_t url_loader_options,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client,
    BlobHandles blob_handles,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    HeaderInterceptorResult interceptor_result) {
  DCHECK(requester_info->IsRenderer() ||
         requester_info->IsNavigationPreload() ||
         requester_info->IsCertificateFetcherForSignedExchange());
  // The request is always for a subresource.
  // The renderer process is killed in BeginRequest() when it happens with a
  // main resource and the function returns immediatly.
  DCHECK(!IsResourceTypeFrame(
      static_cast<ResourceType>(request_data.resource_type)));

  if (interceptor_result != HeaderInterceptorResult::CONTINUE) {
    if (requester_info->IsRenderer() &&
        interceptor_result == HeaderInterceptorResult::KILL) {
      // TODO(ananta): Find a way to specify the right error code here. Passing
      // in a non-content error code is not safe, but future header interceptors
      // might say to kill for reasons other than illegal origins.
      bad_message::ReceivedBadMessage(requester_info->filter(),
                                      bad_message::RDH_ILLEGAL_ORIGIN);
    }
    AbortRequestBeforeItStarts(requester_info->filter(), request_id,
                               std::move(url_loader_client));
    return;
  }
  int child_id = requester_info->child_id();
  storage::BlobStorageContext* blob_context = nullptr;
  bool do_not_prompt_for_login = false;
  bool report_raw_headers = false;
  bool report_security_info = false;
  int load_flags = request_data.load_flags;

  ResourceContext* resource_context = nullptr;
  net::URLRequestContext* request_context = nullptr;
  requester_info->GetContexts(
      static_cast<ResourceType>(request_data.resource_type), &resource_context,
      &request_context);

  // All PREFETCH requests should be GETs, but be defensive about it.
  if (request_data.resource_type == RESOURCE_TYPE_PREFETCH &&
      request_data.method != "GET") {
    AbortRequestBeforeItStarts(requester_info->filter(), request_id,
                               std::move(url_loader_client));
    return;
  }

  // Construct the request.
  std::unique_ptr<net::URLRequest> new_request = request_context->CreateRequest(
      request_data.url, request_data.priority, nullptr, traffic_annotation);

  // Log that this request is a service worker navigation preload request
  // here, since navigation preload machinery has no access to netlog.
  // TODO(falken): Figure out how network::mojom::URLLoaderClient can
  // access the request's netlog.
  if (requester_info->IsNavigationPreload()) {
    new_request->net_log().AddEvent(
        net::NetLogEventType::SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST);
  }

  new_request->set_method(request_data.method);
  new_request->set_site_for_cookies(request_data.site_for_cookies);
  new_request->set_attach_same_site_cookies(
      request_data.attach_same_site_cookies);
  new_request->set_upgrade_if_insecure(request_data.upgrade_if_insecure);

  // The initiator should normally be present, unless this is a navigation.
  // Browser-initiated navigations don't have an initiator document, the
  // others have one.
  DCHECK(request_data.request_initiator.has_value() ||
         IsResourceTypeFrame(
             static_cast<ResourceType>(request_data.resource_type)));
  new_request->set_initiator(request_data.request_initiator);

  if (request_data.originated_from_service_worker) {
    new_request->SetUserData(URLRequestServiceWorkerData::kUserDataKey,
                             std::make_unique<URLRequestServiceWorkerData>());
  }

  new_request->SetReferrer(network::ComputeReferrer(request_data.referrer));
  new_request->set_referrer_policy(request_data.referrer_policy);

  new_request->SetExtraRequestHeaders(headers);
  if (!request_data.requested_with.empty()) {
    // X-Requested-With header must be set here to avoid breaking CORS checks.
    new_request->SetExtraRequestHeaderByName("X-Requested-With",
                                             request_data.requested_with, true);
  }

  std::unique_ptr<network::ScopedThrottlingToken> throttling_token =
      network::ScopedThrottlingToken::MaybeCreate(
          new_request->net_log().source().id,
          request_data.throttling_profile_id);

  blob_context = GetBlobStorageContext(requester_info->blob_storage_context());
  // Resolve elements from request_body and prepare upload data.
  if (request_data.request_body.get()) {
    new_request->set_upload(UploadDataStreamBuilder::Build(
        request_data.request_body.get(), blob_context,
        requester_info->file_system_context(),
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
            .get()));
  }

  do_not_prompt_for_login = request_data.do_not_prompt_for_login;

  // Raw headers are sensitive, as they include Cookie/Set-Cookie, so only
  // allow requesting them if requester has ReadRawCookies permission.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  report_raw_headers = request_data.report_raw_headers;
  // Security info is less sensitive than raw headers (does not include cookie
  // values), so |report_security_info| is not subject to the extra security
  // checks that are applied to |report_raw_headers|.
  report_security_info = request_data.report_raw_headers;
  if (report_raw_headers && !policy->CanReadRawCookies(child_id) &&
      !requester_info->IsNavigationPreload()) {
    // For navigation preload, the child_id is -1 so CanReadRawCookies would
    // return false. But |report_raw_headers| of the navigation preload
    // request was copied from the original request, so this check has already
    // been carried out.
    // TODO: https://crbug.com/523063 can we call
    // bad_message::ReceivedBadMessage here?
    VLOG(1) << "Denied unauthorized request for raw headers";
    report_raw_headers = false;
  }

  // Do not report raw headers if the request's site needs to be isolated
  // from the current process.
  if (report_raw_headers) {
    bool is_isolated =
        SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
        policy->IsIsolatedOrigin(url::Origin::Create(request_data.url));
    if (is_isolated &&
        !policy->CanAccessDataForOrigin(child_id, request_data.url))
      report_raw_headers = false;
  }

  if (DoNotPromptForLogin(static_cast<ResourceType>(request_data.resource_type),
                          request_data.url, request_data.site_for_cookies)) {
    // Prevent third-party image content from prompting for login, as this
    // is often a scam to extract credentials for another domain from the
    // user. Only block image loads, as the attack applies largely to the
    // "src" property of the <img> tag. It is common for web properties to
    // allow untrusted values for <img src>; this is considered a fair thing
    // for an HTML sanitizer to do. Conversely, any HTML sanitizer that didn't
    // filter sources for <script>, <link>, <embed>, <object>, <iframe> tags
    // would be considered vulnerable in and of itself.
    do_not_prompt_for_login = true;
    load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  // Sync loads should have maximum priority and should be the only
  // requets that have the ignore limits flag set.
  if (is_sync_load) {
    DCHECK_EQ(request_data.priority, net::MAXIMUM_PRIORITY);
    DCHECK_NE(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  } else {
    DCHECK_EQ(load_flags & net::LOAD_IGNORE_LIMITS, 0);
  }

  if (request_data.keepalive) {
    const auto& map = keepalive_statistics_recorder_.per_process_records();
    if (child_id != 0 && map.find(child_id) == map.end())
      keepalive_statistics_recorder_.Register(child_id);
  }

  new_request->SetLoadFlags(load_flags);

  // Make extra info and read footer (contains request ID).
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      requester_info, route_id,
      -1,  // frame_tree_node_id
      request_data.plugin_child_id, request_id, request_data.render_frame_id,
      request_data.is_main_frame,
      static_cast<ResourceType>(request_data.resource_type),
      static_cast<ui::PageTransition>(request_data.transition_type),
      false,  // is download
      false,  // is stream
      false,  // allow_download,
      request_data.has_user_gesture, request_data.enable_load_timing,
      request_data.enable_upload_progress, do_not_prompt_for_login,
      request_data.keepalive,
      Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
          request_data.referrer_policy),
      request_data.is_prerendering, resource_context, report_raw_headers,
      report_security_info, !is_sync_load, request_data.previews_state,
      request_data.request_body, request_data.initiated_in_secure_context);
  extra_info->SetBlobHandles(std::move(blob_handles));

  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(), requester_info->blob_storage_context()
                               ->context()
                               ->GetBlobDataFromPublicURL(new_request->url()));
  }

  // Initialize the service worker handler for the request.
  ServiceWorkerRequestHandler::InitializeHandler(
      new_request.get(), requester_info->service_worker_context(), blob_context,
      child_id, request_data.service_worker_provider_id,
      request_data.skip_service_worker, request_data.fetch_request_mode,
      request_data.fetch_credentials_mode, request_data.fetch_redirect_mode,
      request_data.fetch_integrity, request_data.keepalive,
      static_cast<ResourceType>(request_data.resource_type),
      static_cast<blink::mojom::RequestContextType>(
          request_data.fetch_request_context_type),
      request_data.fetch_frame_type, request_data.request_body);

  // Have the appcache associate its extra info with the request.
  AppCacheInterceptor::SetExtraRequestInfo(
      new_request.get(), requester_info->appcache_service(), child_id,
      request_data.appcache_host_id,
      static_cast<ResourceType>(request_data.resource_type),
      request_data.should_reset_appcache);

  std::unique_ptr<ResourceHandler> handler = CreateResourceHandler(
      requester_info.get(), new_request.get(), request_data, route_id, child_id,
      resource_context, url_loader_options, std::move(mojo_request),
      std::move(url_loader_client));

  if (handler) {
    RecordFetchRequestMode(request_data.url, request_data.method,
                           request_data.fetch_request_mode);
    const bool is_initiated_by_fetch_api =
        request_data.fetch_request_context_type ==
        static_cast<int>(blink::mojom::RequestContextType::FETCH);
    BeginRequestInternal(std::move(new_request), std::move(handler),
                         is_initiated_by_fetch_api,
                         std::move(throttling_token));
  }
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::CreateResourceHandler(
    ResourceRequesterInfo* requester_info,
    net::URLRequest* request,
    const network::ResourceRequest& request_data,
    int route_id,
    int child_id,
    ResourceContext* resource_context,
    uint32_t url_loader_options,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client) {
  DCHECK(requester_info->IsRenderer() ||
         requester_info->IsNavigationPreload() ||
         requester_info->IsCertificateFetcherForSignedExchange());
  // Construct the IPC resource handler.
  std::unique_ptr<ResourceHandler> handler =
      std::make_unique<MojoAsyncResourceHandler>(
          request, this, std::move(mojo_request), std::move(url_loader_client),
          static_cast<ResourceType>(request_data.resource_type),
          url_loader_options);

  // Prefetches outlive their child process.
  if (request_data.resource_type == RESOURCE_TYPE_PREFETCH) {
    auto detachable_handler = std::make_unique<DetachableResourceHandler>(
        request,
        base::TimeDelta::FromMilliseconds(kDefaultDetachableCancelDelayMs),
        std::move(handler));
    handler = std::move(detachable_handler);
  }

  return AddStandardHandlers(
      request, static_cast<ResourceType>(request_data.resource_type),
      resource_context, request_data.fetch_request_mode,
      static_cast<blink::mojom::RequestContextType>(
          request_data.fetch_request_context_type),
      url_loader_options, requester_info->appcache_service(), child_id,
      route_id, std::move(handler));
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::AddStandardHandlers(
    net::URLRequest* request,
    ResourceType resource_type,
    ResourceContext* resource_context,
    network::mojom::FetchRequestMode fetch_request_mode,
    blink::mojom::RequestContextType fetch_request_context_type,
    uint32_t url_loader_options,
    AppCacheService* appcache_service,
    int child_id,
    int route_id,
    std::unique_ptr<ResourceHandler> handler) {
  // The InterceptingResourceHandler will replace its next handler with an
  // appropriate one based on the MIME type of the response if needed. It
  // should be placed at the end of the chain, just before |handler|.
  handler.reset(new InterceptingResourceHandler(std::move(handler), request));
  InterceptingResourceHandler* intercepting_handler =
      static_cast<InterceptingResourceHandler*>(handler.get());

  std::vector<std::unique_ptr<ResourceThrottle>> throttles;

  if (delegate_) {
    delegate_->RequestBeginning(request,
                                resource_context,
                                appcache_service,
                                resource_type,
                                &throttles);
  }

  // The Clear-Site-Data throttle.
  std::unique_ptr<ResourceThrottle> clear_site_data_throttle =
      ClearSiteDataThrottle::MaybeCreateThrottleForRequest(request);
  if (clear_site_data_throttle)
    throttles.push_back(std::move(clear_site_data_throttle));

  // TODO(ricea): Stop looking this up so much.
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  throttles.push_back(std::make_unique<ScheduledResourceRequestAdapter>(
      scheduler_->ScheduleRequest(child_id, route_id, info->IsAsync(),
                                  request)));

  // Split the handler in two groups: the ones that need to execute
  // WillProcessResponse before mime sniffing and the others.
  std::vector<std::unique_ptr<ResourceThrottle>> pre_mime_sniffing_throttles;
  std::vector<std::unique_ptr<ResourceThrottle>> post_mime_sniffing_throttles;
  for (auto& throttle : throttles) {
    if (throttle->MustProcessResponseBeforeReadingBody()) {
      pre_mime_sniffing_throttles.push_back(std::move(throttle));
    } else {
      post_mime_sniffing_throttles.push_back(std::move(throttle));
    }
  }
  throttles.clear();

  // Add the post mime sniffing throttles.
  handler.reset(new ThrottlingResourceHandler(
      std::move(handler), request, std::move(post_mime_sniffing_throttles)));

  PluginService* plugin_service = nullptr;
#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_service = PluginService::GetInstance();
#endif

  if (!IsResourceTypeFrame(resource_type)) {
    // Add a handler to block cross-site documents from the renderer process.
    bool is_nocors_plugin_request =
        resource_type == RESOURCE_TYPE_PLUGIN_RESOURCE &&
        fetch_request_mode == network::mojom::FetchRequestMode::kNoCORS;
    handler.reset(new CrossSiteDocumentResourceHandler(
        std::move(handler), request, is_nocors_plugin_request));
  }

  // Insert a buffered event handler to sniff the mime type.
  // Note: all ResourceHandler following the MimeSniffingResourceHandler
  // should expect OnWillRead to be called *before* OnResponseStarted as
  // part of the mime sniffing process.
  if (url_loader_options & network::mojom::kURLLoadOptionSniffMimeType) {
    handler.reset(new MimeSniffingResourceHandler(
        std::move(handler), this, plugin_service, intercepting_handler, request,
        fetch_request_context_type));
  }

  // Add the pre mime sniffing throttles.
  handler.reset(new ThrottlingResourceHandler(
      std::move(handler), request, std::move(pre_mime_sniffing_throttles)));

  return handler;
}

ResourceRequestInfoImpl* ResourceDispatcherHostImpl::CreateRequestInfo(
    int child_id,
    int render_view_route_id,
    int render_frame_route_id,
    PreviewsState previews_state,
    bool download,
    ResourceContext* context) {
  return new ResourceRequestInfoImpl(
      ResourceRequesterInfo::CreateForDownloadOrPageSave(child_id),
      render_view_route_id,
      -1,                                  // frame_tree_node_id
      ChildProcessHost::kInvalidUniqueID,  // plugin_child_id
      MakeRequestID(), render_frame_route_id,
      false,  // is_main_frame
      RESOURCE_TYPE_SUB_RESOURCE, ui::PAGE_TRANSITION_LINK,
      download,  // is_download
      false,     // is_stream
      download,  // allow_download
      false,     // has_user_gesture
      false,     // enable_load_timing
      false,     // enable_upload_progress
      false,     // do_not_prompt_for_login
      false,     // keepalive
      network::mojom::ReferrerPolicy::kDefault,
      false,  // is_prerendering
      context,
      false,           // report_raw_headers
      false,           // report_security_info
      true,            // is_async
      previews_state,  // previews_state
      nullptr,         // body
      false);          // initiated_in_secure_context
}

void ResourceDispatcherHostImpl::OnRenderViewHostCreated(
    int child_id,
    int route_id,
    net::URLRequestContextGetter* url_request_context_getter) {
  scheduler_->OnClientCreated(child_id, route_id,
                              url_request_context_getter->GetURLRequestContext()
                                  ->network_quality_estimator());
}

void ResourceDispatcherHostImpl::OnRenderViewHostDeleted(int child_id,
                                                         int route_id) {
  scheduler_->OnClientDeleted(child_id, route_id);
}

void ResourceDispatcherHostImpl::OnRenderViewHostSetIsLoading(int child_id,
                                                              int route_id,
                                                              bool is_loading) {
  scheduler_->DeprecatedOnLoadingStateChanged(child_id, route_id, !is_loading);
}

// The object died, so cancel and detach all requests associated with it except
// for downloads and detachable resources, which belong to the browser process
// even if initiated via a renderer.
void ResourceDispatcherHostImpl::CancelRequestsForProcess(int child_id) {
  CancelRequestsForRoute(
      GlobalFrameRoutingId(child_id, MSG_ROUTING_NONE /* cancel all */));
  const auto& map = keepalive_statistics_recorder_.per_process_records();
  if (map.find(child_id) != map.end())
    keepalive_statistics_recorder_.Unregister(child_id);
}

void ResourceDispatcherHostImpl::CancelRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  int child_id = global_routing_id.child_id;
  int route_id = global_routing_id.frame_routing_id;
  bool cancel_all_routes = (route_id == MSG_ROUTING_NONE);

  std::vector<GlobalRequestID> matching_requests;
  for (const auto& loader : pending_loaders_) {
    if (loader.first.child_id != child_id)
      continue;

    ResourceRequestInfoImpl* info = loader.second->GetRequestInfo();

    GlobalRequestID id(child_id, loader.first.request_id);
    DCHECK(id == loader.first);
    // Don't cancel navigations that are expected to live beyond this process.
    if (cancel_all_routes || route_id == info->GetRenderFrameID()) {
      if (info->keepalive() && !cancel_all_routes) {
        // If the keepalive flag is set, that request will outlive the frame
        // deliberately, so we don't cancel it here.
      } else if (info->detachable_handler()) {
        info->detachable_handler()->Detach();
      } else if (!info->IsDownload() && !info->is_stream()) {
        matching_requests.push_back(id);
      }
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    auto iter = pending_loaders_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a net::URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // net::URLRequest that needs exclusive access to the same cache entry, and
    // that net::URLRequest may complete and remove itself from
    // pending_requests_. So we need to check that iter is not equal to
    // pending_requests_.end().
    if (iter != pending_loaders_.end())
      RemovePendingLoader(iter);
  }

  // Now deal with blocked requests if any.
  if (!cancel_all_routes) {
    if (blocked_loaders_map_.find(global_routing_id) !=
        blocked_loaders_map_.end()) {
      CancelBlockedRequestsForRoute(global_routing_id);
    }
  } else {
    // We have to do all render frames for the process |child_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_loaders_map_, as blocking requests modifies the map.
    std::set<GlobalFrameRoutingId> routing_ids;
    for (const auto& blocked_loaders : blocked_loaders_map_) {
      if (blocked_loaders.first.child_id == child_id)
        routing_ids.insert(blocked_loaders.first);
    }
    for (const GlobalFrameRoutingId& frame_route_id : routing_ids) {
      CancelBlockedRequestsForRoute(frame_route_id);
    }
  }
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHostImpl::RemovePendingRequest(int child_id,
                                                      int request_id) {
  auto i = pending_loaders_.find(GlobalRequestID(child_id, request_id));
  if (i == pending_loaders_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingLoader(i);
}

void ResourceDispatcherHostImpl::RemovePendingLoader(
    const LoaderMap::iterator& iter) {
  ResourceRequestInfoImpl* info = iter->second->GetRequestInfo();

  if (info->keepalive())
    keepalive_statistics_recorder_.OnLoadFinished(info->GetChildID());

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemory(-1, *info);

  pending_loaders_.erase(iter);
}

void ResourceDispatcherHostImpl::CancelRequest(int child_id,
                                               int request_id) {
  ResourceLoader* loader = GetLoader(child_id, request_id);
  if (!loader) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DVLOG(1) << "Canceling a request that wasn't found";
    return;
  }

  RemovePendingRequest(child_id, request_id);
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::GetOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info) {
  auto entry = outstanding_requests_stats_map_.find(info.GetChildID());
  OustandingRequestsStats stats = { 0, 0 };
  if (entry != outstanding_requests_stats_map_.end())
    stats = entry->second;
  return stats;
}

void ResourceDispatcherHostImpl::UpdateOutstandingRequestsStats(
    const ResourceRequestInfoImpl& info,
    const OustandingRequestsStats& stats) {
  if (stats.memory_cost == 0 && stats.num_requests == 0)
    outstanding_requests_stats_map_.erase(info.GetChildID());
  else
    outstanding_requests_stats_map_[info.GetChildID()] = stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsMemory(
    int count,
    const ResourceRequestInfoImpl& info) {
  DCHECK_EQ(1, abs(count));

  // Retrieve the previous value (defaulting to 0 if not found).
  OustandingRequestsStats stats = GetOutstandingRequestsStats(info);

  // Insert/update the total; delete entries when their count reaches 0.
  stats.memory_cost += count * info.memory_cost();
  DCHECK_GE(stats.memory_cost, 0);
  UpdateOutstandingRequestsStats(info, stats);

  return stats;
}

ResourceDispatcherHostImpl::OustandingRequestsStats
ResourceDispatcherHostImpl::IncrementOutstandingRequestsCount(
    int count,
    ResourceRequestInfoImpl* info) {
  DCHECK_EQ(1, abs(count));
  num_in_flight_requests_ += count;

  // Keep track of whether this request is counting toward the number of
  // in-flight requests for this process, in case we need to transfer it to
  // another process. This should be a toggle.
  DCHECK_NE(info->counted_as_in_flight_request(), count > 0);
  info->set_counted_as_in_flight_request(count > 0);

  OustandingRequestsStats stats = GetOutstandingRequestsStats(*info);
  stats.num_requests += count;
  DCHECK_GE(stats.num_requests, 0);
  UpdateOutstandingRequestsStats(*info, stats);

  return stats;
}

bool ResourceDispatcherHostImpl::HasSufficientResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  OustandingRequestsStats stats = IncrementOutstandingRequestsCount(1, info);

  if (stats.num_requests > max_num_in_flight_requests_per_process_)
    return false;
  if (num_in_flight_requests_ > max_num_in_flight_requests_)
    return false;

  return true;
}

void ResourceDispatcherHostImpl::FinishedWithResourcesForRequest(
    net::URLRequest* request) {
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request);
  IncrementOutstandingRequestsCount(-1, info);
}

void ResourceDispatcherHostImpl::BeginNavigationRequest(
    ResourceContext* resource_context,
    net::URLRequestContext* request_context,
    storage::FileSystemContext* upload_file_system_context,
    const NavigationRequestInfo& info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    network::mojom::URLLoaderClientPtr url_loader_client,
    network::mojom::URLLoaderRequest url_loader_request,
    ServiceWorkerNavigationHandleCore* service_worker_handle_core,
    AppCacheNavigationHandleCore* appcache_handle_core,
    uint32_t url_loader_options,
    const GlobalRequestID& global_request_id) {
  DCHECK(url_loader_client.is_bound());
  DCHECK(url_loader_request.is_pending());

  ResourceType resource_type = info.is_main_frame ?
      RESOURCE_TYPE_MAIN_FRAME : RESOURCE_TYPE_SUB_FRAME;

  // Do not allow browser plugin guests to navigate to non-web URLs, since they
  // cannot swap processes or grant bindings. Do not check external protocols
  // here because they're checked in
  // ChromeResourceDispatcherHostDelegate::HandleExternalProtocol.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  bool is_external_protocol =
      info.common_params.url.is_valid() &&
      !resource_context->GetRequestContext()->job_factory()->IsHandledProtocol(
          info.common_params.url.scheme());
  bool non_web_url_in_guest =
      info.is_for_guests_only &&
      !policy->IsWebSafeScheme(info.common_params.url.scheme()) &&
      !is_external_protocol;

  if (is_shutdown_ || non_web_url_in_guest) {
    url_loader_client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  int load_flags = info.begin_params->load_flags;
  if (info.is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  std::unique_ptr<net::URLRequest> new_request;
  net::RequestPriority net_priority = net::HIGHEST;
  if (!info.is_main_frame &&
      base::FeatureList::IsEnabled(features::kLowPriorityIframes)) {
    net_priority = net::LOWEST;
  }
  new_request = request_context->CreateRequest(
      info.common_params.url, net_priority, nullptr, GetTrafficAnnotation());

  new_request->set_method(info.common_params.method);
  new_request->set_site_for_cookies(info.site_for_cookies);
  new_request->set_initiator(info.begin_params->initiator_origin);
  new_request->set_upgrade_if_insecure(info.upgrade_if_insecure);
  if (info.is_main_frame) {
    new_request->set_first_party_url_policy(
        net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  }

  std::unique_ptr<network::ScopedThrottlingToken> throttling_token =
      network::ScopedThrottlingToken::MaybeCreate(
          new_request->net_log().source().id, info.devtools_frame_token);

  Referrer::SetReferrerForRequest(new_request.get(),
                                  info.common_params.referrer);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(info.begin_params->headers);

  std::string accept_value = network::kFrameAcceptHeader;
  if (signed_exchange_utils::ShouldAdvertiseAcceptHeader(
          url::Origin::Create(info.common_params.url))) {
    DCHECK(!accept_value.empty());
    accept_value.append(kAcceptHeaderSignedExchangeSuffix);
  }

  headers.SetHeader(network::kAcceptHeader, accept_value);
  new_request->SetExtraRequestHeaders(headers);

  new_request->SetLoadFlags(load_flags);

  storage::BlobStorageContext* blob_context = GetBlobStorageContext(
      GetChromeBlobStorageContextForResourceContext(resource_context));

  // Resolve elements from request_body and prepare upload data.
  network::ResourceRequestBody* body = info.common_params.post_data.get();
  BlobHandles blob_handles;
  if (body) {
    if (!GetBodyBlobDataHandles(body, resource_context, &blob_handles)) {
      new_request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
      url_loader_client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
      return;
    }
    new_request->set_upload(UploadDataStreamBuilder::Build(
        body, blob_context, upload_file_system_context,
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
            .get()));
  }

  // Make extra info and read footer (contains request ID).
  //
  // TODO(davidben): Associate the request with the FrameTreeNode and/or tab so
  // that IO thread -> UI thread hops will work.
  ResourceRequestInfoImpl* extra_info = new ResourceRequestInfoImpl(
      ResourceRequesterInfo::CreateForBrowserSideNavigation(
          service_worker_handle_core
              ? service_worker_handle_core->context_wrapper()
              : scoped_refptr<ServiceWorkerContextWrapper>()),
      -1,  // route_id
      info.frame_tree_node_id,
      ChildProcessHost::kInvalidUniqueID,  // plugin_child_id
      global_request_id.request_id,
      -1,  // request_data.render_frame_id,
      info.is_main_frame, resource_type, info.common_params.transition,
      false,  // is download
      false,  // is stream
      info.common_params.allow_download, info.common_params.has_user_gesture,
      true,   // enable_load_timing
      false,  // enable_upload_progress
      false,  // do_not_prompt_for_login
      false,  // keepalive
      info.common_params.referrer.policy, info.is_prerendering,
      resource_context, info.report_raw_headers,
      // For navigation requests, security info is reported whenever raw headers
      // are. This behavior is different for subresources; see
      // ContinuePendingBeginRequest.
      info.report_raw_headers,
      true,  // is_async
      info.common_params.previews_state, info.common_params.post_data,
      // TODO(mek): Currently initiated_in_secure_context is only used for
      // subresource requests, so it doesn't matter what value it gets here.
      // If in the future this changes this should be updated to somehow get a
      // meaningful value.
      false);  // initiated_in_secure_context
  extra_info->SetBlobHandles(std::move(blob_handles));
  extra_info->set_navigation_ui_data(std::move(navigation_ui_data));

  // Request takes ownership.
  extra_info->AssociateWithRequest(new_request.get());

  if (new_request->url().SchemeIs(url::kBlobScheme)) {
    // Hang on to a reference to ensure the blob is not released prior
    // to the job being started.
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        new_request.get(),
        blob_context->GetBlobDataFromPublicURL(new_request->url()));
  }

  network::mojom::RequestContextFrameType frame_type =
      info.is_main_frame ? network::mojom::RequestContextFrameType::kTopLevel
                         : network::mojom::RequestContextFrameType::kNested;
  ServiceWorkerRequestHandler::InitializeForNavigation(
      new_request.get(), service_worker_handle_core, blob_context,
      info.begin_params->skip_service_worker, resource_type,
      info.begin_params->request_context_type, frame_type,
      info.are_ancestors_secure, info.common_params.post_data,
      extra_info->GetWebContentsGetterForRequest());

  // Have the appcache associate its extra info with the request.
  if (appcache_handle_core) {
    AppCacheInterceptor::SetExtraRequestInfoForHost(
        new_request.get(), appcache_handle_core->host(), resource_type, false);
  }

  std::unique_ptr<ResourceHandler> handler;
  handler = std::make_unique<MojoAsyncResourceHandler>(
      new_request.get(), this, std::move(url_loader_request),
      std::move(url_loader_client), resource_type, url_loader_options);

  // Safe to consider navigations as kNoCORS.
  // TODO(davidben): Fix the dependency on child_id/route_id. Those are used
  // by the ResourceScheduler. currently it's a no-op.
  handler = AddStandardHandlers(
      new_request.get(), resource_type, resource_context,
      network::mojom::FetchRequestMode::kNoCORS,
      info.begin_params->request_context_type, url_loader_options,
      appcache_handle_core ? appcache_handle_core->GetAppCacheService()
                           : nullptr,
      -1,  // child_id
      -1,  // route_id
      std::move(handler));

  RecordFetchRequestMode(new_request->url(), new_request->method(),
                         network::mojom::FetchRequestMode::kNavigate);
  BeginRequestInternal(std::move(new_request), std::move(handler),
                       false /* is_initiated_by_fetch_api */,
                       std::move(throttling_token));
}

void ResourceDispatcherHostImpl::SetLoaderDelegate(
    LoaderDelegate* loader_delegate) {
  loader_delegate_ = loader_delegate;
}

void ResourceDispatcherHostImpl::OnRenderFrameDeleted(
    const GlobalFrameRoutingId& global_routing_id) {
  CancelRequestsForRoute(global_routing_id);
}

void ResourceDispatcherHostImpl::OnRequestResourceWithMojo(
    ResourceRequesterInfo* requester_info,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderRequest mojo_request,
    network::mojom::URLLoaderClientPtr url_loader_client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!url_loader_client) {
    VLOG(1) << "Killed renderer for null client";
    bad_message::ReceivedBadMessage(requester_info->filter(),
                                    bad_message::RDH_NULL_CLIENT);
    return;
  }
  bool is_sync_load = options & network::mojom::kURLLoadOptionSynchronous;
  OnRequestResourceInternal(requester_info, routing_id, request_id,
                            is_sync_load, request, options,
                            std::move(mojo_request),
                            std::move(url_loader_client), traffic_annotation);
}

// static
int ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
    net::URLRequest* request) {
  // The following fields should be a minor size contribution (experimentally
  // on the order of 100). However since they are variable length, it could
  // in theory be a sizeable contribution.
  int strings_cost = 0;
  for (net::HttpRequestHeaders::Iterator it(request->extra_request_headers());
       it.GetNext();) {
    strings_cost += it.name().length() + it.value().length();
  }
  strings_cost +=
      request->original_url().parsed_for_possibly_invalid_spec().Length() +
      request->referrer().size() + request->method().size();

  // Note that this expression will typically be dominated by:
  // |kAvgBytesPerOutstandingRequest|.
  return kAvgBytesPerOutstandingRequest + strings_cost;
}

void ResourceDispatcherHostImpl::BeginRequestInternal(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceHandler> handler,
    bool is_initiated_by_fetch_api,
    std::unique_ptr<network::ScopedThrottlingToken> throttling_token) {
  DCHECK(!request->is_pending());
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());

  // Log metrics for back-forward navigations.
  // TODO(clamy): Remove this once we understand the reason behind the
  // back-forward PLT regression with PlzNavigate
  if ((info->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      IsResourceTypeFrame(info->GetResourceType()) &&
      request->url().SchemeIsHTTPOrHTTPS()) {
    LogBackForwardNavigationFlagsHistogram(request->load_flags());
  }

  if ((TimeTicks::Now() - last_user_gesture_time_) <
      TimeDelta::FromMilliseconds(kUserGestureWindowMs)) {
    request->SetLoadFlags(request->load_flags() | net::LOAD_MAYBE_USER_GESTURE);
  }

  // Add the memory estimate that starting this request will consume.
  info->set_memory_cost(CalculateApproximateMemoryCost(request.get()));

  bool exhausted = false;

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  OustandingRequestsStats stats = IncrementOutstandingRequestsMemory(1, *info);
  if (stats.memory_cost > max_outstanding_requests_cost_per_process_)
    exhausted = true;

  // requests with keepalive set have additional limitations.
  if (info->keepalive()) {
    constexpr auto kMaxKeepaliveConnections =
        network::URLLoaderFactory::kMaxKeepaliveConnections;
    constexpr auto kMaxKeepaliveConnectionsPerProcess =
        network::URLLoaderFactory::kMaxKeepaliveConnectionsPerProcess;
    constexpr auto kMaxKeepaliveConnectionsPerProcessForFetchAPI = network::
        URLLoaderFactory::kMaxKeepaliveConnectionsPerProcessForFetchAPI;
    // URLLoaderFactory::CreateLoaderAndStart has the duplicate logic.
    // Update there too when this logic is updated.
    const auto& recorder = keepalive_statistics_recorder_;
    if (recorder.num_inflight_requests() >= kMaxKeepaliveConnections)
      exhausted = true;
    if (recorder.NumInflightRequestsPerProcess(info->GetChildID()) >=
        kMaxKeepaliveConnectionsPerProcess) {
      exhausted = true;
    }
    if (is_initiated_by_fetch_api &&
        recorder.NumInflightRequestsPerProcess(info->GetChildID()) >=
            kMaxKeepaliveConnectionsPerProcessForFetchAPI) {
      exhausted = true;
    }
  }

  if (exhausted) {
    // We call "CancelWithError()" as a way of setting the net::URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);

    bool was_resumed = false;
    // TODO(mmenke): Get rid of NullResourceController and do something more
    // reasonable.
    handler->OnResponseCompleted(
        request->status(),
        std::make_unique<NullResourceController>(&was_resumed));
    // TODO(darin): The handler is not ready for us to kill the request. Oops!
    DCHECK(was_resumed);

    IncrementOutstandingRequestsMemory(-1, *info);

    // A ResourceHandler must not outlive its associated URLRequest.
    handler.reset();
    return;
  }

  ResourceContext* resource_context = info->GetContext();
  std::unique_ptr<ResourceLoader> loader(
      new ResourceLoader(std::move(request), std::move(handler), this,
                         resource_context, std::move(throttling_token)));

  GlobalFrameRoutingId id(info->GetChildID(), info->GetRenderFrameID());
  BlockedLoadersMap::const_iterator iter = blocked_loaders_map_.find(id);
  if (iter != blocked_loaders_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(std::move(loader));
    return;
  }

  StartLoading(info, std::move(loader));
}

void ResourceDispatcherHostImpl::InitializeURLRequest(
    net::URLRequest* request,
    const Referrer& referrer,
    bool is_download,
    int render_process_host_id,
    int render_view_routing_id,
    int render_frame_routing_id,
    PreviewsState previews_state,
    ResourceContext* context) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!request->is_pending());

  Referrer::SetReferrerForRequest(request, referrer);

  ResourceRequestInfoImpl* info = CreateRequestInfo(
      render_process_host_id, render_view_routing_id, render_frame_routing_id,
      previews_state, is_download, context);
  // Request takes ownership.
  info->AssociateWithRequest(request);
}

void ResourceDispatcherHostImpl::BeginURLRequest(
    std::unique_ptr<net::URLRequest> request,
    std::unique_ptr<ResourceHandler> handler,
    bool is_download,
    bool is_content_initiated,
    bool do_not_prompt_for_login,
    ResourceContext* context) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!request->is_pending());

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request.get());
  DCHECK(info);
  info->set_do_not_prompt_for_login(do_not_prompt_for_login);

  // TODO(ananta)
  // Find a better place for notifying the delegate about the download start.
  if (is_download && delegate()) {
    // TODO(ananta)
    // Investigate whether the blob logic should apply for the SaveAs case and
    // if yes then move the code below outside the if block.
    if (request->original_url().SchemeIs(url::kBlobScheme) &&
        !storage::BlobProtocolHandler::GetRequestBlobDataHandle(
            request.get())) {
      ChromeBlobStorageContext* blob_context =
          GetChromeBlobStorageContextForResourceContext(context);
      storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
          request.get(),
          blob_context->context()->GetBlobDataFromPublicURL(
              request->original_url()));
    }
    handler = HandleDownloadStarted(
        request.get(), std::move(handler), is_content_initiated,
        true /* force_download */, true /* is_new_request */);
  }
  BeginRequestInternal(std::move(request), std::move(handler),
                       false /* is_initiated_by_fetch_api */,
                       nullptr /* throttling_token */);
}

int ResourceDispatcherHostImpl::MakeRequestID() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  return --request_id_;
}

GlobalRequestID ResourceDispatcherHostImpl::MakeGlobalRequestID() {
  return GlobalRequestID(ChildProcessHost::kInvalidUniqueID, MakeRequestID());
}

void ResourceDispatcherHostImpl::CancelRequestFromRenderer(
    GlobalRequestID request_id) {
  ResourceLoader* loader = GetLoader(request_id);

  // It is possible that the request has been completed and removed from the
  // loader queue but the client has not processed the request completed message
  // before issuing a cancel. This happens frequently for beacons which are
  // canceled in the response received handler.
  if (!loader)
    return;

  loader->CancelRequest(true);
}

bool ResourceDispatcherHostImpl::DoNotPromptForLogin(
    ResourceType resource_type,
    const GURL& url,
    const GURL& site_for_cookies) {
  if (resource_type == RESOURCE_TYPE_IMAGE &&
      HTTP_AUTH_RELATION_BLOCKED_CROSS ==
          HttpAuthRelationTypeOf(url, site_for_cookies)) {
    return true;
  }
  return false;
}

void ResourceDispatcherHostImpl::StartLoading(
    ResourceRequestInfoImpl* info,
    std::unique_ptr<ResourceLoader> loader) {
  ResourceLoader* loader_ptr = loader.get();
  DCHECK(pending_loaders_[info->GetGlobalRequestID()] == nullptr);
  pending_loaders_[info->GetGlobalRequestID()] = std::move(loader);
  if (info->keepalive())
    keepalive_statistics_recorder_.OnLoadStarted(info->GetChildID());

  loader_ptr->StartRequest();
}

void ResourceDispatcherHostImpl::OnUserGesture() {
  last_user_gesture_time_ = TimeTicks::Now();
}

net::URLRequest* ResourceDispatcherHostImpl::GetURLRequest(
    const GlobalRequestID& id) {
  ResourceLoader* loader = GetLoader(id);
  if (!loader)
    return nullptr;

  return loader->request();
}

// static
// This is duplicated in services/network/network_service.cc.
bool ResourceDispatcherHostImpl::LoadInfoIsMoreInteresting(const LoadInfo& a,
                                                           const LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64_t b_uploading_size = 0;
  if (b.load_state.state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state.state > b.load_state.state;
}

// static
void ResourceDispatcherHostImpl::UpdateLoadStateOnUI(
    LoaderDelegate* loader_delegate, std::unique_ptr<LoadInfoList> infos) {
  DCHECK(Get()->main_thread_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<LoadInfoMap> info_map =
      PickMoreInterestingLoadInfos(std::move(infos));
  for (const auto& load_info: *info_map) {
    loader_delegate->LoadStateChanged(
        load_info.first, load_info.second.host, load_info.second.load_state,
        load_info.second.upload_position, load_info.second.upload_size);
  }
}

// static
std::unique_ptr<ResourceDispatcherHostImpl::LoadInfoMap>
ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
    std::unique_ptr<LoadInfoList> infos) {
  auto info_map = std::make_unique<LoadInfoMap>();
  for (const auto& load_info : *infos) {
    WebContents* web_contents = load_info.web_contents_getter.Run();
    if (!web_contents)
      continue;

    auto existing = info_map->find(web_contents);
    if (existing == info_map->end() ||
        LoadInfoIsMoreInteresting(load_info, existing->second)) {
      (*info_map)[web_contents] = load_info;
    }
  }
  return info_map;
}

std::unique_ptr<ResourceDispatcherHostImpl::LoadInfoList>
ResourceDispatcherHostImpl::GetInterestingPerFrameLoadInfos() {
  auto infos = std::make_unique<LoadInfoList>();
  std::map<GlobalFrameRoutingId, LoadInfo> frame_infos;
  for (const auto& loader : pending_loaders_) {
    net::URLRequest* request = loader.second->request();
    net::UploadProgress upload_progress = request->GetUploadProgress();

    LoadInfo load_info;
    load_info.host = request->url().host();
    load_info.load_state = request->GetLoadState();
    load_info.upload_size = upload_progress.size();
    load_info.upload_position = upload_progress.position();

    ResourceRequestInfoImpl* request_info = loader.second->GetRequestInfo();
    load_info.web_contents_getter =
        request_info->GetWebContentsGetterForRequest();

    // Navigation requests have frame_tree_node_ids, and may not have frame
    // routing ids. Just include them unconditionally.
    if (request_info->frame_tree_node_id() != -1) {
      infos->push_back(load_info);
    } else {
      GlobalFrameRoutingId id(request_info->GetChildID(),
                              request_info->GetRenderFrameID());
      auto existing = frame_infos.find(id);
      if (existing == frame_infos.end() ||
          LoadInfoIsMoreInteresting(load_info, existing->second)) {
        frame_infos[id] = std::move(load_info);
      }
    }
  }

  for (auto it : frame_infos) {
    infos->push_back(std::move(it.second));
  }
  return infos;
}

void ResourceDispatcherHostImpl::UpdateLoadInfo() {
  std::unique_ptr<LoadInfoList> infos(GetInterestingPerFrameLoadInfos());

  // We need to be able to compare all requests to find the most important one
  // per tab. Since some requests may be navigation requests and we don't have
  // their render frame routing IDs yet (which is what we have for subresource
  // requests), we must go to the UI thread and compare the requests using their
  // WebContents.
  DCHECK(!waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = true;
  main_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(UpdateLoadStateOnUI, loader_delegate_, std::move(infos)),
      base::BindOnce(&ResourceDispatcherHostImpl::AckUpdateLoadInfo,
                     weak_factory_on_io_.GetWeakPtr()));
}

void ResourceDispatcherHostImpl::AckUpdateLoadInfo() {
  DCHECK(waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = false;
  MaybeStartUpdateLoadInfoTimer();
}

void ResourceDispatcherHostImpl::MaybeStartUpdateLoadInfoTimer() {
  // If shutdown has occurred, |update_load_info_timer_| is nullptr.
  if (!is_shutdown_ && !waiting_on_load_state_ack_ &&
      !update_load_info_timer_->IsRunning() &&
      scheduler_->DeprecatedHasLoadingClients() && !pending_loaders_.empty()) {
    update_load_info_timer_->Start(
        FROM_HERE, TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHostImpl::UpdateLoadInfo);
  }
}

void ResourceDispatcherHostImpl::BlockRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(blocked_loaders_map_.find(global_routing_id) ==
         blocked_loaders_map_.end())
      << "BlockRequestsForRoute called  multiple time for the same RFH";
  blocked_loaders_map_[global_routing_id] =
      std::make_unique<BlockedLoadersList>();
}

void ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  ProcessBlockedRequestsForRoute(global_routing_id, false);
}

void ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id) {
  ProcessBlockedRequestsForRoute(global_routing_id, true);
}

void ResourceDispatcherHostImpl::ProcessBlockedRequestsForRoute(
    const GlobalFrameRoutingId& global_routing_id,
    bool cancel_requests) {
  auto iter = blocked_loaders_map_.find(global_routing_id);
  if (iter == blocked_loaders_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedLoadersList* loaders = iter->second.get();
  std::unique_ptr<BlockedLoadersList> deleter(std::move(iter->second));

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_loaders_map_.erase(iter);

  for (std::unique_ptr<ResourceLoader>& loader : *loaders) {
    ResourceRequestInfoImpl* info = loader->GetRequestInfo();
    if (cancel_requests) {
      IncrementOutstandingRequestsMemory(-1, *info);
    } else {
      StartLoading(info, std::move(loader));
    }
  }
}

ResourceDispatcherHostImpl::HttpAuthRelationType
ResourceDispatcherHostImpl::HttpAuthRelationTypeOf(
    const GURL& request_url,
    const GURL& first_party) {
  if (!first_party.is_valid())
    return HTTP_AUTH_RELATION_TOP;

  if (net::registry_controlled_domains::SameDomainOrHost(
          first_party, request_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // If the first party is secure but the subresource is not, this is
    // mixed-content. Do not allow the image.
    if (!allow_cross_origin_auth_prompt() && IsOriginSecure(first_party) &&
        !IsOriginSecure(request_url)) {
      return HTTP_AUTH_RELATION_BLOCKED_CROSS;
    }
    return HTTP_AUTH_RELATION_SAME_DOMAIN;
  }

  if (allow_cross_origin_auth_prompt())
    return HTTP_AUTH_RELATION_ALLOWED_CROSS;

  return HTTP_AUTH_RELATION_BLOCKED_CROSS;
}

bool ResourceDispatcherHostImpl::allow_cross_origin_auth_prompt() {
  return allow_cross_origin_auth_prompt_;
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(
    const GlobalRequestID& id) const {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());

  auto i = pending_loaders_.find(id);
  if (i == pending_loaders_.end())
    return nullptr;

  return i->second.get();
}

ResourceLoader* ResourceDispatcherHostImpl::GetLoader(int child_id,
                                                      int request_id) const {
  return GetLoader(GlobalRequestID(child_id, request_id));
}

bool ResourceDispatcherHostImpl::ShouldServiceRequest(
    int child_id,
    const network::ResourceRequest& request_data,
    const net::HttpRequestHeaders& headers,
    ResourceRequesterInfo* requester_info,
    ResourceContext* resource_context) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(child_id, request_data.url)) {
    VLOG(1) << "Denied unauthorized request for "
            << request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is using an illegal Origin header.  If so, kill it.
  std::string origin_string;
  bool has_origin =
      headers.GetHeader("Origin", &origin_string) && origin_string != "null";
  if (has_origin) {
    GURL origin(origin_string);
    if (!policy->CanSetAsOriginHeader(child_id, origin)) {
      VLOG(1) << "Killed renderer for illegal origin: " << origin_string;
      bad_message::ReceivedBadMessage(requester_info->filter(),
                                      bad_message::RDH_ILLEGAL_ORIGIN);
      return false;
    }
  }

  // Check if the renderer is permitted to upload the requested files.
  if (!policy->CanReadRequestBody(child_id,
                                  requester_info->file_system_context(),
                                  request_data.request_body)) {
    NOTREACHED() << "Denied unauthorized upload";
    return false;
  }

  // Check that |plugin_child_id|, if present, is actually a plugin process.
  if (!ValidatePluginChildId(request_data.plugin_child_id)) {
    NOTREACHED() << "Invalid request_data.plugin_child_id: "
                 << request_data.plugin_child_id << " (" << child_id << ", "
                 << request_data.render_frame_id << ")";
    return false;
  }

  return true;
}

std::unique_ptr<ResourceHandler>
ResourceDispatcherHostImpl::HandleDownloadStarted(
    net::URLRequest* request,
    std::unique_ptr<ResourceHandler> handler,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request) {
  if (delegate()) {
    const ResourceRequestInfoImpl* request_info(
        ResourceRequestInfoImpl::ForRequest(request));
    std::vector<std::unique_ptr<ResourceThrottle>> throttles;
    delegate()->DownloadStarting(request, request_info->GetContext(),
                                 is_content_initiated, true, is_new_request,
                                 &throttles);
    if (!throttles.empty()) {
      handler.reset(new ThrottlingResourceHandler(std::move(handler), request,
                                                  std::move(throttles)));
    }
  }
  return handler;
}

void ResourceDispatcherHostImpl::RunAuthRequiredCallback(
    GlobalRequestID request_id,
    const base::Optional<net::AuthCredentials>& credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResourceLoader* loader = GetLoader(request_id);
  if (!loader)
    return;

  net::URLRequest* url_request = loader->request();
  if (!url_request)
    return;

  if (!credentials.has_value()) {
    url_request->CancelAuth();
  } else {
    url_request->SetAuth(credentials.value());
  }

  // Clears the LoginDelegate associated with the request.
  loader->ClearLoginDelegate();
}

// static
void ResourceDispatcherHostImpl::RecordFetchRequestMode(
    const GURL& url,
    base::StringPiece method,
    network::mojom::FetchRequestMode mode) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  std::string lower_method = base::ToLowerASCII(method);
  if (lower_method == "get") {
    UMA_HISTOGRAM_ENUMERATION("Net.ResourceDispatcherHost.RequestMode.Get",
                              mode);
  } else if (lower_method == "post") {
    UMA_HISTOGRAM_ENUMERATION("Net.ResourceDispatcherHost.RequestMode.Post",
                              mode);
    if (url.has_port()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Net.ResourceDispatcherHost.RequestMode.Post.WithPort", mode);
    }
  }
}

// static
// We have this function at the bottom of this file because it confuses
// syntax highliting.
net::NetworkTrafficAnnotationTag
ResourceDispatcherHostImpl::GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("resource_dispatcher_host",
                                             R"(
        semantics {
          sender: "Resource Dispatcher Host"
          description:
            "Navigation-initiated request or renderer process initiated "
            "request, which includes all resources for normal page loads, "
            "chrome URLs, resources for installed extensions, as well as "
            "downloads."
          trigger:
            "Navigating to a URL or downloading a file. A webpage, "
            "ServiceWorker, chrome:// page, or extension may also initiate "
            "requests in the background."
          data: "Anything the initiator wants to send."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user or per-app cookie store"
          setting: "These requests cannot be disabled."
          policy_exception_justification:
            "Not implemented. Without these requests, Chrome will be unable to "
            "load any webpage."
        })");
}

}  // namespace content
