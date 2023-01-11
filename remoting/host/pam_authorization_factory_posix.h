// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAM_AUTHORIZATION_FACTORY_POSIX_H_
#define REMOTING_HOST_PAM_AUTHORIZATION_FACTORY_POSIX_H_

#include <memory>

#include "remoting/protocol/authenticator.h"

// PamAuthorizationFactory abuses the AuthenticatorFactory interface to apply
// PAM-based authorization on top of some underlying authentication scheme.

namespace remoting {

class PamAuthorizationFactory : public protocol::AuthenticatorFactory {
 public:
  PamAuthorizationFactory(
      std::unique_ptr<protocol::AuthenticatorFactory> underlying);
  ~PamAuthorizationFactory() override;

  std::unique_ptr<protocol::Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) override;

 private:
  std::unique_ptr<protocol::AuthenticatorFactory> underlying_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PAM_AUTHORIZATION_FACTORY_POSIX_H_
