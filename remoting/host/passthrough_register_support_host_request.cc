// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/passthrough_register_support_host_request.h"

#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"

namespace remoting {

PassthroughRegisterSupportHostRequest::PassthroughRegisterSupportHostRequest(
    const std::string& support_id)
    : support_id_(support_id) {}

PassthroughRegisterSupportHostRequest::
    ~PassthroughRegisterSupportHostRequest() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void PassthroughRegisterSupportHostRequest::StartRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& authorized_helper,
    std::optional<ChromeOsEnterpriseParams> params,
    RegisterCallback callback) {
  signal_strategy_ = signal_strategy;
  callback_ = std::move(callback);

  signal_strategy_->AddListener(this);
}

void PassthroughRegisterSupportHostRequest::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      RunCallback(support_id_, {}, protocol::ErrorCode::OK);
      break;
    case SignalStrategy::State::DISCONNECTED:
      RunCallback({}, {}, protocol::ErrorCode::SIGNALING_ERROR);
      break;
    default:
      // No work is needed until signaling connects or errors out.
      break;
  }
}

bool PassthroughRegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void PassthroughRegisterSupportHostRequest::RunCallback(
    const std::string& support_id,
    base::TimeDelta lifetime,
    protocol::ErrorCode error_code) {
  if (!callback_) {
    // Callback has already been run, so just return.
    return;
  }

  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  std::move(callback_).Run(support_id, lifetime, error_code);
}

}  // namespace remoting
