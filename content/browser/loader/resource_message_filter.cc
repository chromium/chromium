// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_filter.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_switches.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {
namespace {
network::mojom::URLLoaderFactory* g_test_factory;
ResourceMessageFilter* g_current_filter;

int GetFrameTreeNodeId(int render_process_host_id, int render_frame_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_host_id, render_frame_host_id);
  return render_frame_host ? render_frame_host->GetFrameTreeNodeId() : -1;
}

}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    int child_id,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    storage::FileSystemContext* file_system_context,
    ServiceWorkerContextWrapper* service_worker_context,
    PrefetchURLLoaderService* prefetch_url_loader_service,
    const SharedCorsOriginAccessList* shared_cors_origin_access_list,
    const GetContextsCallback& get_contexts_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner)
    : BrowserMessageFilter(ResourceMsgStart),
      BrowserAssociatedInterface<network::mojom::URLLoaderFactory>(this, this),
      is_channel_closed_(false),
      requester_info_(
          ResourceRequesterInfo::CreateForRenderer(child_id,
                                                   appcache_service,
                                                   blob_storage_context,
                                                   file_system_context,
                                                   service_worker_context,
                                                   get_contexts_callback)),
      prefetch_url_loader_service_(prefetch_url_loader_service),
      shared_cors_origin_access_list_(shared_cors_origin_access_list),
      io_thread_task_runner_(io_thread_runner),
      weak_ptr_factory_(this) {}

ResourceMessageFilter::~ResourceMessageFilter() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(is_channel_closed_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void ResourceMessageFilter::OnFilterAdded(IPC::Channel*) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  InitializeOnIOThread();
}

void ResourceMessageFilter::OnChannelClosing() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());

  prefetch_url_loader_service_ = nullptr;
  url_loader_factory_ = nullptr;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  ResourceDispatcherHostImpl::Get()->CancelRequestsForProcess(
      requester_info_->child_id());

  weak_ptr_factory_.InvalidateWeakPtrs();
  is_channel_closed_ = true;
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  return false;
}

void ResourceMessageFilter::OnDestruct() const {
  // Destroy the filter on the IO thread since that's where its weak pointers
  // are being used.
  if (io_thread_task_runner_->BelongsToCurrentThread()) {
    delete this;
  } else {
    io_thread_task_runner_->DeleteSoon(FROM_HERE, this);
  }
}

base::WeakPtr<ResourceMessageFilter> ResourceMessageFilter::GetWeakPtr() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  return is_channel_closed_ ? nullptr : weak_ptr_factory_.GetWeakPtr();
}

void ResourceMessageFilter::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (g_test_factory && !g_current_filter) {
    g_current_filter = this;
    g_test_factory->CreateLoaderAndStart(std::move(request), routing_id,
                                         request_id, options, url_request,
                                         std::move(client), traffic_annotation);
    g_current_filter = nullptr;
    return;
  }

  // TODO(kinuko): Remove this flag guard when we have more confidence, this
  // doesn't need to be paired up with SignedExchange feature.
  if (signed_exchange_utils::IsSignedExchangeHandlingEnabled() &&
      url_request.resource_type == RESOURCE_TYPE_PREFETCH &&
      prefetch_url_loader_service_) {
    prefetch_url_loader_service_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get()),
        base::BindRepeating(&GetFrameTreeNodeId, child_id(),
                            url_request.render_frame_id));
    return;
  }

  url_loader_factory_->CreateLoaderAndStart(
      std::move(request), routing_id, request_id, options, url_request,
      std::move(client), traffic_annotation);
}

void ResourceMessageFilter::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  if (!url_loader_factory_) {
    queued_clone_requests_.emplace_back(std::move(request));
    return;
  }
  url_loader_factory_->Clone(std::move(request));
}

int ResourceMessageFilter::child_id() const {
  return requester_info_->child_id();
}

void ResourceMessageFilter::InitializeForTest() {
  InitializeOnIOThread();
}

void ResourceMessageFilter::SetNetworkFactoryForTesting(
    network::mojom::URLLoaderFactory* test_factory) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!test_factory || !g_test_factory);
  g_test_factory = test_factory;
}

ResourceMessageFilter* ResourceMessageFilter::GetCurrentForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return g_current_filter;
}

void ResourceMessageFilter::InitializeOnIOThread() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  // The WeakPtr of the filter must be created on the IO thread. So sets the
  // WeakPtr of |requester_info_| now.
  requester_info_->set_filter(GetWeakPtr());
  url_loader_factory_ = std::make_unique<network::cors::CORSURLLoaderFactory>(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity),
      std::make_unique<URLLoaderFactoryImpl>(requester_info_),
      base::BindRepeating(&ResourceDispatcherHostImpl::CancelRequest,
                          base::Unretained(ResourceDispatcherHostImpl::Get()),
                          requester_info_->child_id()),
      &shared_cors_origin_access_list_->GetOriginAccessList());

  std::vector<network::mojom::URLLoaderFactoryRequest> requests =
      std::move(queued_clone_requests_);
  for (auto& request : requests)
    Clone(std::move(request));
  queued_clone_requests_.clear();
}

}  // namespace content
