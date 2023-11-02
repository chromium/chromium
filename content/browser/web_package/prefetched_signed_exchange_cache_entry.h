// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ENTRY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ENTRY_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace net {
struct SHA256HashValue;
}  // namespace net

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

// The entry struct of PrefetchedSignedExchangeCache.
class CONTENT_EXPORT PrefetchedSignedExchangeCacheEntry {
 public:
  PrefetchedSignedExchangeCacheEntry();
  ~PrefetchedSignedExchangeCacheEntry();

  PrefetchedSignedExchangeCacheEntry(
      const PrefetchedSignedExchangeCacheEntry&) = delete;
  PrefetchedSignedExchangeCacheEntry& operator=(
      const PrefetchedSignedExchangeCacheEntry&) = delete;

  const GURL& outer_url() const { return outer_url_; }
  const network::mojom::URLResponseHeadPtr& outer_response() const {
    return outer_response_;
  }
  const std::unique_ptr<const net::SHA256HashValue>& header_integrity() const {
    return header_integrity_;
  }
  const GURL& inner_url() const { return inner_url_; }
  const network::mojom::URLResponseHeadPtr& inner_response() const {
    return inner_response_;
  }
  const std::unique_ptr<const network::URLLoaderCompletionStatus>&
  completion_status() const {
    return completion_status_;
  }
  const std::unique_ptr<const storage::BlobDataHandle>& blob_data_handle()
      const {
    return blob_data_handle_;
  }
  base::Time signature_expire_time() const { return signature_expire_time_; }
  const GURL& cert_url() const { return cert_url_; }
  const net::IPAddress& cert_server_ip_address() const {
    return cert_server_ip_address_;
  }

  void SetOuterUrl(const GURL& outer_url);
  void SetOuterResponse(network::mojom::URLResponseHeadPtr outer_response);
  void SetHeaderIntegrity(
      std::unique_ptr<const net::SHA256HashValue> header_integrity);
  void SetInnerUrl(const GURL& inner_url);
  void SetInnerResponse(network::mojom::URLResponseHeadPtr inner_response);
  void SetCompletionStatus(
      std::unique_ptr<const network::URLLoaderCompletionStatus>
          completion_status);
  void SetBlobDataHandle(
      std::unique_ptr<const storage::BlobDataHandle> blob_data_handle);
  void SetSignatureExpireTime(const base::Time& signature_expire_time);
  void SetCertUrl(const GURL& cert_url);
  void SetCertServerIPAddress(const net::IPAddress& cert_server_ip_address);

  std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> Clone() const;

 private:
  GURL outer_url_;
  network::mojom::URLResponseHeadPtr outer_response_;
  std::unique_ptr<const net::SHA256HashValue> header_integrity_;
  GURL inner_url_;
  network::mojom::URLResponseHeadPtr inner_response_;
  std::unique_ptr<const network::URLLoaderCompletionStatus> completion_status_;
  std::unique_ptr<const storage::BlobDataHandle> blob_data_handle_;
  base::Time signature_expire_time_;
  GURL cert_url_;
  net::IPAddress cert_server_ip_address_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ENTRY_H_
