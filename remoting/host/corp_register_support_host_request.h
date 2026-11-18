// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/host/register_support_host_request_base.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

// Corp implementation of RegisterSupportHostRequest that registers a remote
// support host by calling the corp internal CreateRemoteSupportHost API.
class CorpRegisterSupportHostRequest final
    : public RegisterSupportHostRequestBase {
 public:
  CorpRegisterSupportHostRequest(
      std::unique_ptr<OAuthTokenGetter> token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CorpRegisterSupportHostRequest(const CorpRegisterSupportHostRequest&) =
      delete;
  CorpRegisterSupportHostRequest& operator=(
      const CorpRegisterSupportHostRequest&) = delete;

  ~CorpRegisterSupportHostRequest() override;

 private:
  void RegisterHost(
      const internal::RemoteSupportHostStruct& host,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      RegisterHostCallback callback) override;
  void CancelPendingRequests() override;

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_
