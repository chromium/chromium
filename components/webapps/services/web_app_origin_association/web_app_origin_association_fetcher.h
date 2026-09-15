// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "url/origin.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace webapps {

using FetchFileCallback =
    base::OnceCallback<void(std::optional<std::string> file_content)>;

// Makes network requests to fetch web app origin association files.
// Enforces Local Network Access (LNA) checks based on initiator_address_space,
// preventing public web app installation from fetching origin association files
// from private or loopback networks, and disallows following HTTP redirects.
class WebAppOriginAssociationFetcher {
 public:
  explicit WebAppOriginAssociationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  virtual ~WebAppOriginAssociationFetcher();
  WebAppOriginAssociationFetcher(const WebAppOriginAssociationFetcher&) =
      delete;
  WebAppOriginAssociationFetcher& operator=(
      const WebAppOriginAssociationFetcher&) = delete;

  // Fetches the association file for |origin|, specifying the IP address space
  // of the initiating web app.
  virtual void FetchWebAppOriginAssociationFile(
      const url::Origin& origin,
      network::mojom::IPAddressSpace initiator_address_space,
      FetchFileCallback callback);

  // Overload that determines initiator address space from the origin or
  // defaults to kUnknown.
  virtual void FetchWebAppOriginAssociationFile(const url::Origin& origin,
                                                FetchFileCallback callback);

  void SetRetryOptionsForTest(int max_retry,
                              network::SimpleURLLoader::RetryMode retry_mode);

 private:
  void SendRequest(const GURL& url,
                   network::mojom::IPAddressSpace initiator_address_space,
                   FetchFileCallback callback);
  void OnResponse(FetchFileCallback callback,
                  std::optional<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<WebAppOriginAssociationFetcher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_
