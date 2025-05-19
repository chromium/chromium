// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_BASE_H_
#define REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_BASE_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/proto/remote_support_service.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

class HttpStatus;

// A RegisterSupportHostRequest abstract class that waits for signal strategy
// connection and calls RegisterSupportHost, which needs to be implemented by
// subclasses.
class RegisterSupportHostRequestBase : public RegisterSupportHostRequest,
                                       public SignalStrategy::Listener {
 public:
  RegisterSupportHostRequestBase();

  RegisterSupportHostRequestBase(const RegisterSupportHostRequestBase&) =
      delete;
  RegisterSupportHostRequestBase& operator=(
      const RegisterSupportHostRequestBase&) = delete;

  ~RegisterSupportHostRequestBase() override;

  // RegisterSupportHostRequest implementation.
  void StartRequest(SignalStrategy* signal_strategy,
                    std::unique_ptr<net::ClientCertStore> client_cert_store,
                    scoped_refptr<RsaKeyPair> key_pair,
                    const std::string& authorized_helper,
                    std::optional<ChromeOsEnterpriseParams> params,
                    RegisterCallback callback) override;

 protected:
  friend class RegisterSupportHostRequestBaseTest;

  // `support_id` may be default-initialized if `status` is not OK.
  using RegisterHostCallback =
      base::OnceCallback<void((const HttpStatus& status,
                               std::string_view support_id,
                               base::TimeDelta support_id_lifetime))>;

  virtual void Initialize(
      std::unique_ptr<net::ClientCertStore> client_cert_store) = 0;
  virtual void RegisterHost(
      const internal::RemoteSupportHostStruct& host,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      RegisterHostCallback callback) = 0;
  virtual void CancelPendingRequests() = 0;

 private:
  enum class State {
    NOT_STARTED,
    REGISTERING,
    REGISTERED,
  };

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void RegisterHostInternal();
  void OnRegisterHostResult(const HttpStatus& status,
                            std::string_view support_id,
                            base::TimeDelta support_id_lifetime);

  void RunCallback(std::string_view support_id,
                   base::TimeDelta lifetime,
                   protocol::ErrorCode error_code);

  raw_ptr<SignalStrategy> signal_strategy_ = nullptr;
  scoped_refptr<RsaKeyPair> key_pair_;
  RegisterCallback callback_;
  std::optional<ChromeOsEnterpriseParams> enterprise_params_;
  std::string authorized_helper_;

  State state_ = State::NOT_STARTED;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_BASE_H_
