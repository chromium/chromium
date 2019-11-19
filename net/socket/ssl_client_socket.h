// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "net/base/net_export.h"
#include "net/cert/cert_database.h"
#include "net/socket/ssl_socket.h"
#include "net/ssl/ssl_client_auth_cache.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class CTVerifier;
class HostPortPair;
class SSLClientSessionCache;
struct SSLConfig;
class SSLKeyLogger;
class StreamSocket;
class TransportSecurityState;

// A client socket that uses SSL as the transport layer.
//
// NOTE: The SSL handshake occurs within the Connect method after a TCP
// connection is established.  If a SSL error occurs during the handshake,
// Connect will fail.
//
class NET_EXPORT SSLClientSocket : public SSLSocket {
 public:
  SSLClientSocket();

  // Log SSL key material to |logger|. Must be called before any
  // SSLClientSockets are created.
  //
  // TODO(davidben): Switch this to a parameter on the SSLClientSocketContext
  // once https://crbug.com/458365 is resolved.
  static void SetSSLKeyLogger(std::unique_ptr<SSLKeyLogger> logger);

 protected:
  void set_signed_cert_timestamps_received(
      bool signed_cert_timestamps_received) {
    signed_cert_timestamps_received_ = signed_cert_timestamps_received;
  }

  void set_stapled_ocsp_response_received(bool stapled_ocsp_response_received) {
    stapled_ocsp_response_received_ = stapled_ocsp_response_received;
  }

  // Serialize |next_protos| in the wire format for ALPN and NPN: protocols are
  // listed in order, each prefixed by a one-byte length.
  static std::vector<uint8_t> SerializeNextProtos(
      const NextProtoVector& next_protos);

 private:
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocket, SerializeNextProtos);
  // For signed_cert_timestamps_received_ and stapled_ocsp_response_received_.
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsTLSExtension);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsEnablesOCSP);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           ConnectSignedCertTimestampsDisabled);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketTest,
                           VerifyServerChainProperlyOrdered);

  // True if SCTs were received via a TLS extension.
  bool signed_cert_timestamps_received_;
  // True if a stapled OCSP response was received.
  bool stapled_ocsp_response_received_;
};

// Shared state and configuration across multiple SSLClientSockets.
class NET_EXPORT SSLClientContext : public SSLConfigService::Observer,
                                    public CertDatabase::Observer {
 public:
  class NET_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when SSL configuration for all hosts changed. Newly-created
    // SSLClientSockets will pick up the new configuration. Note that changes
    // which only apply to one server will result in a call to
    // OnSSLConfigForServerChanged() instead.
    virtual void OnSSLConfigChanged(bool is_cert_database_change) = 0;
    // Called when SSL configuration for |server| changed. Newly-created
    // SSLClientSockets to |server| will pick up the new configuration.
    virtual void OnSSLConfigForServerChanged(const HostPortPair& server) = 0;
  };

  // Creates a new SSLClientContext with the specified parameters. The
  // SSLClientContext may not outlive the input parameters.
  //
  // |ssl_config_service| may be null to always use the default
  // SSLContextConfig. |ssl_client_session_cache| may be null to disable session
  // caching.
  SSLClientContext(SSLConfigService* ssl_config_service,
                   CertVerifier* cert_verifier,
                   TransportSecurityState* transport_security_state,
                   CTVerifier* cert_transparency_verifier,
                   CTPolicyEnforcer* ct_policy_enforcer,
                   SSLClientSessionCache* ssl_client_session_cache);
  ~SSLClientContext() override;

  const SSLContextConfig& config() { return config_; }

  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  CertVerifier* cert_verifier() { return cert_verifier_; }
  TransportSecurityState* transport_security_state() {
    return transport_security_state_;
  }
  CTVerifier* cert_transparency_verifier() {
    return cert_transparency_verifier_;
  }
  CTPolicyEnforcer* ct_policy_enforcer() { return ct_policy_enforcer_; }
  SSLClientSessionCache* ssl_client_session_cache() {
    return ssl_client_session_cache_;
  }

  // Creates a new SSLClientSocket which can then be used to establish an SSL
  // connection to |host_and_port| over the already-connected |stream_socket|.
  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config);

  // Looks up the client certificate preference for |server|. If one is found,
  // returns true and sets |client_cert| and |private_key| to the certificate
  // and key. Note these may be null if the preference is to continue with no
  // client certificate. Returns false if no preferences are configured,
  // which means client certificate requests should be reported as
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  bool GetClientCertificate(const HostPortPair& server,
                            scoped_refptr<X509Certificate>* client_cert,
                            scoped_refptr<SSLPrivateKey>* private_key);

  // Configures all subsequent connections to |server| to authenticate with
  // |client_cert| and |private_key| when requested. If there is already a
  // client certificate for |server|, it will be overwritten. |client_cert| and
  // |private_key| may be null to indicate that no client certificate should be
  // sent to |server|.
  //
  // Note this method will synchronously call OnSSLConfigForServerChanged() on
  // observers.
  void SetClientCertificate(const HostPortPair& server,
                            scoped_refptr<X509Certificate> client_cert,
                            scoped_refptr<SSLPrivateKey> private_key);

  // Clears a client certificate preference for |server| set by
  // SetClientCertificate(). Returns true if one was removed and false
  // otherwise.
  //
  // Note this method will synchronously call OnSSLConfigForServerChanged() on
  // observers.
  bool ClearClientCertificate(const HostPortPair& server);

  // Add an observer to be notified when configuration has changed.
  // RemoveObserver() must be called before |observer| is destroyed.
  void AddObserver(Observer* observer);

  // Remove an observer added with AddObserver().
  void RemoveObserver(Observer* observer);

  // SSLConfigService::Observer:
  void OnSSLContextConfigChanged() override;

  // CertDatabase::Observer:
  void OnCertDBChanged() override;

 private:
  void NotifySSLConfigChanged(bool is_cert_database_change);
  void NotifySSLConfigForServerChanged(const HostPortPair& server);

  SSLContextConfig config_;

  SSLConfigService* ssl_config_service_;
  CertVerifier* cert_verifier_;
  TransportSecurityState* transport_security_state_;
  CTVerifier* cert_transparency_verifier_;
  CTPolicyEnforcer* ct_policy_enforcer_;
  SSLClientSessionCache* ssl_client_session_cache_;

  SSLClientAuthCache ssl_client_auth_cache_;

  base::ObserverList<Observer, true /* check_empty */> observers_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientContext);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_H_
