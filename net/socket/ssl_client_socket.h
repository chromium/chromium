// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_verifier.h"
#include "net/socket/ssl_socket.h"
#include "net/ssl/ssl_client_auth_cache.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class HostPortPair;
class SCTAuditingDelegate;
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
  // Records some histograms based on the result of the SSL handshake.
  static void RecordSSLConnectResult(
      SSLClientSocket& ssl_socket,
      int result,
      bool is_ech_capable,
      bool ech_enabled,
      const std::optional<std::vector<uint8_t>>& ech_retry_configs,
      const LoadTimingInfo::ConnectTiming& connect_timing);

  SSLClientSocket();

  // Called in response to |ERR_ECH_NOT_NEGOTIATED| in Connect(), to determine
  // how to retry the connection, up to some limit. If this method returns a
  // non-empty string, it is the serialized updated ECHConfigList provided by
  // the server. The connection can be retried with the new value. If it returns
  // an empty string, the server has indicated ECH has been disabled. The
  // connection can be retried with ECH disabled.
  virtual std::vector<uint8_t> GetECHRetryConfigs() = 0;

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

  // Serialize |next_protos| in the wire format for ALPN: protocols are listed
  // in order, each prefixed by a one-byte length.
  static std::vector<uint8_t> SerializeNextProtos(
      const NextProtoVector& next_protos);

 private:
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocket, SerializeNextProtos);
  // For signed_cert_timestamps_received_ and stapled_ocsp_response_received_.
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketVersionTest,
                           ConnectSignedCertTimestampsTLSExtension);
  FRIEND_TEST_ALL_PREFIXES(SSLClientSocketVersionTest,
                           ConnectSignedCertTimestampsEnablesOCSP);

  // True if SCTs were received via a TLS extension.
  bool signed_cert_timestamps_received_ = false;
  // True if a stapled OCSP response was received.
  bool stapled_ocsp_response_received_ = false;
};

// Shared state and configuration across multiple SSLClientSockets.
class NET_EXPORT SSLClientContext : public SSLConfigService::Observer,
                                    public CertVerifier::Observer,
                                    public CertDatabase::Observer {
 public:
  enum class SSLConfigChangeType {
    kSSLConfigChanged,
    kCertDatabaseChanged,
    kCertVerifierChanged,
  };

  class NET_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when SSL configuration for all hosts changed. Newly-created
    // SSLClientSockets will pick up the new configuration. Note that changes
    // which only apply to one server will result in a call to
    // OnSSLConfigForServersChanged() instead.
    virtual void OnSSLConfigChanged(SSLConfigChangeType change_type) = 0;
    // Called when SSL configuration for |servers| changed. Newly-created
    // SSLClientSockets to any server in |servers| will pick up the new
    // configuration.
    virtual void OnSSLConfigForServersChanged(
        const base::flat_set<HostPortPair>& servers) = 0;
  };

  // Creates a new SSLClientContext with the specified parameters. The
  // SSLClientContext may not outlive the input parameters.
  //
  // |ssl_config_service| may be null to always use the default
  // SSLContextConfig. |ssl_client_session_cache| may be null to disable session
  // caching. |sct_auditing_delegate| may be null to disable SCT auditing.
  SSLClientContext(SSLConfigService* ssl_config_service,
                   CertVerifier* cert_verifier,
                   TransportSecurityState* transport_security_state,
                   SSLClientSessionCache* ssl_client_session_cache,
                   SCTAuditingDelegate* sct_auditing_delegate);

  SSLClientContext(const SSLClientContext&) = delete;
  SSLClientContext& operator=(const SSLClientContext&) = delete;

  ~SSLClientContext() override;

  const SSLContextConfig& config() { return config_; }

  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  CertVerifier* cert_verifier() { return cert_verifier_; }
  TransportSecurityState* transport_security_state() {
    return transport_security_state_;
  }
  SSLClientSessionCache* ssl_client_session_cache() {
    return ssl_client_session_cache_;
  }
  SCTAuditingDelegate* sct_auditing_delegate() {
    return sct_auditing_delegate_;
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
  // Note this method will synchronously call OnSSLConfigForServersChanged() on
  // observers.
  void SetClientCertificate(const HostPortPair& server,
                            scoped_refptr<X509Certificate> client_cert,
                            scoped_refptr<SSLPrivateKey> private_key);

  // Clears a client certificate preference for |server| set by
  // SetClientCertificate(). Returns true if one was removed and false
  // otherwise.
  //
  // Note this method will synchronously call OnSSLConfigForServersChanged() on
  // observers.
  bool ClearClientCertificate(const HostPortPair& server);

  // Clears a client certificate preference for |host| set by
  // SetClientCertificate() if |certificate| doesn't match the cached
  // certificate.
  //
  // Note this method will synchronously call OnSSLConfigForServersChanged() on
  // observers.
  void ClearClientCertificateIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate);

  // Clears a client certificate preference, set by SetClientCertificate(),
  // for all hosts whose cached certificate matches |certificate|.
  //
  // Note this method will synchronously call OnSSLConfigForServersChanged() on
  // observers.
  void ClearMatchingClientCertificate(
      const scoped_refptr<net::X509Certificate>& certificate);

  base::flat_set<HostPortPair> GetClientCertificateCachedServersForTesting()
      const {
    return ssl_client_auth_cache_.GetCachedServers();
  }

  // Add an observer to be notified when configuration has changed.
  // RemoveObserver() must be called before |observer| is destroyed.
  void AddObserver(Observer* observer);

  // Remove an observer added with AddObserver().
  void RemoveObserver(Observer* observer);

  // SSLConfigService::Observer:
  void OnSSLContextConfigChanged() override;

  // CertVerifier::Observer:
  void OnCertVerifierChanged() override;

  // CertDatabase::Observer:
  void OnTrustStoreChanged() override;
  void OnClientCertStoreChanged() override;

 private:
  void NotifySSLConfigChanged(SSLConfigChangeType change_type);
  void NotifySSLConfigForServersChanged(
      const base::flat_set<HostPortPair>& servers);

  SSLContextConfig config_;

  raw_ptr<SSLConfigService> ssl_config_service_;
  raw_ptr<CertVerifier> cert_verifier_;
  raw_ptr<TransportSecurityState> transport_security_state_;
  raw_ptr<SSLClientSessionCache> ssl_client_session_cache_;
  raw_ptr<SCTAuditingDelegate> sct_auditing_delegate_;

  SSLClientAuthCache ssl_client_auth_cache_;

  base::ObserverList<Observer, true /* check_empty */> observers_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_H_
