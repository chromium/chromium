// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/fake_ssl_client_socket.h"

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/socket/socket_tag.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/test_certificate_data.h"

namespace net {

FakeSSLClientSocket::FakeSSLClientSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port)
    : stream_socket_(std::move(stream_socket)),
      host_and_port_(host_and_port),
      is_connected_(false) {
  CreateFakeCertificate();
}

FakeSSLClientSocket::~FakeSSLClientSocket() = default;

int FakeSSLClientSocket::Read(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  return stream_socket_->Read(buf, buf_len, std::move(callback));
}

int FakeSSLClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return stream_socket_->Write(buf, buf_len, std::move(callback), traffic_annotation);
}

int FakeSSLClientSocket::SetReceiveBufferSize(int32_t size) {
  return stream_socket_->SetReceiveBufferSize(size);
}

int FakeSSLClientSocket::SetSendBufferSize(int32_t size) {
  return stream_socket_->SetSendBufferSize(size);
}

int FakeSSLClientSocket::Connect(CompletionOnceCallback callback) {
  int result = stream_socket_->Connect(std::move(callback));
  if (result == OK) {
    is_connected_ = true;
  }
  return result;
}

void FakeSSLClientSocket::Disconnect() {
  is_connected_ = false;
  stream_socket_->Disconnect();
}

bool FakeSSLClientSocket::IsConnected() const {
  return is_connected_ && stream_socket_->IsConnected();
}

bool FakeSSLClientSocket::IsConnectedAndIdle() const {
  return IsConnected() && stream_socket_->IsConnectedAndIdle();
}

int FakeSSLClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return stream_socket_->GetPeerAddress(address);
}

int FakeSSLClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return stream_socket_->GetLocalAddress(address);
}

const NetLogWithSource& FakeSSLClientSocket::NetLog() const {
  return stream_socket_->NetLog();
}

bool FakeSSLClientSocket::WasEverUsed() const {
  return stream_socket_->WasEverUsed();
}

NextProto FakeSSLClientSocket::GetNegotiatedProtocol() const {
  return NextProto::kProtoHTTP11;  // Default to HTTP/1.1
}

std::optional<std::string_view> FakeSSLClientSocket::GetPeerApplicationSettings() const {
  return std::nullopt;
}

void FakeSSLClientSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  // No client cert requested for .dapp domains
}

int FakeSSLClientSocket::ExportKeyingMaterial(std::string_view label,
                                              std::optional<base::span<const uint8_t>> context,
                                              base::span<uint8_t> out) {
  // Not supported for fake SSL connections
  return ERR_NOT_IMPLEMENTED;
}

std::vector<uint8_t> FakeSSLClientSocket::GetECHRetryConfigs() {
  return std::vector<uint8_t>();
}

std::vector<std::vector<uint8_t>> FakeSSLClientSocket::GetServerTrustAnchorIDsForRetry() {
  return std::vector<std::vector<uint8_t>>();
}

bool FakeSSLClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  *ssl_info = SSLInfo();
  ssl_info->cert = fake_cert_;
  ssl_info->unverified_cert = fake_cert_;
  
  // Set up a secure TLS 1.3 connection status
  ssl_info->connection_status = 0;
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_3, &ssl_info->connection_status);
  SSLConnectionStatusSetCipherSuite(0x1301, &ssl_info->connection_status); // TLS_AES_128_GCM_SHA256
  
  ssl_info->is_issued_by_known_root = true;
  ssl_info->cert_status = 0;  // No errors - this is crucial for green lock
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  ssl_info->key_exchange_group = 29; // X25519
  ssl_info->peer_signature_algorithm = 0x0804; // rsa_pss_rsae_sha256
  
  // Mark as secure and valid
  ssl_info->is_fatal_cert_error = false;
  
  return true;
}

int64_t FakeSSLClientSocket::GetTotalReceivedBytes() const {
  return stream_socket_->GetTotalReceivedBytes();
}

void FakeSSLClientSocket::ApplySocketTag(const SocketTag& tag) {
  stream_socket_->ApplySocketTag(tag);
}

void FakeSSLClientSocket::CreateFakeCertificate() {
  // Use a real test certificate from Chromium's test data
  // This ensures the certificate appears valid and trusted
  
  // Convert the test certificate data to string_view
  std::vector<std::string_view> der_certs;
  der_certs.push_back(std::string_view(
      reinterpret_cast<const char*>(google_der), sizeof(google_der)));
  
  fake_cert_ = X509Certificate::CreateFromDERCertChain(der_certs);
  if (!fake_cert_) {
    // Fallback: try to create a minimal certificate
    std::vector<std::string_view> fallback_certs;
    fallback_certs.push_back("fake_cert_data");
    fake_cert_ = X509Certificate::CreateFromDERCertChain(fallback_certs);
  }
}

}  // namespace net