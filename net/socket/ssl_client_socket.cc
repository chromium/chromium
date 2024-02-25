// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include <string>

#include "base/containers/flat_tree.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/ssl_client_socket_impl.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/ssl/ssl_key_logger.h"

namespace net {

namespace {

// Returns true if |first_cert| and |second_cert| represent the same certificate
// (with the same chain), or if they're both NULL.
bool AreCertificatesEqual(const scoped_refptr<X509Certificate>& first_cert,
                          const scoped_refptr<X509Certificate>& second_cert) {
  return (!first_cert && !second_cert) ||
         (first_cert && second_cert &&
          first_cert->EqualsIncludingChain(second_cert.get()));
}

// Returns a base::Value::Dict value NetLog parameter with the expected format
// for events of type CLEAR_CACHED_CLIENT_CERT.
base::Value::Dict NetLogClearCachedClientCertParams(
    const net::HostPortPair& host,
    const scoped_refptr<net::X509Certificate>& cert,
    bool is_cleared) {
  return base::Value::Dict()
      .Set("host", host.ToString())
      .Set("certificates", cert ? net::NetLogX509CertificateList(cert.get())
                                : base::Value(base::Value::List()))
      .Set("is_cleared", is_cleared);
}

}  // namespace

SSLClientSocket::SSLClientSocket() = default;

// static
void SSLClientSocket::SetSSLKeyLogger(std::unique_ptr<SSLKeyLogger> logger) {
  SSLClientSocketImpl::SetSSLKeyLogger(std::move(logger));
}

// static
std::vector<uint8_t> SSLClientSocket::SerializeNextProtos(
    const NextProtoVector& next_protos) {
  std::vector<uint8_t> wire_protos;
  for (const NextProto next_proto : next_protos) {
    const std::string proto = NextProtoToString(next_proto);
    if (proto.size() > 255) {
      LOG(WARNING) << "Ignoring overlong ALPN protocol: " << proto;
      continue;
    }
    if (proto.size() == 0) {
      LOG(WARNING) << "Ignoring empty ALPN protocol";
      continue;
    }
    wire_protos.push_back(proto.size());
    for (const char ch : proto) {
      wire_protos.push_back(static_cast<uint8_t>(ch));
    }
  }

  return wire_protos;
}

SSLClientContext::SSLClientContext(
    SSLConfigService* ssl_config_service,
    CertVerifier* cert_verifier,
    TransportSecurityState* transport_security_state,
    SSLClientSessionCache* ssl_client_session_cache,
    SCTAuditingDelegate* sct_auditing_delegate)
    : ssl_config_service_(ssl_config_service),
      cert_verifier_(cert_verifier),
      transport_security_state_(transport_security_state),
      ssl_client_session_cache_(ssl_client_session_cache),
      sct_auditing_delegate_(sct_auditing_delegate) {
  CHECK(cert_verifier_);
  CHECK(transport_security_state_);

  if (ssl_config_service_) {
    config_ = ssl_config_service_->GetSSLContextConfig();
    ssl_config_service_->AddObserver(this);
  }
  cert_verifier_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

SSLClientContext::~SSLClientContext() {
  if (ssl_config_service_) {
    ssl_config_service_->RemoveObserver(this);
  }
  cert_verifier_->RemoveObserver(this);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

std::unique_ptr<SSLClientSocket> SSLClientContext::CreateSSLClientSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  return std::make_unique<SSLClientSocketImpl>(this, std::move(stream_socket),
                                               host_and_port, ssl_config);
}

bool SSLClientContext::GetClientCertificate(
    const HostPortPair& server,
    scoped_refptr<X509Certificate>* client_cert,
    scoped_refptr<SSLPrivateKey>* private_key) {
  return ssl_client_auth_cache_.Lookup(server, client_cert, private_key);
}

void SSLClientContext::SetClientCertificate(
    const HostPortPair& server,
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> private_key) {
  ssl_client_auth_cache_.Add(server, std::move(client_cert),
                             std::move(private_key));

  if (ssl_client_session_cache_) {
    // Session resumption bypasses client certificate negotiation, so flush all
    // associated sessions when preferences change.
    ssl_client_session_cache_->FlushForServers({server});
  }
  NotifySSLConfigForServersChanged({server});
}

bool SSLClientContext::ClearClientCertificate(const HostPortPair& server) {
  if (!ssl_client_auth_cache_.Remove(server)) {
    return false;
  }

  if (ssl_client_session_cache_) {
    // Session resumption bypasses client certificate negotiation, so flush all
    // associated sessions when preferences change.
    ssl_client_session_cache_->FlushForServers({server});
  }
  NotifySSLConfigForServersChanged({server});
  return true;
}

void SSLClientContext::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SSLClientContext::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SSLClientContext::OnSSLContextConfigChanged() {
  config_ = ssl_config_service_->GetSSLContextConfig();
  if (ssl_client_session_cache_) {
    ssl_client_session_cache_->Flush();
  }
  NotifySSLConfigChanged(SSLConfigChangeType::kSSLConfigChanged);
}

void SSLClientContext::OnCertVerifierChanged() {
  NotifySSLConfigChanged(SSLConfigChangeType::kCertVerifierChanged);
}

void SSLClientContext::OnTrustStoreChanged() {
  NotifySSLConfigChanged(SSLConfigChangeType::kCertDatabaseChanged);
}

void SSLClientContext::OnClientCertStoreChanged() {
  base::flat_set<HostPortPair> servers =
      ssl_client_auth_cache_.GetCachedServers();
  ssl_client_auth_cache_.Clear();
  if (ssl_client_session_cache_) {
    ssl_client_session_cache_->FlushForServers(servers);
  }
  NotifySSLConfigForServersChanged(servers);
}

void SSLClientContext::ClearClientCertificateIfNeeded(
    const net::HostPortPair& host,
    const scoped_refptr<net::X509Certificate>& certificate) {
  scoped_refptr<X509Certificate> cached_certificate;
  scoped_refptr<SSLPrivateKey> cached_private_key;
  if (!ssl_client_auth_cache_.Lookup(host, &cached_certificate,
                                     &cached_private_key) ||
      AreCertificatesEqual(cached_certificate, certificate)) {
    // No cached client certificate preference for this host.
    net::NetLog::Get()->AddGlobalEntry(
        NetLogEventType::CLEAR_CACHED_CLIENT_CERT, [&]() {
          return NetLogClearCachedClientCertParams(host, certificate,
                                                   /*is_cleared=*/false);
        });
    return;
  }

  net::NetLog::Get()->AddGlobalEntry(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT, [&]() {
        return NetLogClearCachedClientCertParams(host, certificate,
                                                 /*is_cleared=*/true);
      });

  ssl_client_auth_cache_.Remove(host);

  if (ssl_client_session_cache_) {
    ssl_client_session_cache_->FlushForServers({host});
  }

  NotifySSLConfigForServersChanged({host});
}

void SSLClientContext::NotifySSLConfigChanged(SSLConfigChangeType change_type) {
  for (Observer& observer : observers_) {
    observer.OnSSLConfigChanged(change_type);
  }
}

void SSLClientContext::NotifySSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  for (Observer& observer : observers_) {
    observer.OnSSLConfigForServersChanged(servers);
  }
}

}  // namespace net
