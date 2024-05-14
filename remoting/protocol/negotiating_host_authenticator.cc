// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_host_authenticator.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/pairing_host_authenticator.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session_authz_authenticator.h"
#include "remoting/protocol/spake2_authenticator.h"
#include "remoting/protocol/third_party_host_authenticator.h"
#include "remoting/protocol/token_validator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

NegotiatingHostAuthenticator::NegotiatingHostAuthenticator(
    std::string_view local_id,
    std::string_view remote_id,
    std::unique_ptr<HostAuthenticationConfig> config)
    : NegotiatingAuthenticatorBase(WAITING_MESSAGE),
      local_id_(local_id),
      remote_id_(remote_id),
      config_(std::move(config)) {
  methods_ = config_->GetSupportedMethods();
  DCHECK(!methods_.empty());
}

NegotiatingHostAuthenticator::~NegotiatingHostAuthenticator() = default;

void NegotiatingHostAuthenticator::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);
  state_ = PROCESSING_MESSAGE;

  const jingle_xmpp::XmlElement* pairing_tag =
      message->FirstNamed(kPairingInfoTag);
  if (pairing_tag) {
    client_id_ = pairing_tag->Attr(kClientIdAttribute);
  }

  std::string method_attr = message->Attr(kMethodAttributeQName);
  Method method = HostAuthenticationConfig::ParseMethodString(method_attr);

  // If the host has already chosen a method, it can't be changed by the client.
  if (current_method_ != Method::INVALID && method != current_method_) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    std::move(resume_callback).Run();
    return;
  }

  // If the client did not specify a preferred auth method, or specified an
  // unknown or unsupported method, then select the first known method from
  // the supported-methods attribute.
  if (method == Method::INVALID || !base::Contains(methods_, method)) {
    method = Method::INVALID;

    std::string supported_methods_attr =
        message->Attr(kSupportedMethodsAttributeQName);
    if (supported_methods_attr.empty()) {
      // Message contains neither method nor supported-methods attributes.
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
      std::move(resume_callback).Run();
      return;
    }

    // Find the first mutually-supported method in the client's list of
    // supported-methods.
    for (const std::string& method_str : base::SplitString(
             supported_methods_attr, std::string(1, kSupportedMethodsSeparator),
             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      Method list_value =
          HostAuthenticationConfig::ParseMethodString(method_str);
      if (list_value != Method::INVALID &&
          base::Contains(methods_, list_value)) {
        // Found common method.
        method = list_value;
        break;
      }
    }

    if (method == Method::INVALID) {
      // Failed to find a common auth method.
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
      std::move(resume_callback).Run();
      return;
    }

    // Drop the current message because we've chosen a different method.
    current_method_ = method;
    CreateAuthenticator(
        MESSAGE_READY,
        base::BindOnce(&NegotiatingHostAuthenticator::UpdateState,
                       base::Unretained(this), std::move(resume_callback)));
    return;
  }

  // If the client specified a supported method, and the host hasn't chosen a
  // method yet, use the client's preferred method and process the message.
  if (current_method_ == Method::INVALID) {
    current_method_ = method;
    // Copy the message since the authenticator may process it asynchronously.
    CreateAuthenticator(
        WAITING_MESSAGE,
        base::BindOnce(&NegotiatingAuthenticatorBase::ProcessMessageInternal,
                       base::Unretained(this),
                       base::Owned(new jingle_xmpp::XmlElement(*message)),
                       std::move(resume_callback)));
    return;
  }

  // If the client is using the host's current method, just process the message.
  ProcessMessageInternal(message, std::move(resume_callback));
}

std::unique_ptr<jingle_xmpp::XmlElement>
NegotiatingHostAuthenticator::GetNextMessage() {
  return GetNextMessageInternal();
}

void NegotiatingHostAuthenticator::CreateAuthenticator(
    Authenticator::State preferred_initial_state,
    base::OnceClosure resume_callback) {
  DCHECK(current_method_ != Method::INVALID);

  switch (current_method_) {
    case Method::INVALID:
      NOTREACHED_IN_MIGRATION();
      break;

    case Method::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519: {
      DCHECK(config_->session_authz_client_factory);
      auto authenticator = std::make_unique<SessionAuthzAuthenticator>(
          CredentialsType::CORP_SESSION_AUTHZ,
          config_->session_authz_client_factory->Create(),
          base::BindRepeating(&Spake2Authenticator::CreateForHost, local_id_,
                              remote_id_, config_->local_cert,
                              config_->key_pair));
      authenticator->Start(std::move(resume_callback));
      current_authenticator_ = std::move(authenticator);
      break;
    }

    case Method::THIRD_PARTY_SPAKE2_CURVE25519:
      current_authenticator_ = std::make_unique<ThirdPartyHostAuthenticator>(
          base::BindRepeating(&Spake2Authenticator::CreateForHost, local_id_,
                              remote_id_, config_->local_cert,
                              config_->key_pair),
          config_->token_validator_factory->CreateTokenValidator(local_id_,
                                                                 remote_id_));
      std::move(resume_callback).Run();
      break;

    case Method::PAIRED_SPAKE2_CURVE25519: {
      PairingHostAuthenticator* pairing_authenticator =
          new PairingHostAuthenticator(
              config_->pairing_registry,
              base::BindRepeating(&Spake2Authenticator::CreateForHost,
                                  local_id_, remote_id_, config_->local_cert,
                                  config_->key_pair),
              config_->shared_secret_hash);
      current_authenticator_.reset(pairing_authenticator);
      pairing_authenticator->Initialize(client_id_, preferred_initial_state,
                                        std::move(resume_callback));
      break;
    }

    case Method::SHARED_SECRET_SPAKE2_CURVE25519:
      current_authenticator_ = Spake2Authenticator::CreateForHost(
          local_id_, remote_id_, config_->local_cert, config_->key_pair,
          config_->shared_secret_hash, preferred_initial_state);
      std::move(resume_callback).Run();
      break;
  }

  ChainStateChangeAfterAcceptedWithUnderlying(*current_authenticator_);
}

}  // namespace remoting::protocol
