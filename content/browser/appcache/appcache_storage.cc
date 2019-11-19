// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_storage.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

// static
const int64_t AppCacheStorage::kUnitializedId = -1;

AppCacheStorage::AppCacheStorage(AppCacheServiceImpl* service)
    : last_cache_id_(kUnitializedId),
      last_group_id_(kUnitializedId),
      last_response_id_(kUnitializedId),
      service_(service) {}

AppCacheStorage::~AppCacheStorage() {
  DCHECK(delegate_references_.empty());
}

AppCacheStorage::DelegateReference::DelegateReference(
    Delegate* delegate, AppCacheStorage* storage)
    : delegate(delegate), storage(storage) {
  storage->delegate_references_.insert(
      std::map<Delegate*, DelegateReference*>::value_type(delegate, this));
}

AppCacheStorage::DelegateReference::~DelegateReference() {
  if (delegate)
    storage->delegate_references_.erase(delegate);
}

AppCacheStorage::ResponseInfoLoadTask::ResponseInfoLoadTask(
    const GURL& manifest_url,
    int64_t response_id,
    AppCacheStorage* storage)
    : storage_(storage),
      manifest_url_(manifest_url),
      response_id_(response_id),
      info_buffer_(base::MakeRefCounted<HttpResponseInfoIOBuffer>()) {
  storage_->pending_info_loads_[response_id] = base::WrapUnique(this);
}

AppCacheStorage::ResponseInfoLoadTask::~ResponseInfoLoadTask() = default;

void AppCacheStorage::ResponseInfoLoadTask::StartIfNeeded() {
  if (reader_)
    return;
  reader_ = storage_->CreateResponseReader(manifest_url_, response_id_);
  reader_->ReadInfo(info_buffer_.get(),
                    base::BindOnce(&ResponseInfoLoadTask::OnReadComplete,
                                   base::Unretained(this)));
}

void AppCacheStorage::ResponseInfoLoadTask::OnReadComplete(int result) {
  std::unique_ptr<ResponseInfoLoadTask> this_wrapper(
      std::move(storage_->pending_info_loads_[response_id_]));
  storage_->pending_info_loads_.erase(response_id_);

  scoped_refptr<AppCacheResponseInfo> info;
  if (result >= 0) {
    info = base::MakeRefCounted<AppCacheResponseInfo>(
        storage_->GetWeakPtr(), manifest_url_, response_id_,
        std::move(info_buffer_->http_info), info_buffer_->response_data_size);
  }
  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnResponseInfoLoaded(info.get(), response_id_);
      });

  // returning deletes this
}

void AppCacheStorage::LoadResponseInfo(const GURL& manifest_url,
                                       int64_t id,
                                       Delegate* delegate) {
  AppCacheResponseInfo* info = working_set_.GetResponseInfo(id);
  if (info) {
    delegate->OnResponseInfoLoaded(info, id);
    return;
  }
  ResponseInfoLoadTask* info_load =
      GetOrCreateResponseInfoLoadTask(manifest_url, id);
  DCHECK(manifest_url == info_load->manifest_url());
  DCHECK(id == info_load->response_id());
  info_load->AddDelegate(GetOrCreateDelegateReference(delegate));
  info_load->StartIfNeeded();
}

base::WeakPtr<AppCacheStorage> AppCacheStorage::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheStorage::UpdateUsageMapAndNotify(const url::Origin& origin,
                                              int64_t new_usage) {
  DCHECK_GE(new_usage, 0);
  int64_t old_usage = usage_map_[origin];
  if (new_usage > 0)
    usage_map_[origin] = new_usage;
  else
    usage_map_.erase(origin);
  if (new_usage != old_usage && service()->quota_manager_proxy()) {
    service()->quota_manager_proxy()->NotifyStorageModified(
        storage::QuotaClient::kAppcache, origin,
        blink::mojom::StorageType::kTemporary, new_usage - old_usage);
  }
}

void AppCacheStorage::ClearUsageMapAndNotify() {
  if (service()->quota_manager_proxy()) {
    for (const auto& pair : usage_map_) {
      service()->quota_manager_proxy()->NotifyStorageModified(
          storage::QuotaClient::kAppcache, pair.first,
          blink::mojom::StorageType::kTemporary, -(pair.second));
    }
  }
  usage_map_.clear();
}

void AppCacheStorage::NotifyStorageAccessed(const url::Origin& origin) {
  if (service()->quota_manager_proxy() &&
      usage_map_.find(origin) != usage_map_.end())
    service()->quota_manager_proxy()->NotifyStorageAccessed(
        storage::QuotaClient::kAppcache, origin,
        blink::mojom::StorageType::kTemporary);
}

}  // namespace content
