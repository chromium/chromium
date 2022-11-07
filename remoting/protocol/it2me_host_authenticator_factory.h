// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_
#define REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/validating_authenticator.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

// It2MeHostAuthenticatorFactory implements AuthenticatorFactory and
// understands both the V2 and legacy V1 authentication mechanisms.
class It2MeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  It2MeHostAuthenticatorFactory(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& access_code,
      const ValidatingAuthenticator::ValidationCallback& callback);

  It2MeHostAuthenticatorFactory(const It2MeHostAuthenticatorFactory&) = delete;
  It2MeHostAuthenticatorFactory& operator=(
      const It2MeHostAuthenticatorFactory&) = delete;

  ~It2MeHostAuthenticatorFactory() override;

  // AuthenticatorFactory interface.
  std::unique_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) override;

 private:
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string access_code_hash_;
  ValidatingAuthenticator::ValidationCallback validation_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_
