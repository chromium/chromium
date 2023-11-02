// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_
#define CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_

#include "media/base/provision_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace content {

// The ProvisionFetcher that retrieves the data by HTTP POST request.

class URLProvisionFetcher : public media::ProvisionFetcher {
 public:
  explicit URLProvisionFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  URLProvisionFetcher(const URLProvisionFetcher&) = delete;
  URLProvisionFetcher& operator=(const URLProvisionFetcher&) = delete;

  ~URLProvisionFetcher() override;

  // media::ProvisionFetcher implementation.
  void Retrieve(const GURL& default_url,
                const std::string& request_data,
                ProvisionFetcher::ResponseCB response_cb) override;

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  media::ProvisionFetcher::ResponseCB response_cb_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_
