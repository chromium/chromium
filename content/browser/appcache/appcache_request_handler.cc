// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/appcache/appcache_url_request_job.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

bool g_running_in_tests = false;

}  // namespace

AppCacheRequestHandler::AppCacheRequestHandler(
    AppCacheHost* host,
    ResourceType resource_type,
    bool should_reset_appcache,
    std::unique_ptr<AppCacheRequest> request)
    : host_(host),
      resource_type_(resource_type),
      should_reset_appcache_(should_reset_appcache),
      is_waiting_for_cache_selection_(false),
      found_group_id_(0),
      found_cache_id_(0),
      found_network_namespace_(false),
      cache_entry_not_found_(false),
      is_delivering_network_response_(false),
      maybe_load_resource_executed_(false),
      cache_id_(kAppCacheNoCacheId),
      service_(host_->service()),
      request_(std::move(request)),
      weak_factory_(this) {
  DCHECK(host_);
  DCHECK(service_);
  host_->AddObserver(this);
  service_->AddObserver(this);
}

AppCacheRequestHandler::~AppCacheRequestHandler() {
  if (host_) {
    storage()->CancelDelegateCallbacks(this);
    host_->RemoveObserver(this);
  }
  if (service_)
    service_->RemoveObserver(this);

  if (job_ && job_->AsURLLoaderJob())
    job_->AsURLLoaderJob()->DeleteIfNeeded();
}

AppCacheStorage* AppCacheRequestHandler::storage() const {
  DCHECK(host_);
  return host_->storage();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadResource(
    net::NetworkDelegate* network_delegate) {
  maybe_load_resource_executed_ = true;
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_) {
    return nullptr;
  }

  // This method can get called multiple times over the life
  // of a request. The case we detect here is having scheduled
  // delivery of a "network response" using a job set up on an
  // earlier call through this method. To send the request through
  // to the network involves restarting the request altogether,
  // which will call through to our interception layer again.
  // This time through, we return NULL so the request hits the wire.
  if (is_delivering_network_response_) {
    is_delivering_network_response_ = false;
    return nullptr;
  }

  // Clear out our 'found' fields since we're starting a request for a
  // new resource, any values in those fields are no longer valid.
  found_entry_ = AppCacheEntry();
  found_fallback_entry_ = AppCacheEntry();
  found_cache_id_ = kAppCacheNoCacheId;
  found_manifest_url_ = GURL();
  found_network_namespace_ = false;

  std::unique_ptr<AppCacheJob> job;
  if (is_main_resource())
    job = MaybeLoadMainResource(network_delegate);
  else
    job = MaybeLoadSubResource(network_delegate);

  // If its been setup to deliver a network response, we can just delete
  // it now and return NULL instead to achieve that since it couldn't
  // have been started yet.
  if (job && job->IsDeliveringNetworkResponse()) {
    DCHECK(!job->IsStarted());
    if (job->AsURLLoaderJob()) {
      job.release();  // AppCacheURLLoaderJob always deletes itself.
      job_ = nullptr;
    } else {
      job.reset();
    }
  }

  return job.release();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadFallbackForRedirect(
    net::NetworkDelegate* network_delegate,
    const GURL& location) {
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_)
    return nullptr;
  if (is_main_resource())
    return nullptr;
  // TODO(vabr) This is a temporary fix (see crbug/141114). We should get rid of
  // it once a more general solution to crbug/121325 is in place.
  if (!maybe_load_resource_executed_)
    return nullptr;
  if (request_->GetURL().GetOrigin() == location.GetOrigin())
    return nullptr;

  DCHECK(!job_.get());  // our jobs never generate redirects

  std::unique_ptr<AppCacheJob> job;
  if (found_fallback_entry_.has_response_id()) {
    // 6.9.6, step 4: If this results in a redirect to another origin,
    // get the resource of the fallback entry.
    job = CreateJob(network_delegate);
    DeliverAppCachedResponse(found_fallback_entry_, found_cache_id_,
                             found_manifest_url_, true,
                             found_namespace_entry_url_);
  } else if (!found_network_namespace_) {
    // 6.9.6, step 6: Fail the resource load.
    job = CreateJob(network_delegate);
    DeliverErrorResponse();
  } else {
    // 6.9.6 step 3 and 5: Fetch the resource normally.
  }

  return job.release();
}

AppCacheJob* AppCacheRequestHandler::MaybeLoadFallbackForResponse(
    net::NetworkDelegate* network_delegate) {
  if (!host_ ||
      !AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(request_.get()) ||
      cache_entry_not_found_)
    return nullptr;
  if (!found_fallback_entry_.has_response_id())
    return nullptr;

  if (request_->IsCancelled()) {
    // 6.9.6, step 4: But not if the user canceled the download.
    return nullptr;
  }

  // We don't fallback for responses that we delivered.
  if (job_.get()) {
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      DCHECK(!job_->IsDeliveringNetworkResponse());
      return nullptr;
    } else if (job_->IsDeliveringAppCacheResponse() ||
               job_->IsDeliveringErrorResponse()) {
      return nullptr;
    }
  }

  if (request_->IsSuccess()) {
    int code_major = request_->GetResponseCode() / 100;
    if (code_major !=4 && code_major != 5)
      return nullptr;

    // Servers can override the fallback behavior with a response header.
    const std::string kFallbackOverrideHeader(
        "x-chromium-appcache-fallback-override");
    const std::string kFallbackOverrideValue(
        "disallow-fallback");
    std::string header_value;
    header_value = request_->GetResponseHeaderByName(kFallbackOverrideHeader);
    if (header_value == kFallbackOverrideValue)
      return nullptr;
  }

  // 6.9.6, step 4: If this results in a 4xx or 5xx status code
  // or there were network errors, get the resource of the fallback entry.

  std::unique_ptr<AppCacheJob> job = CreateJob(network_delegate);

  DeliverAppCachedResponse(found_fallback_entry_, found_cache_id_,
                           found_manifest_url_, true,
                           found_namespace_entry_url_);
  return job.release();
}

void AppCacheRequestHandler::GetExtraResponseInfo(int64_t* cache_id,
                                                  GURL* manifest_url) {
  *cache_id = cache_id_;
  *manifest_url = manifest_url_;
}

// static
std::unique_ptr<AppCacheRequestHandler>
AppCacheRequestHandler::InitializeForMainResourceNetworkService(
    const network::ResourceRequest& request,
    base::WeakPtr<AppCacheHost> appcache_host,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory) {
  std::unique_ptr<AppCacheRequestHandler> handler =
      appcache_host->CreateRequestHandler(
          AppCacheURLLoaderRequest::Create(request),
          static_cast<ResourceType>(request.resource_type),
          request.should_reset_appcache);
  handler->network_loader_factory_ = std::move(network_loader_factory);
  handler->appcache_host_ = std::move(appcache_host);
  return handler;
}

void AppCacheRequestHandler::OnDestructionImminent(AppCacheHost* host) {
  storage()->CancelDelegateCallbacks(this);
  host_ = nullptr;  // no need to RemoveObserver, the host is being deleted

  // Since the host is being deleted, we don't have to complete any job
  // that is current running. It's destined for the bit bucket anyway.
  if (job_.get()) {
    if (job_->AsURLRequestJob())
      job_->AsURLRequestJob()->Kill();
    job_.reset();
  }
}

void AppCacheRequestHandler::OnServiceDestructionImminent(
    AppCacheServiceImpl* service) {
  service_ = nullptr;
  if (!host_) {
    DCHECK(!job_);
    return;
  }
  host_->RemoveObserver(this);
  OnDestructionImminent(host_);
}

void AppCacheRequestHandler::DeliverAppCachedResponse(
    const AppCacheEntry& entry,
    int64_t cache_id,
    const GURL& manifest_url,
    bool is_fallback,
    const GURL& namespace_entry_url) {
  DCHECK(host_ && job_.get() && job_->IsWaiting());
  DCHECK(entry.has_response_id());

  // Cache information about the response, for use by GetExtraResponseInfo.
  cache_id_ = cache_id;
  manifest_url_ = manifest_url;

  if (IsResourceTypeFrame(resource_type_) && !namespace_entry_url.is_empty())
    host_->NotifyMainResourceIsNamespaceEntry(namespace_entry_url);

  job_->DeliverAppCachedResponse(manifest_url, cache_id, entry, is_fallback);
}

void AppCacheRequestHandler::DeliverErrorResponse() {
  DCHECK(job_.get() && job_->IsWaiting());
  DCHECK_EQ(kAppCacheNoCacheId, cache_id_);
  DCHECK(manifest_url_.is_empty());
  job_->DeliverErrorResponse();
}

void AppCacheRequestHandler::DeliverNetworkResponse() {
  DCHECK(job_.get() && job_->IsWaiting());
  DCHECK_EQ(kAppCacheNoCacheId, cache_id_);
  DCHECK(manifest_url_.is_empty());
  job_->DeliverNetworkResponse();
}

void AppCacheRequestHandler::OnPrepareToRestartURLRequest() {
  DCHECK(job_->AsURLRequestJob());
  DCHECK(job_->IsDeliveringNetworkResponse() || job_->IsCacheEntryNotFound());

  // Any information about the source of the response is no longer relevant.
  cache_id_ = kAppCacheNoCacheId;
  manifest_url_ = GURL();

  cache_entry_not_found_ = job_->IsCacheEntryNotFound();
  is_delivering_network_response_ = job_->IsDeliveringNetworkResponse();

  storage()->CancelDelegateCallbacks(this);

  job_.reset();
}

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::CreateJob(
    net::NetworkDelegate* network_delegate) {
  std::unique_ptr<AppCacheJob> job;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    job.reset(new AppCacheURLLoaderJob(request_->AsURLLoaderRequest(),
                                       storage(), std::move(loader_callback_)));
  } else {
    job.reset(new AppCacheURLRequestJob(
        request_->GetURLRequest(), network_delegate, storage(), host_,
        is_main_resource(),
        base::BindOnce(&AppCacheRequestHandler::OnPrepareToRestartURLRequest,
                       base::Unretained(this))));
  }
  job_ = job->GetWeakPtr();
  return job;
}

// Main-resource handling ----------------------------------------------

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::MaybeLoadMainResource(
    net::NetworkDelegate* network_delegate) {
  DCHECK(!job_.get());
  DCHECK(host_);

  // If a page falls into the scope of a ServiceWorker, any matching AppCaches
  // should be ignored. This depends on the ServiceWorker handler being invoked
  // prior to the AppCache handler.
  // TODO(ananta/michaeln)
  // We need to handle this for AppCache requests initiated for the network
  // service
  if (request_->GetURLRequest() &&
      ServiceWorkerRequestHandler::IsControlledByServiceWorker(
          request_->GetURLRequest())) {
    host_->enable_cache_selection(false);
    return nullptr;
  }

  if (storage()->IsInitialized() &&
      !base::ContainsKey(*service_->storage()->usage_map(),
                         url::Origin::Create(request_->GetURL()))) {
    return nullptr;
  }

  host_->enable_cache_selection(true);

  const AppCacheHost* spawning_host =
      (resource_type_ == RESOURCE_TYPE_SHARED_WORKER) ?
      host_ : host_->GetSpawningHost();
  GURL preferred_manifest_url = spawning_host ?
      spawning_host->preferred_manifest_url() : GURL();

  // We may have to wait for our storage query to complete, but
  // this query can also complete syncrhonously.
  std::unique_ptr<AppCacheJob> job = CreateJob(network_delegate);
  storage()->FindResponseForMainRequest(request_->GetURL(),
                                        preferred_manifest_url, this);
  return job;
}

void AppCacheRequestHandler::OnMainResponseFound(
    const GURL& url,
    const AppCacheEntry& entry,
    const GURL& namespace_entry_url,
    const AppCacheEntry& fallback_entry,
    int64_t cache_id,
    int64_t group_id,
    const GURL& manifest_url) {
  DCHECK(host_);
  DCHECK(is_main_resource());
  DCHECK(!entry.IsForeign());
  DCHECK(!fallback_entry.IsForeign());
  DCHECK(!(entry.has_response_id() && fallback_entry.has_response_id()));

  // Request may have been canceled, but not yet deleted, while waiting on
  // the cache.
  if (!job_.get())
    return;

  AppCachePolicy* policy = host_->service()->appcache_policy();
  bool was_blocked_by_policy = !manifest_url.is_empty() && policy &&
      !policy->CanLoadAppCache(manifest_url, host_->first_party_url());

  if (was_blocked_by_policy) {
    if (IsResourceTypeFrame(resource_type_)) {
      host_->NotifyMainResourceBlocked(manifest_url);
    } else {
      DCHECK_EQ(resource_type_, RESOURCE_TYPE_SHARED_WORKER);
      host_->frontend()->OnContentBlocked(host_->host_id(), manifest_url);
    }
    DeliverNetworkResponse();
    return;
  }

  if (should_reset_appcache_ && !manifest_url.is_empty()) {
    host_->service()->DeleteAppCacheGroup(manifest_url,
                                          net::CompletionOnceCallback());
    DeliverNetworkResponse();
    return;
  }

  if (IsMainResourceType(resource_type_) && cache_id != kAppCacheNoCacheId) {
    // AppCacheHost loads and holds a reference to the main resource cache
    // for two reasons, firstly to preload the cache into the working set
    // in advance of subresource loads happening, secondly to prevent the
    // AppCache from falling out of the working set on frame navigations.
    host_->LoadMainResourceCache(cache_id);
    host_->set_preferred_manifest_url(manifest_url);
  }

  // 6.11.1 Navigating across documents, steps 10 and 14.

  found_entry_ = entry;
  found_namespace_entry_url_ = namespace_entry_url;
  found_fallback_entry_ = fallback_entry;
  found_cache_id_ = cache_id;
  found_group_id_ = group_id;
  found_manifest_url_ = manifest_url;
  found_network_namespace_ = false;  // not applicable to main requests

  if (found_entry_.has_response_id()) {
    DCHECK(!found_fallback_entry_.has_response_id());
    DeliverAppCachedResponse(found_entry_, found_cache_id_, found_manifest_url_,
                             false, found_namespace_entry_url_);
  } else {
    DeliverNetworkResponse();
  }
}

// NetworkService loading:
void AppCacheRequestHandler::RunLoaderCallbackForMainResource(
    LoaderCallback callback,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  // For now let |this| always also return the subresource loader
  // if (and only if) this returns a non-null |start_loader_callback|
  // for handling the main resource.
  if (handler)
    should_create_subresource_loader_ = true;
  std::move(callback).Run(std::move(handler));
}

// Sub-resource handling ----------------------------------------------

std::unique_ptr<AppCacheJob> AppCacheRequestHandler::MaybeLoadSubResource(
    net::NetworkDelegate* network_delegate) {
  DCHECK(!job_.get());

  if (host_->is_selection_pending()) {
    // We have to wait until cache selection is complete and the
    // selected cache is loaded.
    is_waiting_for_cache_selection_ = true;
    return CreateJob(network_delegate);
  }

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete() ||
      host_->associated_cache()->owning_group()->is_being_deleted()) {
    return nullptr;
  }

  std::unique_ptr<AppCacheJob> job = CreateJob(network_delegate);
  ContinueMaybeLoadSubResource();
  return job;
}

void AppCacheRequestHandler::ContinueMaybeLoadSubResource() {
  // 6.9.6 Changes to the networking model
  // If the resource is not to be fetched using the HTTP GET mechanism or
  // equivalent ... then fetch the resource normally.
  DCHECK(job_.get());
  DCHECK(host_->associated_cache() && host_->associated_cache()->is_complete());

  const GURL& url = request_->GetURL();
  AppCache* cache = host_->associated_cache();
  storage()->FindResponseForSubRequest(
      host_->associated_cache(), url,
      &found_entry_, &found_fallback_entry_, &found_network_namespace_);

  if (found_entry_.has_response_id()) {
    // Step 2: If there's an entry, get it instead.
    DCHECK(!found_network_namespace_ &&
           !found_fallback_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_group_id_ = cache->owning_group()->group_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverAppCachedResponse(found_entry_, found_cache_id_, found_manifest_url_,
                             false, GURL());
    return;
  }

  if (found_fallback_entry_.has_response_id()) {
    // Step 4: Fetch the resource normally, if this results
    // in certain conditions, then use the fallback.
    DCHECK(!found_network_namespace_ &&
           !found_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverNetworkResponse();
    return;
  }

  if (found_network_namespace_) {
    // Step 3 and 5: Fetch the resource normally.
    DCHECK(!found_entry_.has_response_id() &&
           !found_fallback_entry_.has_response_id());
    DeliverNetworkResponse();
    return;
  }

  // Step 6: Fail the resource load.
  DeliverErrorResponse();
}

void AppCacheRequestHandler::OnCacheSelectionComplete(AppCacheHost* host) {
  DCHECK(host == host_);

  // Request may have been canceled, but not yet deleted, while waiting on
  // the cache.
  if (!job_.get())
    return;

  if (is_main_resource())
    return;
  if (!is_waiting_for_cache_selection_)
    return;

  is_waiting_for_cache_selection_ = false;

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete()) {
    DeliverNetworkResponse();
    return;
  }

  ContinueMaybeLoadSubResource();
}

void AppCacheRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  loader_callback_ =
      base::BindOnce(&AppCacheRequestHandler::RunLoaderCallbackForMainResource,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  // TODO(crbug.com/876531): Figure out how AppCache interception should
  // interact with URLLoaderThrottles. It might be incorrect to store
  // |tentative_resource_request| here, since throttles can rewrite headers
  // between now and when the request handler passed to |loader_callback_| is
  // invoked.
  request_->AsURLLoaderRequest()->set_request(tentative_resource_request);

  MaybeLoadResource(nullptr);
  // If a job is created, the job assumes ownership of the callback and
  // the responsibility to call it. If no job is created, we call it with
  // a null callback to let our client know we have no loader for this request.
  if (loader_callback_)
    std::move(loader_callback_).Run({});
}

bool AppCacheRequestHandler::MaybeCreateLoaderForResponse(
    const GURL& request_url,
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* loader,
    network::mojom::URLLoaderClientRequest* client_request,
    ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  // The sync interface of this method is inherited from the
  // NavigationLoaderInterceptor class. The LoaderCallback created here is
  // invoked synchronously in fallback cases, and only when there really is
  // a loader to start.
  bool was_called = false;
  loader_callback_ = base::BindOnce(
      [](const network::ResourceRequest& resource_request,
         network::mojom::URLLoaderPtr* loader,
         network::mojom::URLLoaderClientRequest* client_request,
         bool* was_called,
         SingleRequestURLLoaderFactory::RequestHandler handler) {
        *was_called = true;
        network::mojom::URLLoaderClientPtr client;
        *client_request = mojo::MakeRequest(&client);
        std::move(handler).Run(resource_request, mojo::MakeRequest(loader),
                               std::move(client));
      },
      *(request_->AsURLLoaderRequest()->GetResourceRequest()), loader,
      client_request, &was_called);
  request_->AsURLLoaderRequest()->set_response(response);
  if (!MaybeLoadFallbackForResponse(nullptr)) {
    DCHECK(!was_called);
    loader_callback_.Reset();
    return false;
  }
  DCHECK(was_called);
  if (IsMainResourceType(resource_type_))
    should_create_subresource_loader_ = true;
  return true;
}

base::Optional<SubresourceLoaderParams>
AppCacheRequestHandler::MaybeCreateSubresourceLoaderParams() {
  if (!should_create_subresource_loader_)
    return base::nullopt;

  // The factory is destroyed when the renderer drops the connection.
  network::mojom::URLLoaderFactoryPtr factory_ptr;
  AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
      network_loader_factory_, appcache_host_, &factory_ptr);

  SubresourceLoaderParams params;
  params.loader_factory_info = factory_ptr.PassInterface();
  return base::Optional<SubresourceLoaderParams>(std::move(params));
}

void AppCacheRequestHandler::MaybeCreateSubresourceLoader(
    const network::ResourceRequest& resource_request,
    LoaderCallback loader_callback) {
  DCHECK(!job_);
  DCHECK(!is_main_resource());
  // AppCache doesn't use the fallback_callback.
  FallbackCallback fallback_callback = base::DoNothing();

  // Subresource loads start out just like a main resource loads, but they go
  // down different branches along the way to completion.
  MaybeCreateLoader(resource_request, nullptr, std::move(loader_callback),
                    std::move(fallback_callback));
}

void AppCacheRequestHandler::MaybeFallbackForSubresourceResponse(
    const network::ResourceResponseHead& response,
    LoaderCallback loader_callback) {
  DCHECK(!job_);
  DCHECK(!is_main_resource());
  loader_callback_ = std::move(loader_callback);
  request_->AsURLLoaderRequest()->set_response(response);
  MaybeLoadFallbackForResponse(nullptr);
  if (loader_callback_)
    std::move(loader_callback_).Run({});
}

void AppCacheRequestHandler::MaybeFallbackForSubresourceRedirect(
    const net::RedirectInfo& redirect_info,
    LoaderCallback loader_callback) {
  DCHECK(!job_);
  DCHECK(!is_main_resource());
  loader_callback_ = std::move(loader_callback);
  MaybeLoadFallbackForRedirect(nullptr, redirect_info.new_url);
  if (loader_callback_)
    std::move(loader_callback_).Run({});
}

void AppCacheRequestHandler::MaybeFollowSubresourceRedirect(
    const net::RedirectInfo& redirect_info,
    LoaderCallback loader_callback) {
  DCHECK(!job_);
  DCHECK(!is_main_resource());
  loader_callback_ = std::move(loader_callback);
  request_->AsURLLoaderRequest()->UpdateWithRedirectInfo(redirect_info);
  MaybeLoadResource(nullptr);
  if (loader_callback_)
    std::move(loader_callback_).Run({});
}

// static
void AppCacheRequestHandler::SetRunningInTests(bool in_tests) {
  g_running_in_tests = in_tests;
}

// static
bool AppCacheRequestHandler::IsRunningInTests() {
  return g_running_in_tests;
}

}  // namespace content
