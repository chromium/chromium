// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/digital_credentials_handler.h"

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/browser/digital_credentials/digital_credential_environment.h"
#include "content/browser/digital_credentials/virtual_wallet.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content::protocol {

DigitalCredentialsHandler::DigitalCredentialsHandler()
    : DevToolsDomainHandler(DigitalCredentials::Metainfo::domainName) {}

DigitalCredentialsHandler::~DigitalCredentialsHandler() = default;

void DigitalCredentialsHandler::SetRenderer(int process_host_id,
                                            RenderFrameHostImpl* frame_host) {
  frame_host_ = frame_host;
}

void DigitalCredentialsHandler::Wire(UberDispatcher* dispatcher) {
  DigitalCredentials::Dispatcher::wire(dispatcher, this);
}

DispatchResponse DigitalCredentialsHandler::SetVirtualWalletBehavior(
    const String& in_action,
    std::optional<String> in_protocol,
    std::unique_ptr<protocol::DictionaryValue> in_response) {
  if (!frame_host_) {
    return DispatchResponse::InvalidParams(
        "Command must be issued on a session attached to a frame");
  }

  if (in_action != DigitalCredentials::VirtualWalletActionEnum::Respond &&
      (in_protocol || in_response)) {
    return DispatchResponse::InvalidParams(
        "Protocol and response are only allowed when action is 'respond'");
  }

  VirtualWallet* wallet =
      DigitalCredentialEnvironment::GetInstance()->GetOrCreateVirtualWallet(
          frame_host_->frame_tree_node());
  CHECK(wallet);

  if (in_action == DigitalCredentials::VirtualWalletActionEnum::Clear) {
    wallet->Clear();
    return DispatchResponse::Success();
  }

  if (in_action == DigitalCredentials::VirtualWalletActionEnum::Respond) {
    if (!in_protocol || !in_response) {
      return DispatchResponse::InvalidParams(
          "Protocol and response required for 'respond' action");
    }

    wallet->set_action(VirtualWallet::Action::kRespond);
    DigitalIdentityProvider::DigitalCredential credential(
        *in_protocol, base::Value(std::move(*in_response)));
    wallet->SetCredential(std::move(credential));
    return DispatchResponse::Success();
  }

  if (in_action == DigitalCredentials::VirtualWalletActionEnum::Decline) {
    wallet->set_action(VirtualWallet::Action::kDecline);
    return DispatchResponse::Success();
  }

  if (in_action == DigitalCredentials::VirtualWalletActionEnum::Wait) {
    wallet->set_action(VirtualWallet::Action::kWait);
    return DispatchResponse::Success();
  }

  return DispatchResponse::InvalidParams("Unknown action");
}

}  // namespace content::protocol
