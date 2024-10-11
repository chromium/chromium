// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config_fetcher_corp.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting::protocol {

IceConfigFetcherCorp::IceConfigFetcherCorp(
    const std::string& refresh_token,
    const std::string& service_account_email,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : service_client_(refresh_token,
                      service_account_email,
                      url_loader_factory) {}

IceConfigFetcherCorp::~IceConfigFetcherCorp() = default;

void IceConfigFetcherCorp::GetIceConfig(OnIceConfigCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace remoting::protocol
