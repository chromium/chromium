// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request_base.h"

#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "remoting/base/http_status.h"
#include "remoting/host/host_details.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

protocol::ErrorCode MapError(HttpStatus::Code status_code) {
  switch (status_code) {
    case HttpStatus::Code::OK:
      return protocol::ErrorCode::OK;
    case HttpStatus::Code::DEADLINE_EXCEEDED:
      return protocol::ErrorCode::SIGNALING_TIMEOUT;
    case HttpStatus::Code::PERMISSION_DENIED:
    case HttpStatus::Code::UNAUTHENTICATED:
      return protocol::ErrorCode::AUTHENTICATION_FAILED;
    default:
      return protocol::ErrorCode::SIGNALING_ERROR;
  }
}

}  // namespace

RegisterSupportHostRequestBase::RegisterSupportHostRequestBase() = default;

RegisterSupportHostRequestBase::~RegisterSupportHostRequestBase() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void RegisterSupportHostRequestBase::StartRequest(
    SignalStrategy* signal_strategy,
    std::unique_ptr<net::ClientCertStore> client_cert_store,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& authorized_helper,
    std::optional<ChromeOsEnterpriseParams> params,
    RegisterCallback callback) {
  DCHECK_EQ(State::NOT_STARTED, state_);
  DCHECK(signal_strategy);
  DCHECK(key_pair);
  DCHECK(callback);
  signal_strategy_ = signal_strategy;
  key_pair_ = key_pair;
  callback_ = std::move(callback);
  enterprise_params_ = std::move(params);
  authorized_helper_ = authorized_helper;

  signal_strategy_->AddListener(this);
  Initialize(std::move(client_cert_store));
}

void RegisterSupportHostRequestBase::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      RegisterHostInternal();
      break;
    case SignalStrategy::State::DISCONNECTED:
      RunCallback({}, {}, protocol::ErrorCode::SIGNALING_ERROR);
      break;
    default:
      // Do nothing.
      break;
  }
}

bool RegisterSupportHostRequestBase::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void RegisterSupportHostRequestBase::RegisterHostInternal() {
  DCHECK_EQ(SignalStrategy::CONNECTED, signal_strategy_->GetState());
  if (state_ != State::NOT_STARTED) {
    return;
  }
  state_ = State::REGISTERING;
  internal::RemoteSupportHostStruct host;
  host.public_key = key_pair_->GetPublicKey();
  host.version = STRINGIZE(VERSION);
  host.authorized_helper_email = authorized_helper_;
  host.operating_system_info.name = GetHostOperatingSystemName();
  host.operating_system_info.version = GetHostOperatingSystemVersion();
  const SignalingAddress& local_address = signal_strategy_->GetLocalAddress();
  if (!local_address.GetFtlInfo(&host.tachyon_account_info.account_id,
                                &host.tachyon_account_info.registration_id)) {
    LOG(ERROR) << "Local signaling ID is not a valid Tachyon ID: "
               << local_address.id();
  }

  RegisterHost(
      host, enterprise_params_,
      base::BindOnce(&RegisterSupportHostRequestBase::OnRegisterHostResult,
                     base::Unretained(this)));
}

void RegisterSupportHostRequestBase::OnRegisterHostResult(
    const HttpStatus& status,
    std::string_view support_id,
    base::TimeDelta support_id_lifetime) {
  if (!status.ok()) {
    state_ = State::NOT_STARTED;
    LOG(ERROR) << "Failed to register support host: " << status.error_message()
               << " (" << static_cast<int>(status.error_code()) << ")";
    RunCallback({}, {}, MapError(status.error_code()));
    return;
  }
  state_ = State::REGISTERED;
  RunCallback(support_id, support_id_lifetime, protocol::ErrorCode::OK);
}

void RegisterSupportHostRequestBase::RunCallback(
    std::string_view support_id,
    base::TimeDelta lifetime,
    protocol::ErrorCode error_code) {
  if (!callback_) {
    // Callback has already been run, so just return.
    return;
  }

  // Cleanup state before calling the callback.
  CancelPendingRequests();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  // TODO: yuweih - make callback take std::string_view.
  std::move(callback_).Run(std::string(support_id), lifetime, error_code);
}

}  // namespace remoting
