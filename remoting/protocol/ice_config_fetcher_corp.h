// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_CORP_H_
#define REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_CORP_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/corp_service_client.h"
#include "remoting/protocol/ice_config_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting::protocol {

// IceConfigRequest that fetches IceConfig from the Corp API.
class IceConfigFetcherCorp final : public protocol::IceConfigFetcher {
 public:
  IceConfigFetcherCorp(
      const std::string& refresh_token,
      const std::string& service_account_email,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  IceConfigFetcherCorp(const IceConfigFetcherCorp&) = delete;
  IceConfigFetcherCorp& operator=(const IceConfigFetcherCorp&) = delete;

  ~IceConfigFetcherCorp() override;

  // IceConfigFetcher implementation.
  void GetIceConfig(OnIceConfigCallback callback) override;

 private:
  CorpServiceClient service_client_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_CORP_H_
