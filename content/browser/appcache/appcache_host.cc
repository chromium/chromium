// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_host.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/public/common/content_features.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

AppCacheInfo CreateCacheInfo(const AppCache* cache,
                             const GURL& manifest_url,
                             AppCacheStatus status) {
  AppCacheInfo info;
  info.manifest_url = manifest_url;
  info.status = status;

  if (!cache)
    return info;

  info.cache_id = cache->cache_id();

  if (!cache->is_complete())
    return info;

  DCHECK(cache->owning_group());
  info.is_complete = true;
  info.group_id = cache->owning_group()->group_id();
  info.last_update_time = cache->update_time();
  info.creation_time = cache->owning_group()->creation_time();
  info.size = cache->cache_size();
  return info;
}

}  // namespace

AppCacheHost::AppCacheHost(int host_id,
                           AppCacheFrontend* frontend,
                           AppCacheServiceImpl* service)
    : host_id_(host_id),
      spawning_host_id_(kAppCacheNoHostId),
      spawning_process_id_(0),
      parent_host_id_(kAppCacheNoHostId),
      parent_process_id_(0),
      pending_main_resource_cache_id_(kAppCacheNoCacheId),
      pending_selected_cache_id_(kAppCacheNoCacheId),
      was_select_cache_called_(false),
      is_cache_selection_enabled_(true),
      frontend_(frontend),
      service_(service),
      storage_(service->storage()),
      main_resource_was_namespace_entry_(false),
      main_resource_blocked_(false),
      associated_cache_info_pending_(false),
      weak_factory_(this) {
  service_->AddObserver(this);
}

AppCacheHost::~AppCacheHost() {
  service_->RemoveObserver(this);
  for (auto& observer : observers_)
    observer.OnDestructionImminent(this);
  if (associated_cache_.get())
    associated_cache_->UnassociateHost(this);
  if (group_being_updated_.get())
    group_being_updated_->RemoveUpdateObserver(this);
  storage()->CancelDelegateCallbacks(this);
  if (service()->quota_manager_proxy() && !origin_in_use_.opaque())
    service()->quota_manager_proxy()->NotifyOriginNoLongerInUse(origin_in_use_);
}

void AppCacheHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppCacheHost::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AppCacheHost::SelectCache(const GURL& document_url,
                               const int64_t cache_document_was_loaded_from,
                               const GURL& manifest_url) {
  if (was_select_cache_called_)
    return false;

  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null() &&
         !is_selection_pending());

  was_select_cache_called_ = true;
  if (!is_cache_selection_enabled_) {
    FinishCacheSelection(nullptr, nullptr);
    return true;
  }

  origin_in_use_ = url::Origin::Create(document_url);
  if (service()->quota_manager_proxy() && !origin_in_use_.opaque())
    service()->quota_manager_proxy()->NotifyOriginInUse(origin_in_use_);

  if (main_resource_blocked_)
    frontend_->OnContentBlocked(host_id_,
                                blocked_manifest_url_);

  // 6.9.6 The application cache selection algorithm.
  // The algorithm is started here and continues in FinishCacheSelection,
  // after cache or group loading is complete.
  // Note: Foreign entries are detected on the client side and
  // MarkAsForeignEntry is called in that case, so that detection
  // step is skipped here. See WebApplicationCacheHostImpl.cc

  if (cache_document_was_loaded_from != kAppCacheNoCacheId) {
    LoadSelectedCache(cache_document_was_loaded_from);
    return true;
  }

  if (!manifest_url.is_empty() &&
      (manifest_url.GetOrigin() == document_url.GetOrigin())) {
    DCHECK(!first_party_url_.is_empty());
    AppCachePolicy* policy = service()->appcache_policy();
    if (policy &&
        !policy->CanCreateAppCache(manifest_url, first_party_url_)) {
      FinishCacheSelection(nullptr, nullptr);
      std::vector<int> host_ids(1, host_id_);
      frontend_->OnEventRaised(host_ids,
                               AppCacheEventID::APPCACHE_CHECKING_EVENT);
      frontend_->OnErrorEventRaised(
          host_ids, AppCacheErrorDetails(
                        "Cache creation was blocked by the content policy",
                        AppCacheErrorReason::APPCACHE_POLICY_ERROR, GURL(), 0,
                        false /*is_cross_origin*/));
      frontend_->OnContentBlocked(host_id_, manifest_url);
      return true;
    }
    // Note: The client detects if the document was not loaded using HTTP GET
    // and invokes SelectCache without a manifest url, so that detection step
    // is also skipped here. See WebApplicationCacheHostImpl.cc
    set_preferred_manifest_url(manifest_url);
    new_master_entry_url_ = document_url;
    LoadOrCreateGroup(manifest_url);
    return true;
  }
  // TODO(michaeln): If there was a manifest URL, the user agent may report
  // to the user that it was ignored, to aid in application development.
  FinishCacheSelection(nullptr, nullptr);
  return true;
}

bool AppCacheHost::SelectCacheForSharedWorker(int64_t appcache_id) {
  if (was_select_cache_called_)
    return false;

  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null() &&
         !is_selection_pending());

  was_select_cache_called_ = true;
  if (appcache_id != kAppCacheNoCacheId) {
    LoadSelectedCache(appcache_id);
    return true;
  }
  FinishCacheSelection(nullptr, nullptr);
  return true;
}

// TODO(michaeln): change method name to MarkEntryAsForeign for consistency
bool AppCacheHost::MarkAsForeignEntry(const GURL& document_url,
                                      int64_t cache_document_was_loaded_from) {
  if (was_select_cache_called_)
    return false;

  // The document url is not the resource url in the fallback case.
  storage()->MarkEntryAsForeign(
      main_resource_was_namespace_entry_ ? namespace_entry_url_ : document_url,
      cache_document_was_loaded_from);
  SelectCache(document_url, kAppCacheNoCacheId, GURL());
  return true;
}

void AppCacheHost::GetStatusWithCallback(GetStatusCallback callback) {
  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null());

  pending_get_status_callback_ = std::move(callback);
  if (is_selection_pending())
    return;

  DoPendingGetStatus();
}

void AppCacheHost::DoPendingGetStatus() {
  DCHECK_EQ(false, pending_get_status_callback_.is_null());

  std::move(pending_get_status_callback_).Run(GetStatus());
}

void AppCacheHost::StartUpdateWithCallback(StartUpdateCallback callback) {
  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null());

  pending_start_update_callback_ = std::move(callback);
  if (is_selection_pending())
    return;

  DoPendingStartUpdate();
}

void AppCacheHost::DoPendingStartUpdate() {
  DCHECK_EQ(false, pending_start_update_callback_.is_null());

  // 6.9.8 Application cache API
  bool success = false;
  if (associated_cache_.get() && associated_cache_->owning_group()) {
    AppCacheGroup* group = associated_cache_->owning_group();
    if (!group->is_obsolete() && !group->is_being_deleted()) {
      success = true;
      group->StartUpdate();
    }
  }

  std::move(pending_start_update_callback_).Run(success);
}

void AppCacheHost::SwapCacheWithCallback(SwapCacheCallback callback) {
  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null());

  pending_swap_cache_callback_ = std::move(callback);

  if (is_selection_pending())
    return;

  DoPendingSwapCache();
}

void AppCacheHost::DoPendingSwapCache() {
  DCHECK_EQ(false, pending_swap_cache_callback_.is_null());

  // 6.9.8 Application cache API
  bool success = false;
  if (associated_cache_.get() && associated_cache_->owning_group()) {
    if (associated_cache_->owning_group()->is_obsolete()) {
      success = true;
      AssociateNoCache(GURL());
    } else if (swappable_cache_.get()) {
      DCHECK(swappable_cache_.get() ==
             swappable_cache_->owning_group()->newest_complete_cache());
      success = true;
      AssociateCompleteCache(swappable_cache_.get());
    }
  }

  std::move(pending_swap_cache_callback_).Run(success);
}

void AppCacheHost::SetSpawningHostId(
    int spawning_process_id, int spawning_host_id) {
  spawning_process_id_ = spawning_process_id;
  spawning_host_id_ = spawning_host_id;
}

const AppCacheHost* AppCacheHost::GetSpawningHost() const {
  AppCacheBackendImpl* backend = service_->GetBackend(spawning_process_id_);
  return backend ? backend->GetHost(spawning_host_id_) : nullptr;
}

AppCacheHost* AppCacheHost::GetParentAppCacheHost() const {
  DCHECK(is_for_dedicated_worker());
  AppCacheBackendImpl* backend = service_->GetBackend(parent_process_id_);
  return backend ? backend->GetHost(parent_host_id_) : nullptr;
}

std::unique_ptr<AppCacheRequestHandler> AppCacheHost::CreateRequestHandler(
    std::unique_ptr<AppCacheRequest> request,
    ResourceType resource_type,
    bool should_reset_appcache) {
  if (is_for_dedicated_worker()) {
    AppCacheHost* parent_host = GetParentAppCacheHost();
    if (parent_host)
      return parent_host->CreateRequestHandler(
          std::move(request), resource_type, should_reset_appcache);
    return nullptr;
  }

  if (AppCacheRequestHandler::IsMainResourceType(resource_type)) {
    // Store the first party origin so that it can be used later in SelectCache
    // for checking whether the creation of the appcache is allowed.
    first_party_url_ = request->GetSiteForCookies();
    return base::WrapUnique(new AppCacheRequestHandler(
        this, resource_type, should_reset_appcache, std::move(request)));
  }

  if ((associated_cache() && associated_cache()->is_complete()) ||
      is_selection_pending()) {
    return base::WrapUnique(new AppCacheRequestHandler(
        this, resource_type, should_reset_appcache, std::move(request)));
  }
  return nullptr;
}

void AppCacheHost::GetResourceList(
    std::vector<AppCacheResourceInfo>* resource_infos) {
  if (associated_cache_.get() && associated_cache_->is_complete())
    associated_cache_->ToResourceInfoVector(resource_infos);
}

AppCacheStatus AppCacheHost::GetStatus() {
  // 6.9.8 Application cache API
  AppCache* cache = associated_cache();
  if (!cache)
    return AppCacheStatus::APPCACHE_STATUS_UNCACHED;

  // A cache without an owning group represents the cache being constructed
  // during the application cache update process.
  if (!cache->owning_group())
    return AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;

  if (cache->owning_group()->is_obsolete())
    return AppCacheStatus::APPCACHE_STATUS_OBSOLETE;
  if (cache->owning_group()->update_status() == AppCacheGroup::CHECKING)
    return AppCacheStatus::APPCACHE_STATUS_CHECKING;
  if (cache->owning_group()->update_status() == AppCacheGroup::DOWNLOADING)
    return AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
  if (swappable_cache_.get())
    return AppCacheStatus::APPCACHE_STATUS_UPDATE_READY;
  return AppCacheStatus::APPCACHE_STATUS_IDLE;
}

void AppCacheHost::LoadOrCreateGroup(const GURL& manifest_url) {
  DCHECK(manifest_url.is_valid());
  pending_selected_manifest_url_ = manifest_url;
  storage()->LoadOrCreateGroup(manifest_url, this);
}

void AppCacheHost::OnGroupLoaded(AppCacheGroup* group,
                                 const GURL& manifest_url) {
  DCHECK(manifest_url == pending_selected_manifest_url_);
  pending_selected_manifest_url_ = GURL();
  FinishCacheSelection(nullptr, group);
}

void AppCacheHost::LoadSelectedCache(int64_t cache_id) {
  DCHECK(cache_id != kAppCacheNoCacheId);
  pending_selected_cache_id_ = cache_id;
  storage()->LoadCache(cache_id, this);
}

void AppCacheHost::OnCacheLoaded(AppCache* cache, int64_t cache_id) {
  if (cache_id == pending_main_resource_cache_id_) {
    pending_main_resource_cache_id_ = kAppCacheNoCacheId;
    main_resource_cache_ = cache;
  } else if (cache_id == pending_selected_cache_id_) {
    pending_selected_cache_id_ = kAppCacheNoCacheId;
    FinishCacheSelection(cache, nullptr);
  }
}

void AppCacheHost::FinishCacheSelection(
    AppCache *cache, AppCacheGroup* group) {
  DCHECK(!associated_cache());

  // 6.9.6 The application cache selection algorithm
  if (cache) {
    // If document was loaded from an application cache, Associate document
    // with the application cache from which it was loaded. Invoke the
    // application cache update process for that cache and with the browsing
    // context being navigated.
    DCHECK(cache->owning_group());
    DCHECK(new_master_entry_url_.is_empty());
    DCHECK_EQ(cache->owning_group()->manifest_url(), preferred_manifest_url_);
    AppCacheGroup* owing_group = cache->owning_group();
    const char* kFormatString =
        "Document was loaded from Application Cache with manifest %s";
    frontend_->OnLogMessage(
        host_id_, APPCACHE_LOG_INFO,
        base::StringPrintf(
            kFormatString, owing_group->manifest_url().spec().c_str()));
    AssociateCompleteCache(cache);
    if (!owing_group->is_obsolete() && !owing_group->is_being_deleted()) {
      owing_group->StartUpdateWithHost(this);
      ObserveGroupBeingUpdated(owing_group);
    }
  } else if (group && !group->is_being_deleted()) {
    // If document was loaded using HTTP GET or equivalent, and, there is a
    // manifest URL, and manifest URL has the same origin as document.
    // Invoke the application cache update process for manifest URL, with
    // the browsing context being navigated, and with document and the
    // resource from which document was loaded as the new master resourse.
    DCHECK(!group->is_obsolete());
    DCHECK(new_master_entry_url_.is_valid());
    DCHECK_EQ(group->manifest_url(), preferred_manifest_url_);
    const char* kFormatString = group->HasCache() ?
        "Adding master entry to Application Cache with manifest %s" :
        "Creating Application Cache with manifest %s";
    frontend_->OnLogMessage(
        host_id_, APPCACHE_LOG_INFO,
        base::StringPrintf(kFormatString,
                           group->manifest_url().spec().c_str()));
    // The UpdateJob may produce one for us later.
    AssociateNoCache(preferred_manifest_url_);
    group->StartUpdateWithNewMasterEntry(this, new_master_entry_url_);
    ObserveGroupBeingUpdated(group);
  } else {
    // Otherwise, the Document is not associated with any application cache.
    new_master_entry_url_ = GURL();
    AssociateNoCache(GURL());
  }

  // Respond to pending callbacks now that we have a selection.
  if (!pending_get_status_callback_.is_null())
    DoPendingGetStatus();
  else if (!pending_start_update_callback_.is_null())
    DoPendingStartUpdate();
  else if (!pending_swap_cache_callback_.is_null())
    DoPendingSwapCache();

  for (auto& observer : observers_)
    observer.OnCacheSelectionComplete(this);
}

void AppCacheHost::OnServiceReinitialized(
    AppCacheStorageReference* old_storage_ref) {
  // We continue to use the disabled instance, but arrange for its
  // deletion when its no longer needed.
  if (old_storage_ref->storage() == storage())
    disabled_storage_reference_ = old_storage_ref;
}

void AppCacheHost::ObserveGroupBeingUpdated(AppCacheGroup* group) {
  DCHECK(!group_being_updated_.get());
  group_being_updated_ = group;
  newest_cache_of_group_being_updated_ = group->newest_complete_cache();
  group->AddUpdateObserver(this);
}

void AppCacheHost::OnUpdateComplete(AppCacheGroup* group) {
  DCHECK_EQ(group, group_being_updated_.get());
  group->RemoveUpdateObserver(this);

  // Add a reference to the newest complete cache.
  SetSwappableCache(group);

  group_being_updated_ = nullptr;
  newest_cache_of_group_being_updated_ = nullptr;

  if (associated_cache_info_pending_ && associated_cache_.get() &&
      associated_cache_->is_complete()) {
    AppCacheInfo info = CreateCacheInfo(associated_cache_.get(),
                                        preferred_manifest_url_, GetStatus());
    associated_cache_info_pending_ = false;
    // In the network service world, we need to pass the URLLoaderFactory
    // instance to the renderer which it can use to request subresources.
    // This ensures that they can be served out of the AppCache.
    MaybePassSubresourceFactory();
    frontend_->OnCacheSelected(host_id_, info);
  }
}

void AppCacheHost::SetSwappableCache(AppCacheGroup* group) {
  if (!group) {
    swappable_cache_ = nullptr;
  } else {
    AppCache* new_cache = group->newest_complete_cache();
    if (new_cache != associated_cache_.get())
      swappable_cache_ = new_cache;
    else
      swappable_cache_ = nullptr;
  }
}

void AppCacheHost::LoadMainResourceCache(int64_t cache_id) {
  DCHECK(cache_id != kAppCacheNoCacheId);
  if (pending_main_resource_cache_id_ == cache_id ||
      (main_resource_cache_.get() &&
       main_resource_cache_->cache_id() == cache_id)) {
    return;
  }
  pending_main_resource_cache_id_ = cache_id;
  storage()->LoadCache(cache_id, this);
}

void AppCacheHost::NotifyMainResourceIsNamespaceEntry(
    const GURL& namespace_entry_url) {
  main_resource_was_namespace_entry_ = true;
  namespace_entry_url_ = namespace_entry_url;
}

void AppCacheHost::NotifyMainResourceBlocked(const GURL& manifest_url) {
  main_resource_blocked_ = true;
  blocked_manifest_url_ = manifest_url;
}

base::WeakPtr<AppCacheHost> AppCacheHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheHost::MaybePassSubresourceFactory() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  // We already have a valid factory. This happens when the document was loaded
  // from the AppCache during navigation.
  if (subresource_url_factory_.get())
    return;

  network::mojom::URLLoaderFactoryPtr factory_ptr = nullptr;

  AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
      service()->url_loader_factory_getter()->GetNetworkFactory(), GetWeakPtr(),
      &factory_ptr);

  frontend_->OnSetSubresourceFactory(host_id(), std::move(factory_ptr));
}

void AppCacheHost::SetAppCacheSubresourceFactory(
    AppCacheSubresourceURLFactory* subresource_factory) {
  subresource_url_factory_ = subresource_factory->GetWeakPtr();
}

void AppCacheHost::AssociateNoCache(const GURL& manifest_url) {
  // manifest url can be empty.
  AssociateCacheHelper(nullptr, manifest_url);
}

void AppCacheHost::AssociateIncompleteCache(AppCache* cache,
                                            const GURL& manifest_url) {
  DCHECK(cache && !cache->is_complete());
  DCHECK(!manifest_url.is_empty());
  AssociateCacheHelper(cache, manifest_url);
}

void AppCacheHost::AssociateCompleteCache(AppCache* cache) {
  DCHECK(cache && cache->is_complete());
  AssociateCacheHelper(cache, cache->owning_group()->manifest_url());
}

void AppCacheHost::AssociateCacheHelper(AppCache* cache,
                                        const GURL& manifest_url) {
  if (associated_cache_.get()) {
    associated_cache_->UnassociateHost(this);
  }

  associated_cache_ = cache;
  SetSwappableCache(cache ? cache->owning_group() : nullptr);
  associated_cache_info_pending_ = cache && !cache->is_complete();
  if (cache)
    cache->AssociateHost(this);

  AppCacheInfo info = CreateCacheInfo(cache, manifest_url, GetStatus());
  // In the network service world, we need to pass the URLLoaderFactory
  // instance to the renderer which it can use to request subresources.
  // This ensures that they can be served out of the AppCache.
  if (cache && cache->is_complete())
    MaybePassSubresourceFactory();

  frontend_->OnCacheSelected(host_id_, info);
}

}  // namespace content
