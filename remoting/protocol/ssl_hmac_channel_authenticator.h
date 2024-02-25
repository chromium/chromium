// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/channel_authenticator.h"

namespace net {
class CertVerifier;
class DrainableIOBuffer;
class GrowableIOBuffer;
class SSLClientContext;
class SSLServerContext;
class SSLSocket;
class TransportSecurityState;
}  // namespace net

namespace remoting {

class RsaKeyPair;

namespace protocol {

// SslHmacChannelAuthenticator implements ChannelAuthenticator that
// secures channels using SSL and authenticates them with a shared
// secret HMAC.
// Please update network traffic annotation in the .cc file if this class is
// used for any new purposes.
class SslHmacChannelAuthenticator : public ChannelAuthenticator {
 public:
  enum LegacyMode {
    NONE,
    SEND_ONLY,
    RECEIVE_ONLY,
  };

  // CreateForClient() and CreateForHost() create an authenticator
  // instances for client and host. |auth_key| specifies shared key
  // known by both host and client. In case of V1Authenticator the
  // |auth_key| is set to access code. For EKE-based authentication
  // |auth_key| is the key established using EKE over the signaling
  // channel.
  static std::unique_ptr<SslHmacChannelAuthenticator> CreateForClient(
      const std::string& remote_cert,
      const std::string& auth_key);

  static std::unique_ptr<SslHmacChannelAuthenticator> CreateForHost(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& auth_key);

  SslHmacChannelAuthenticator(const SslHmacChannelAuthenticator&) = delete;
  SslHmacChannelAuthenticator& operator=(const SslHmacChannelAuthenticator&) =
      delete;

  ~SslHmacChannelAuthenticator() override;

  // ChannelAuthenticator interface.
  void SecureAndAuthenticate(std::unique_ptr<P2PStreamSocket> socket,
                             DoneCallback done_callback) override;

 private:
  class P2PStreamSocketAdapter;

  // P2PStreamSocketAdater outlives the SslHmacChannelAuthenticator, but SSL
  // sockets must not outlive their context structures. SslSocketContext bundles
  // them together for convenience.
  struct SslSocketContext {
    SslSocketContext();
    SslSocketContext(SslSocketContext&&);
    ~SslSocketContext();
    SslSocketContext& operator=(SslSocketContext&&);

    // Used in the SERVER mode only.
    std::unique_ptr<net::SSLServerContext> server_context;

    // Used in the CLIENT mode only.
    std::unique_ptr<net::TransportSecurityState> transport_security_state;
    std::unique_ptr<net::CertVerifier> cert_verifier;
    std::unique_ptr<net::SSLClientContext> client_context;
  };

  SslHmacChannelAuthenticator(const std::string& auth_key);

  bool is_ssl_server();

  void OnConnected(int result);

  void WriteAuthenticationBytes(bool* callback_called);
  void OnAuthBytesWritten(int result);
  bool HandleAuthBytesWritten(int result, bool* callback_called);

  void ReadAuthenticationBytes();
  void OnAuthBytesRead(int result);
  bool HandleAuthBytesRead(int result);
  bool VerifyAuthBytes(const std::string& received_auth_bytes);

  void CheckDone(bool* callback_called);
  void NotifyError(int error);

  // The mutual secret used for authentication.
  std::string auth_key_;

  // Used in the SERVER mode only.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;

  // Used in the CLIENT mode only.
  std::string remote_cert_;

  SslSocketContext socket_context_;
  std::unique_ptr<net::SSLSocket> socket_;
  DoneCallback done_callback_;

  scoped_refptr<net::DrainableIOBuffer> auth_write_buf_;
  scoped_refptr<net::GrowableIOBuffer> auth_read_buf_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SSL_HMAC_CHANNEL_AUTHENTICATOR_H_
