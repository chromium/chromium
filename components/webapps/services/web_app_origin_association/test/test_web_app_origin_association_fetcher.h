// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_TEST_TEST_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_TEST_TEST_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_

#include <map>

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace webapps {

class TestWebAppOriginAssociationFetcher
    : public WebAppOriginAssociationFetcher {
 public:
  TestWebAppOriginAssociationFetcher();
  TestWebAppOriginAssociationFetcher(
      const TestWebAppOriginAssociationFetcher&) = delete;
  TestWebAppOriginAssociationFetcher& operator=(
      const TestWebAppOriginAssociationFetcher&) = delete;
  ~TestWebAppOriginAssociationFetcher() override;

  // WebAppOriginAssociationFetcher:
  void FetchWebAppOriginAssociationFile(
      const url::Origin& origin,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      FetchFileCallback callback) override;

  void SetData(std::map<url::Origin, std::string> data);

 private:
  std::map<url::Origin, std::string> data_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_TEST_TEST_WEB_APP_ORIGIN_ASSOCIATION_FETCHER_H_