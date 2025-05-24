// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CREDENTIALS_TYPE_H_
#define REMOTING_PROTOCOL_CREDENTIALS_TYPE_H_

namespace remoting::protocol {

// The type of the credentials used for authentication.
enum class CredentialsType {
  // The credentials type is unknown. This is usually reported by a wrapper
  // (e.g. negotiating) authenticator when the underlying authenticator is not
  // determined yet.
  UNKNOWN,

  // Key exchange based on a shared secret, e.g. a PIN or an IT2ME access code.
  SHARED_SECRET,

  // Key exchange based on a shared pairing secret.
  PAIRED,

  // Authentication using the third-party authentication server, which generates
  // a shared secret for key exchange.
  THIRD_PARTY,

  // Authentication using the Cloud SessionAuthz service, which generates a
  // shared secret for key exchange.
  CLOUD_SESSION_AUTHZ,

  // Authentication using the corp-internal SessionAuthz service, which
  // generates a shared secret for key exchange.
  CORP_SESSION_AUTHZ,
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CREDENTIALS_TYPE_H_
