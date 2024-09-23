// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
#define REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/host_authentication_config.h"

namespace remoting::protocol {

class Me2MeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  using CheckAccessPermissionCallback =
      base::RepeatingCallback<bool(std::string_view)>;

  Me2MeHostAuthenticatorFactory(
      CheckAccessPermissionCallback check_access_permission_callback,
      std::unique_ptr<HostAuthenticationConfig> config);

  Me2MeHostAuthenticatorFactory(const Me2MeHostAuthenticatorFactory&) = delete;
  Me2MeHostAuthenticatorFactory& operator=(
      const Me2MeHostAuthenticatorFactory&) = delete;

  ~Me2MeHostAuthenticatorFactory() override;

  // AuthenticatorFactory interface.
  std::unique_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) override;

 private:
  // Used for all host authenticators.
  CheckAccessPermissionCallback check_access_permission_callback_;
  std::unique_ptr<HostAuthenticationConfig> config_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
