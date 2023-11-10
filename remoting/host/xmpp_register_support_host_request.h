// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_XMPP_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_XMPP_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/signal_strategy.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace base {
class TimeDelta;
}  // namespace base

namespace remoting {

class IqRequest;
class IqSender;

// XmppRegisterSupportHostRequest sends a request to register the host for
// a SupportID, as soon as the associated SignalStrategy becomes
// connected. When a response is received from the bot, it calls the
// callback specified in the Init() method.
class XmppRegisterSupportHostRequest : public RegisterSupportHostRequest,
                                       public SignalStrategy::Listener {
 public:
  // |signal_strategy| and |key_pair| must outlive this
  // object. |callback| is called when registration response is
  // received from the server. Callback is never called if the bot
  // malfunctions and doesn't respond to the request.
  explicit XmppRegisterSupportHostRequest(const std::string& directory_bot_jid);

  XmppRegisterSupportHostRequest(const XmppRegisterSupportHostRequest&) =
      delete;
  XmppRegisterSupportHostRequest& operator=(
      const XmppRegisterSupportHostRequest&) = delete;

  ~XmppRegisterSupportHostRequest() override;

  // RegisterSupportHostRequest implementation.
  void StartRequest(SignalStrategy* signal_strategy,
                    scoped_refptr<RsaKeyPair> key_pair,
                    const std::string& authorized_helper,
                    std::optional<ChromeOsEnterpriseParams> params,
                    RegisterCallback callback) override;

  // HostStatusObserver implementation.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

 private:
  void DoSend();

  std::unique_ptr<jingle_xmpp::XmlElement> CreateRegistrationRequest(
      const std::string& jid);
  std::unique_ptr<jingle_xmpp::XmlElement> CreateSignature(
      const std::string& jid);

  void ProcessResponse(IqRequest* request,
                       const jingle_xmpp::XmlElement* response);
  void ParseResponse(const jingle_xmpp::XmlElement* response,
                     std::string* support_id,
                     base::TimeDelta* lifetime,
                     protocol::ErrorCode* error_code);

  void CallCallback(const std::string& support_id,
                    base::TimeDelta lifetime,
                    protocol::ErrorCode error_code);

  raw_ptr<SignalStrategy> signal_strategy_ = nullptr;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string directory_bot_jid_;
  RegisterCallback callback_;
  std::string authorized_helper_;

  std::unique_ptr<IqSender> iq_sender_;
  std::unique_ptr<IqRequest> request_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_XMPP_REGISTER_SUPPORT_HOST_REQUEST_H_
