// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_host.h"

#include <utility>
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
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

namespace {

blink::mojom::AppCacheInfoPtr CreateCacheInfo(
    const AppCache* cache,
    const GURL& manifest_url,
    blink::mojom::AppCacheStatus status) {
  auto info = blink::mojom::AppCacheInfo::New();
  info->manifest_url = manifest_url;
  info->status = status;

  if (!cache)
    return info;

  info->cache_id = cache->cache_id();

  if (!cache->is_complete())
    return info;

  DCHECK(cache->owning_group());
  info->is_complete = true;
  info->group_id = cache->owning_group()->group_id();
  info->last_update_time = cache->update_time();
  info->creation_time = cache->owning_group()->creation_time();
  info->response_sizes = cache->cache_size();
  info->padding_sizes = cache->padding_size();
  return info;
}

bool CanAccessDocumentURL(int process_id, const GURL& document_url) {
  DCHECK_NE(process_id, ChildProcessHost::kInvalidUniqueID);
  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  return document_url.is_empty() ||       // window.open("javascript:''") case.
         document_url.IsAboutSrcdoc() ||  // <iframe srcdoc= ...> case.
         document_url.IsAboutBlank() ||   // <iframe src="javascript:''"> case.
         document_url == GURL("data:,") ||  // CSP blocked_urls.
         (document_url.SchemeIsBlob() &&    // <iframe src="blob:null/xx"> case.
          url::Origin::Create(document_url).opaque()) ||
         security_policy->CanAccessDataForOrigin(process_id,
                                                 document_url) ||
         !security_policy->HasSecurityState(process_id);  // process shutdown.
}

}  // namespace

AppCacheHost::AppCacheHost(
    const base::UnguessableToken& host_id,
    int process_id,
    int render_frame_id,
    mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend_remote,
    AppCacheServiceImpl* service)
    : host_id_(host_id),
      process_id_(process_id),
      pending_main_resource_cache_id_(blink::mojom::kAppCacheNoCacheId),
      pending_selected_cache_id_(blink::mojom::kAppCacheNoCacheId),
      was_select_cache_called_(false),
      is_cache_selection_enabled_(true),
      frontend_remote_(std::move(frontend_remote)),
      frontend_(frontend_remote_.is_bound() ? frontend_remote_.get() : nullptr),
      render_frame_id_(render_frame_id),
      service_(service),
      storage_(service->storage()),
      main_resource_was_namespace_entry_(false),
      main_resource_blocked_(false),
      associated_cache_info_pending_(false) {
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

  // Run pending callbacks in case we get destroyed with pending callbacks while
  // the mojo connection is still open.
  if (pending_get_status_callback_) {
    std::move(pending_get_status_callback_)
        .Run(blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
  }
  if (pending_swap_cache_callback_)
    std::move(pending_swap_cache_callback_).Run(false);
  if (pending_start_update_callback_)
    std::move(pending_start_update_callback_).Run(false);
}

void AppCacheHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::AppCacheHost> receiver) {
  receiver_.Bind(std::move(receiver));
  // Unretained is safe because |this| will outlive |receiver_|.
  receiver_.set_disconnect_handler(
      base::BindOnce(&AppCacheHost::Unregister, base::Unretained(this)));
}

void AppCacheHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppCacheHost::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppCacheHost::Unregister() {
  service_->EraseHost(host_id_);
}

void AppCacheHost::SelectCache(const GURL& document_url,
                               const int64_t cache_document_was_loaded_from,
                               const GURL& manifest_url) {
  if (was_select_cache_called_) {
    mojo::ReportBadMessage("ACH_SELECT_CACHE");
    return;
  }

  DCHECK_NE(process_id_, ChildProcessHost::kInvalidUniqueID);
  if (!CanAccessDocumentURL(process_id_, document_url)) {
    mojo::ReportBadMessage("ACH_SELECT_CACHE_DOCUMENT_URL_ACCESS_NOT_ALLOWED");
    return;
  }

  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!manifest_url.is_empty() &&
      !security_policy->CanAccessDataForOrigin(process_id_, manifest_url) &&
      security_policy->HasSecurityState(process_id_)) {
    mojo::ReportBadMessage("ACH_SELECT_CACHE_MANIFEST_URL_ACCESS_NOT_ALLOWED");
    return;
  }

  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null() && !is_selection_pending());

  was_select_cache_called_ = true;
  if (!is_cache_selection_enabled_) {
    FinishCacheSelection(nullptr, nullptr, mojo::ReportBadMessageCallback());
    return;
  }

  origin_in_use_ = url::Origin::Create(document_url);
  if (service()->quota_manager_proxy() && !origin_in_use_.opaque())
    service()->quota_manager_proxy()->NotifyOriginInUse(origin_in_use_);

  if (main_resource_blocked_)
    OnContentBlocked(blocked_manifest_url_);

  // 7.9.5 The application cache selection algorithm.
  // The algorithm is started here and continues in FinishCacheSelection,
  // after cache or group loading is complete.
  // Note: Foreign entries are detected on the client side and
  // MarkAsForeignEntry is called in that case, so that detection
  // step is skipped here.

  if (cache_document_was_loaded_from != blink::mojom::kAppCacheNoCacheId) {
    LoadSelectedCache(cache_document_was_loaded_from);
    return;
  }

  if (!manifest_url.is_empty() &&
      (manifest_url.GetOrigin() == document_url.GetOrigin())) {
    // TODO(mek): Technically we should be checking to make sure
    // first_party_url_ was initialized, however in practice it appears often
    // this isn't the case (even though that means the renderer is trying to
    // select an AppCache for a document that wasn't fetched through this host
    // in the first place, which shouldn't happen). Since the worst that can
    // happen if it isn't is that AppCache isn't used when third party cookie
    // blocking is enabled, we want to get rid of AppCache anyway, and this has
    // been the behavior for a long time anyway, don't bother checking and just
    // continue whether it was set or not.

    AppCachePolicy* policy = service()->appcache_policy();
    if (policy && !policy->CanCreateAppCache(manifest_url, first_party_url_)) {
      FinishCacheSelection(nullptr, nullptr, mojo::ReportBadMessageCallback());
      frontend()->EventRaised(
          blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
      frontend()->ErrorEventRaised(blink::mojom::AppCacheErrorDetails::New(
          "Cache creation was blocked by the content policy",
          blink::mojom::AppCacheErrorReason::APPCACHE_POLICY_ERROR, GURL(), 0,
          false /*is_cross_origin*/));
      OnContentBlocked(manifest_url);
      return;
    }
    // Note: The client detects if the document was not loaded using HTTP GET
    // and invokes SelectCache without a manifest url, so that detection step
    // is also skipped here.
    set_preferred_manifest_url(manifest_url);
    new_master_entry_url_ = document_url;
    LoadOrCreateGroup(manifest_url);
    return;
  }
  // TODO(michaeln): If there was a manifest URL, the user agent may report
  // to the user that it was ignored, to aid in application development.
  FinishCacheSelection(nullptr, nullptr, mojo::ReportBadMessageCallback());
}

void AppCacheHost::SelectCacheForWorker(int64_t appcache_id) {
  if (was_select_cache_called_) {
    mojo::ReportBadMessage("ACH_SELECT_CACHE_FOR_WORKER");
    return;
  }

  DCHECK(pending_start_update_callback_.is_null() &&
         pending_swap_cache_callback_.is_null() &&
         pending_get_status_callback_.is_null() && !is_selection_pending());

  was_select_cache_called_ = true;
  if (appcache_id != blink::mojom::kAppCacheNoCacheId) {
    LoadSelectedCache(appcache_id);
    return;
  }
  FinishCacheSelection(nullptr, nullptr, mojo::ReportBadMessageCallback());
}

// TODO(michaeln): change method name to MarkEntryAsForeign for consistency
void AppCacheHost::MarkAsForeignEntry(const GURL& document_url,
                                      int64_t cache_document_was_loaded_from) {
  if (was_select_cache_called_) {
    mojo::ReportBadMessage("ACH_MARK_AS_FOREIGN_ENTRY");
    return;
  }

  if (!CanAccessDocumentURL(process_id_, document_url)) {
    mojo::ReportBadMessage(
        "ACH_MARK_AS_FOREIGN_ENTRY_DOCUMENT_URL_ACCESS_NOT_ALLOWED");
    return;
  }

  // The document url is not the resource url in the fallback case.
  storage()->MarkEntryAsForeign(
      main_resource_was_namespace_entry_ ? namespace_entry_url_ : document_url,
      cache_document_was_loaded_from);
  SelectCache(document_url, blink::mojom::kAppCacheNoCacheId, GURL());
}

void AppCacheHost::GetStatus(GetStatusCallback callback) {
  if (!pending_start_update_callback_.is_null() ||
      !pending_swap_cache_callback_.is_null() ||
      !pending_get_status_callback_.is_null()) {
    mojo::ReportBadMessage("ACH_GET_STATUS");
    std::move(callback).Run(
        blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
    return;
  }

  pending_get_status_callback_ = std::move(callback);
  if (is_selection_pending())
    return;

  DoPendingGetStatus();
}

void AppCacheHost::DoPendingGetStatus() {
  DCHECK_EQ(false, pending_get_status_callback_.is_null());

  std::move(pending_get_status_callback_).Run(GetStatusSync());
}

void AppCacheHost::StartUpdate(StartUpdateCallback callback) {
  if (!pending_start_update_callback_.is_null() ||
      !pending_swap_cache_callback_.is_null() ||
      !pending_get_status_callback_.is_null()) {
    mojo::ReportBadMessage("ACH_START_UPDATE");
    std::move(callback).Run(false);
    return;
  }

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

void AppCacheHost::SwapCache(SwapCacheCallback callback) {
  if (!pending_start_update_callback_.is_null() ||
      !pending_swap_cache_callback_.is_null() ||
      !pending_get_status_callback_.is_null()) {
    mojo::ReportBadMessage("ACH_SWAP_CACHE");
    std::move(callback).Run(false);
    return;
  }

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
    const base::UnguessableToken& spawning_host_id) {
  spawning_host_id_ = spawning_host_id;
}

const AppCacheHost* AppCacheHost::GetSpawningHost() const {
  return service_->GetHost(spawning_host_id_);
}

void AppCacheHost::GetResourceList(GetResourceListCallback callback) {
  std::vector<blink::mojom::AppCacheResourceInfo> params;
  std::vector<blink::mojom::AppCacheResourceInfoPtr> out;

  GetResourceListSync(&params);

  // Box up params for output.
  out.reserve(params.size());
  for (auto& p : params) {
    out.emplace_back(base::in_place, std::move(p));
  }
  std::move(callback).Run(std::move(out));
}

std::unique_ptr<AppCacheRequestHandler> AppCacheHost::CreateRequestHandler(
    std::unique_ptr<AppCacheRequest> request,
    ResourceType resource_type,
    bool should_reset_appcache) {
  if (AppCacheRequestHandler::IsMainResourceType(resource_type)) {
    // Store the first party origin so that it can be used later in SelectCache
    // for checking whether the creation of the appcache is allowed.
    first_party_url_ = request->GetSiteForCookies();
    first_party_url_initialized_ = true;
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

void AppCacheHost::GetResourceListSync(
    std::vector<blink::mojom::AppCacheResourceInfo>* resource_infos) {
  if (associated_cache_.get() && associated_cache_->is_complete())
    associated_cache_->ToResourceInfoVector(resource_infos);
}

blink::mojom::AppCacheStatus AppCacheHost::GetStatusSync() {
  // 6.9.8 Application cache API
  AppCache* cache = associated_cache();
  if (!cache)
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;

  // A cache without an owning group represents the cache being constructed
  // during the application cache update process.
  if (!cache->owning_group())
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;

  if (cache->owning_group()->is_obsolete())
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_OBSOLETE;
  if (cache->owning_group()->update_status() == AppCacheGroup::CHECKING)
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  if (cache->owning_group()->update_status() == AppCacheGroup::DOWNLOADING)
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
  if (swappable_cache_.get())
    return blink::mojom::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY;
  return blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE;
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
  FinishCacheSelection(nullptr, group, mojo::ReportBadMessageCallback());
}

void AppCacheHost::LoadSelectedCache(int64_t cache_id) {
  DCHECK(cache_id != blink::mojom::kAppCacheNoCacheId);
  pending_selected_cache_id_ = cache_id;
  pending_selected_cache_bad_message_callback_ = mojo::GetBadMessageCallback();
  storage()->LoadCache(cache_id, this);
}

void AppCacheHost::OnCacheLoaded(AppCache* cache, int64_t cache_id) {
  if (cache_id == pending_main_resource_cache_id_) {
    pending_main_resource_cache_id_ = blink::mojom::kAppCacheNoCacheId;
    main_resource_cache_ = cache;
  } else if (cache_id == pending_selected_cache_id_) {
    pending_selected_cache_id_ = blink::mojom::kAppCacheNoCacheId;
    FinishCacheSelection(
        cache, nullptr,
        std::move(pending_selected_cache_bad_message_callback_));
  }
}

void AppCacheHost::FinishCacheSelection(
    AppCache* cache,
    AppCacheGroup* group,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK(!associated_cache());

  // 7.9.5 The application cache selection algorithm
  if (cache) {
    // If document was loaded from an application cache, Associate document
    // with the application cache from which it was loaded. Invoke the
    // application cache update process for that cache and with the browsing
    // context being navigated.
    DCHECK(new_master_entry_url_.is_empty());
    DCHECK(bad_message_callback);
    if (!cache->owning_group()) {
      std::move(bad_message_callback).Run("ACH_SELECT_CACHE_ID_NOT_OWNED");
      return;
    }
    if (cache->owning_group()->manifest_url() != preferred_manifest_url_) {
      std::move(bad_message_callback).Run("ACH_SELECT_CACHE_BAD_MANIFEST_URL");
      return;
    }
    AppCacheGroup* owning_group = cache->owning_group();
    const char* kFormatString =
        "Document was loaded from Application Cache with manifest %s";
    frontend()->LogMessage(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(kFormatString,
                           owning_group->manifest_url().spec().c_str()));
    AssociateCompleteCache(cache);
    if (!owning_group->is_obsolete() && !owning_group->is_being_deleted()) {
      owning_group->StartUpdateWithHost(this);
      ObserveGroupBeingUpdated(owning_group);
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
    const char* kFormatString =
        group->HasCache()
            ? "Adding master entry to Application Cache with manifest %s"
            : "Creating Application Cache with manifest %s";
    frontend()->LogMessage(
        blink::mojom::ConsoleMessageLevel::kInfo,
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
    blink::mojom::AppCacheInfoPtr info = CreateCacheInfo(
        associated_cache_.get(), preferred_manifest_url_, GetStatusSync());
    associated_cache_info_pending_ = false;
    // In the network service world, we need to pass the URLLoaderFactory
    // instance to the renderer which it can use to request subresources.
    // This ensures that they can be served out of the AppCache.
    MaybePassSubresourceFactory();
    OnAppCacheAccessed(info->manifest_url, false);
    frontend()->CacheSelected(std::move(info));
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
  DCHECK(cache_id != blink::mojom::kAppCacheNoCacheId);
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

void AppCacheHost::SetProcessId(int process_id) {
  DCHECK_EQ(process_id_, ChildProcessHost::kInvalidUniqueID);
  DCHECK_NE(process_id, ChildProcessHost::kInvalidUniqueID);
  process_id_ = process_id;
}

base::WeakPtr<AppCacheHost> AppCacheHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheHost::MaybePassSubresourceFactory() {
  // We already have a valid factory. This happens when the document was loaded
  // from the AppCache during navigation.
  if (subresource_url_factory_.get())
    return;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  AppCacheSubresourceURLFactory::CreateURLLoaderFactory(GetWeakPtr(),
                                                        &factory_remote);

  // We may not have bound |factory_remote| if the storage partition has shut
  // down.
  if (factory_remote)
    frontend()->SetSubresourceFactory(std::move(factory_remote));
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

  blink::mojom::AppCacheInfoPtr info =
      CreateCacheInfo(cache, manifest_url, GetStatusSync());
  // In the network service world, we need to pass the URLLoaderFactory
  // instance to the renderer which it can use to request subresources.
  // This ensures that they can be served out of the AppCache.
  if (cache && cache->is_complete())
    MaybePassSubresourceFactory();

  OnAppCacheAccessed(info->manifest_url, false);
  frontend()->CacheSelected(std::move(info));
}

void AppCacheHost::OnContentBlocked(const GURL& manifest_url) {
  OnAppCacheAccessed(manifest_url, /*blocked=*/true);
}

void AppCacheHost::OnAppCacheAccessed(const GURL& manifest_url, bool blocked) {
  if (!blocked && manifest_url.is_empty())
    return;

  // Unit tests might not have a UI thread, if that's the case just don't bother
  // informing WebContents about this access.
  if (render_frame_id_ != MSG_ROUTING_NONE &&
      BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](int process_id, int render_frame_id, const GURL& manifest_url,
               bool blocked) {
              WebContents* web_contents =
                  WebContentsImpl::FromRenderFrameHostID(process_id,
                                                         render_frame_id);
              if (web_contents) {
                static_cast<WebContentsImpl*>(web_contents)
                    ->OnAppCacheAccessed(manifest_url, blocked);
              }
            },
            process_id_, render_frame_id_, manifest_url, blocked));
  }
}

}  // namespace content
