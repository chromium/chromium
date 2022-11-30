// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"

#include "net/base/hash_value.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

PrefetchedSignedExchangeCacheEntry::PrefetchedSignedExchangeCacheEntry() =
    default;
PrefetchedSignedExchangeCacheEntry::~PrefetchedSignedExchangeCacheEntry() =
    default;

void PrefetchedSignedExchangeCacheEntry::SetOuterUrl(const GURL& outer_url) {
  outer_url_ = outer_url;
}
void PrefetchedSignedExchangeCacheEntry::SetOuterResponse(
    network::mojom::URLResponseHeadPtr outer_response) {
  outer_response_ = std::move(outer_response);
}
void PrefetchedSignedExchangeCacheEntry::SetHeaderIntegrity(
    std::unique_ptr<const net::SHA256HashValue> header_integrity) {
  header_integrity_ = std::move(header_integrity);
}
void PrefetchedSignedExchangeCacheEntry::SetInnerUrl(const GURL& inner_url) {
  inner_url_ = inner_url;
}
void PrefetchedSignedExchangeCacheEntry::SetInnerResponse(
    network::mojom::URLResponseHeadPtr inner_response) {
  inner_response_ = std::move(inner_response);
}
void PrefetchedSignedExchangeCacheEntry::SetCompletionStatus(
    std::unique_ptr<const network::URLLoaderCompletionStatus>
        completion_status) {
  completion_status_ = std::move(completion_status);
}
void PrefetchedSignedExchangeCacheEntry::SetBlobDataHandle(
    std::unique_ptr<const storage::BlobDataHandle> blob_data_handle) {
  blob_data_handle_ = std::move(blob_data_handle);
}
void PrefetchedSignedExchangeCacheEntry::SetSignatureExpireTime(
    const base::Time& signature_expire_time) {
  signature_expire_time_ = signature_expire_time;
}
void PrefetchedSignedExchangeCacheEntry::SetCertUrl(const GURL& cert_url) {
  cert_url_ = cert_url;
}
void PrefetchedSignedExchangeCacheEntry::SetCertServerIPAddress(
    const net::IPAddress& cert_server_ip_address) {
  cert_server_ip_address_ = cert_server_ip_address;
}

std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>
PrefetchedSignedExchangeCacheEntry::Clone() const {
  DCHECK(outer_url().is_valid());
  DCHECK(outer_response());
  DCHECK(header_integrity());
  DCHECK(inner_url().is_valid());
  DCHECK(inner_response());
  DCHECK(completion_status());
  DCHECK(blob_data_handle());
  DCHECK(!signature_expire_time().is_null());

  std::unique_ptr<PrefetchedSignedExchangeCacheEntry> clone =
      std::make_unique<PrefetchedSignedExchangeCacheEntry>();
  clone->SetOuterUrl(outer_url_);
  clone->SetOuterResponse(outer_response_.Clone());
  clone->SetHeaderIntegrity(
      std::make_unique<const net::SHA256HashValue>(*header_integrity_));
  clone->SetInnerUrl(inner_url_);
  clone->SetInnerResponse(inner_response_.Clone());
  clone->SetCompletionStatus(
      std::make_unique<const network::URLLoaderCompletionStatus>(
          *completion_status_));
  clone->SetBlobDataHandle(
      std::make_unique<const storage::BlobDataHandle>(*blob_data_handle_));
  clone->SetSignatureExpireTime(signature_expire_time_);

  clone->SetCertUrl(cert_url_);
  clone->SetCertServerIPAddress(cert_server_ip_address_);

  return clone;
}

}  // namespace content