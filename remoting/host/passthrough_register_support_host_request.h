// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PASSTHROUGH_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_PASSTHROUGH_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

struct ChromeOsEnterpriseParams;
class RsaKeyPair;

// A RegisterSupportHostRequest implementation that skips the server-side
// registration call and returns a specific value. This class is used for cases
// where a host instance has already been registered and we want to use the same
// support id for a new instance.
class PassthroughRegisterSupportHostRequest final
    : public RegisterSupportHostRequest,
      public SignalStrategy::Listener {
 public:
  explicit PassthroughRegisterSupportHostRequest(const std::string& support_id);

  PassthroughRegisterSupportHostRequest(
      const PassthroughRegisterSupportHostRequest&) = delete;
  PassthroughRegisterSupportHostRequest& operator=(
      const PassthroughRegisterSupportHostRequest&) = delete;

  ~PassthroughRegisterSupportHostRequest() override;

  // RegisterSupportHostRequest implementation.
  void StartRequest(SignalStrategy* signal_strategy,
                    scoped_refptr<RsaKeyPair> key_pair,
                    const std::string& authorized_helper,
                    std::optional<ChromeOsEnterpriseParams> params,
                    RegisterCallback callback) override;

 private:
  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void RunCallback(const std::string& support_id,
                   base::TimeDelta lifetime,
                   protocol::ErrorCode error_code);

  std::string support_id_;
  RegisterCallback callback_;
  raw_ptr<SignalStrategy> signal_strategy_ = nullptr;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PASSTHROUGH_REGISTER_SUPPORT_HOST_REQUEST_H_
