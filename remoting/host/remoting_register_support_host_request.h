// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/signaling/signal_strategy.h"

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

class OAuthTokenGetter;
class ProtobufHttpStatus;

// A RegisterSupportHostRequest implementation that uses Remoting API to
// register the host.
class RemotingRegisterSupportHostRequest final
    : public RegisterSupportHostRequest,
      public SignalStrategy::Listener {
 public:
  RemotingRegisterSupportHostRequest(
      std::unique_ptr<OAuthTokenGetter> token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RemotingRegisterSupportHostRequest(
      const RemotingRegisterSupportHostRequest&) = delete;
  RemotingRegisterSupportHostRequest& operator=(
      const RemotingRegisterSupportHostRequest&) = delete;

  ~RemotingRegisterSupportHostRequest() override;

  // RegisterSupportHostRequest implementation.
  void StartRequest(SignalStrategy* signal_strategy,
                    scoped_refptr<RsaKeyPair> key_pair,
                    const std::string& authorized_helper,
                    std::optional<ChromeOsEnterpriseParams> params,
                    RegisterCallback callback) override;

 private:
  using RegisterSupportHostResponseCallback = base::OnceCallback<void(
      const ProtobufHttpStatus&,
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

  enum class State {
    NOT_STARTED,
    REGISTERING,
    REGISTERED,
  };

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void RegisterHost();
  void OnRegisterHostResult(
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::RegisterSupportHostResponse> response);

  void RunCallback(const std::string& support_id,
                   base::TimeDelta lifetime,
                   protocol::ErrorCode error_code);

  raw_ptr<SignalStrategy> signal_strategy_ = nullptr;
  scoped_refptr<RsaKeyPair> key_pair_;
  RegisterCallback callback_;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<RegisterSupportHostClient> register_host_client_;
  std::optional<ChromeOsEnterpriseParams> enterprise_params_;
  std::string authorized_helper_;

  State state_ = State::NOT_STARTED;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
