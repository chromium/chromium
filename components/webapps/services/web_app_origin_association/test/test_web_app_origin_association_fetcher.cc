// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"

#include <memory>
#include <utility>

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace webapps {

TestWebAppOriginAssociationFetcher::TestWebAppOriginAssociationFetcher() =
    default;

TestWebAppOriginAssociationFetcher::~TestWebAppOriginAssociationFetcher() =
    default;

void TestWebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
    const url::Origin& origin,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    FetchFileCallback callback) {
  auto search = data_.find(origin);
  std::string file_content;
  if (search != data_.end())
    file_content = search->second;

  std::move(callback).Run(file_content.empty()
                              ? nullptr
                              : std::make_unique<std::string>(file_content));
}

void TestWebAppOriginAssociationFetcher::SetData(
    std::map<url::Origin, std::string> data) {
  data_ = std::move(data);
}

}  // namespace webapps
