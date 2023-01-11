// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_provision_fetcher.h"

#include "base/functional/bind.h"

namespace media {

MojoProvisionFetcher::MojoProvisionFetcher(
    mojo::PendingRemote<mojom::ProvisionFetcher> provision_fetcher)
    : provision_fetcher_(std::move(provision_fetcher)) {
  DVLOG(1) << __func__;
}

MojoProvisionFetcher::~MojoProvisionFetcher() = default;

// ProvisionFetcher implementation:
void MojoProvisionFetcher::Retrieve(const GURL& default_url,
                                    const std::string& request_data,
                                    ResponseCB response_cb) {
  DVLOG(1) << __func__;
  provision_fetcher_->Retrieve(
      default_url, request_data,
      base::BindOnce(&MojoProvisionFetcher::OnResponse,
                     weak_factory_.GetWeakPtr(), std::move(response_cb)));
}

void MojoProvisionFetcher::OnResponse(ResponseCB response_cb,
                                      bool success,
                                      const std::string& response) {
  std::move(response_cb).Run(success, response);
}

}  // namespace media
