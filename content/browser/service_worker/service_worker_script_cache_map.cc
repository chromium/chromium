// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : owner_(owner), context_(context) {}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64_t ServiceWorkerScriptCacheMap::LookupResourceId(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return blink::mojom::kInvalidServiceWorkerResourceId;
  return found->second->resource_id;
}

std::optional<std::string> ServiceWorkerScriptCacheMap::LookupSha256Checksum(
    const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end()) {
    return std::nullopt;
  }
  return found->second->sha256_checksum;
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(const GURL& url,
                                                       int64_t resource_id) {
  DCHECK_EQ(blink::mojom::kInvalidServiceWorkerResourceId,
            LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING)
      << owner_->status();
  if (!context_) {
    return;  // Our storage has been wiped via DeleteAndStartOver.
  }
  resource_map_[url] = storage::mojom::ServiceWorkerResourceRecord::New(
      resource_id, url, -1, /*sha256_checksum=*/"");
  context_->registry()->StoreUncommittedResourceId(resource_id, owner_->key());
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url,
    int64_t size_bytes,
    const std::string& sha256_checksum,
    net::Error net_error,
    const std::string& status_message) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId,
            LookupResourceId(url));
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING ||
         owner_->status() == ServiceWorkerVersion::REDUNDANT);
  if (!context_)
    return;  // Our storage has been wiped via DeleteAndStartOver.

  if (net_error != net::OK) {
    context_->registry()->DoomUncommittedResource(LookupResourceId(url));
    resource_map_.erase(url);
    if (owner_->script_url() == url) {
      main_script_net_error_ = net_error;
      main_script_status_message_ = status_message;
    }
  } else {
    // |size_bytes| should not be negative when caching finished successfully.
    CHECK_GE(size_bytes, 0);
    resource_map_[url]->size_bytes = size_bytes;
    resource_map_[url]->sha256_checksum = sha256_checksum;
  }
}

std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
ServiceWorkerScriptCacheMap::GetResources() const {
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  for (const auto& it : resource_map_) {
    resources.push_back(it.second->Clone());
  }

  return resources;
}

void ServiceWorkerScriptCacheMap::SetResources(
    const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
        resources) {
  DCHECK(resource_map_.empty());
  for (auto it = resources.begin(); it != resources.end(); ++it) {
    resource_map_[(*it)->url] = (*it)->Clone();
  }
}

void ServiceWorkerScriptCacheMap::UpdateSha256Checksum(
    const GURL& url,
    const std::string& sha256_checksum) {
  DCHECK(base::Contains(resource_map_, url));
  resource_map_[url]->sha256_checksum = sha256_checksum;
}

void ServiceWorkerScriptCacheMap::WriteMetadata(
    const GURL& url,
    base::span<const uint8_t> data,
    net::CompletionOnceCallback callback) {
  if (!context_) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  auto found = resource_map_.find(url);
  if (found == resource_map_.end() ||
      found->second->resource_id ==
          blink::mojom::kInvalidServiceWorkerResourceId) {
    std::move(callback).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }

  CHECK_LT(next_callback_id_, std::numeric_limits<uint64_t>::max());
  uint64_t callback_id = next_callback_id_++;
  mojo_base::BigBuffer buffer(base::as_bytes(data));

  DCHECK(!base::Contains(callbacks_, callback_id));
  callbacks_[callback_id] = std::move(callback);

  mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter> writer;
  context_->GetStorageControl()->CreateResourceMetadataWriter(
      found->second->resource_id, writer.BindNewPipeAndPassReceiver());
  writer.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerScriptCacheMap::OnWriterDisconnected,
                     weak_factory_.GetWeakPtr(), callback_id));

  auto* raw_writer = writer.get();
  raw_writer->WriteMetadata(
      std::move(buffer),
      base::BindOnce(&ServiceWorkerScriptCacheMap::OnMetadataWritten,
                     weak_factory_.GetWeakPtr(), std::move(writer),
                     callback_id));
}

void ServiceWorkerScriptCacheMap::ClearMetadata(
    const GURL& url,
    net::CompletionOnceCallback callback) {
  WriteMetadata(url, std::vector<uint8_t>(), std::move(callback));
}

void ServiceWorkerScriptCacheMap::OnWriterDisconnected(uint64_t callback_id) {
  RunCallback(callback_id, net::ERR_FAILED);
}

void ServiceWorkerScriptCacheMap::OnMetadataWritten(
    mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter> writer,
    uint64_t callback_id,
    int result) {
  RunCallback(callback_id, result);
}

void ServiceWorkerScriptCacheMap::RunCallback(uint64_t callback_id,
                                              int result) {
  auto it = callbacks_.find(callback_id);
  CHECK(it != callbacks_.end(), base::NotFatalUntil::M130);
  std::move(it->second).Run(result);
  callbacks_.erase(it);
}

}  // namespace content
