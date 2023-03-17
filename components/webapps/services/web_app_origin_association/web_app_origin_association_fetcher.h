// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace webapps {

using FetchFileCallback =
    base::OnceCallback<void(std::unique_ptr<std::string> file_content)>;

// Makes network requests to fetch web app origin association files.
class WebAppOriginAssociationFetcher {
 public:
  WebAppOriginAssociationFetcher();
  virtual ~WebAppOriginAssociationFetcher();
  WebAppOriginAssociationFetcher(const WebAppOriginAssociationFetcher&) =
      delete;
  WebAppOriginAssociationFetcher& operator=(
      const WebAppOriginAssociationFetcher&) = delete;

  virtual void FetchWebAppOriginAssociationFile(
      const url::Origin& origin,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      FetchFileCallback callback);

  void SetRetryOptionsForTest(int max_retry,
                              network::SimpleURLLoader::RetryMode retry_mode);

 private:
  void SendRequest(
      const GURL& url,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      FetchFileCallback callback);
  void OnResponse(FetchFileCallback callback,
                  std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<WebAppOriginAssociationFetcher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_
