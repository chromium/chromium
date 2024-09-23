// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/me2me_host_authenticator_factory.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "remoting/protocol/rejecting_authenticator.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

namespace {
using Authenticator::RejectionReason::INVALID_ACCOUNT_ID;
using Authenticator::RejectionReason::INVALID_CREDENTIALS;
}  // namespace

Me2MeHostAuthenticatorFactory::Me2MeHostAuthenticatorFactory(
    CheckAccessPermissionCallback check_access_permission_callback,
    std::unique_ptr<HostAuthenticationConfig> config)
    : check_access_permission_callback_(check_access_permission_callback),
      config_(std::move(config)) {}

Me2MeHostAuthenticatorFactory::~Me2MeHostAuthenticatorFactory() = default;

std::unique_ptr<Authenticator>
Me2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& original_local_jid,
    const std::string& original_remote_jid) {
  std::string local_jid = NormalizeSignalingId(original_local_jid);
  std::string remote_jid = NormalizeSignalingId(original_remote_jid);

  // Verify that the client's jid is an ASCII string.
  auto parts = base::SplitStringOnce(remote_jid, '/');
  if (!base::IsStringASCII(remote_jid) || !parts) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
               << ": Invalid signaling id.";
    return std::make_unique<RejectingAuthenticator>(INVALID_CREDENTIALS);
  }

  auto [email_address, _] = *parts;
  if (!check_access_permission_callback_.Run(email_address)) {
    return std::make_unique<RejectingAuthenticator>(INVALID_CREDENTIALS);
  }

  if (!config_->local_cert.empty() && config_->key_pair.get()) {
    std::string normalized_local_jid = NormalizeSignalingId(local_jid);
    std::string normalized_remote_jid = NormalizeSignalingId(remote_jid);

    return std::make_unique<NegotiatingHostAuthenticator>(
        normalized_local_jid, normalized_remote_jid,
        std::make_unique<HostAuthenticationConfig>(*config_));
  }

  return std::make_unique<RejectingAuthenticator>(INVALID_CREDENTIALS);
}

}  // namespace remoting::protocol
