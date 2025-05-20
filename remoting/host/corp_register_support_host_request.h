// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_

#include "remoting/host/register_support_host_request_base.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;
class ProtobufHttpClient;

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
  void Initialize(
      std::unique_ptr<net::ClientCertStore> client_cert_store) override;
  void RegisterHost(
      const internal::RemoteSupportHostStruct& host,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      RegisterHostCallback callback) override;
  void CancelPendingRequests() override;

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // This is constructed when Initialize() is called.
  std::unique_ptr<ProtobufHttpClient> http_client_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_REGISTER_SUPPORT_HOST_REQUEST_H_
