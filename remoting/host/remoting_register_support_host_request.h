// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/register_support_host_request_base.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace apis {
namespace v1 {

class RegisterSupportHostRequest;
class RegisterSupportHostResponse;

}  // namespace v1
}  // namespace apis

class HttpStatus;
class OAuthTokenGetter;

// A RegisterSupportHostRequest implementation that uses Remoting API to
// register the host.
class RemotingRegisterSupportHostRequest final
    : public RegisterSupportHostRequestBase {
 public:
  RemotingRegisterSupportHostRequest(
      std::unique_ptr<OAuthTokenGetter> token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RemotingRegisterSupportHostRequest(
      const RemotingRegisterSupportHostRequest&) = delete;
  RemotingRegisterSupportHostRequest& operator=(
      const RemotingRegisterSupportHostRequest&) = delete;

  ~RemotingRegisterSupportHostRequest() override;

 private:
  using RegisterSupportHostResponseCallback = base::OnceCallback<void(
      const HttpStatus&,
      std::unique_ptr<apis::v1::RegisterSupportHostResponse>)>;

  friend class RemotingRegisterSupportHostTest;

  class RegisterSupportHostClient {
   public:
    virtual ~RegisterSupportHostClient() = default;
    virtual void RegisterSupportHost(
        std::unique_ptr<apis::v1::RegisterSupportHostRequest> request,
        RegisterSupportHostResponseCallback callback) = 0;
    virtual void CancelPendingRequests() = 0;
  };

  class RegisterSupportHostClientImpl;

  void Initialize(
      std::unique_ptr<net::ClientCertStore> client_cert_store) override;
  void RegisterHost(
      const internal::RemoteSupportHostStruct& host,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      RegisterHostCallback callback) override;
  void CancelPendingRequests() override;

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<RegisterSupportHostClient> register_host_client_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
