// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TOKEN_VALIDATOR_H_
#define REMOTING_PROTOCOL_TOKEN_VALIDATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "remoting/base/result.h"
#include "remoting/protocol/authenticator.h"
#include "url/gurl.h"

namespace remoting::protocol {

// The |TokenValidator| encapsulates the parameters to be sent to the client
// to obtain a token, and the method to validate that token and obtain the
// shared secret for the connection.
class TokenValidator {
 public:
  using ValidationResult =
      Result<std::string, protocol::Authenticator::RejectionReason>;

  // Callback passed to |ValidateThirdPartyToken|, and called once the host
  // authentication finishes. |validation_result.success()| should be used by
  // the host to create a V2Authenticator. In case of failure, the rejection
  // reason can be found via |validation_result.error()|.
  typedef base::OnceCallback<void(const ValidationResult& validation_result)>
      TokenValidatedCallback;

  virtual ~TokenValidator() = default;

  // Validates |token| with the server and exchanges it for a |shared_secret|.
  // |token_validated_callback| is called when the host authentication ends,
  // in the same thread |ValidateThirdPartyToken| was originally called.
  // The request is canceled if this object is destroyed.
  virtual void ValidateThirdPartyToken(
      const std::string& token,
      TokenValidatedCallback token_validated_callback) = 0;

  // URL sent to the client, to be used by its |TokenFetcher| to get a token.
  virtual const GURL& token_url() const = 0;

  // Space-separated list of connection attributes the host must send to the
  // client, and require the token received in response to match.
  virtual const std::string& token_scope() const = 0;
};

// Factory for |TokenValidator|.
class TokenValidatorFactory
    : public base::RefCountedThreadSafe<TokenValidatorFactory> {
 public:
  // Creates a TokenValidator. |local_jid| and |remote_jid| are used to create
  // a token scope that is restricted to the current connection's JIDs.
  virtual std::unique_ptr<TokenValidator> CreateTokenValidator(
      const std::string& local_jid,
      const std::string& remote_jid) = 0;

 protected:
  friend class base::RefCountedThreadSafe<TokenValidatorFactory>;

  virtual ~TokenValidatorFactory() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_TOKEN_VALIDATOR_H_
