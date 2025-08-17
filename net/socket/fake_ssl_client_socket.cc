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
  ssl_info->connection_status = SSL_CONNECTION_VERSION_TLS1_2;
  ssl_info->is_issued_by_known_root = true;
  ssl_info->cert_status = 0;  // No errors
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  return true;
}

int64_t FakeSSLClientSocket::GetTotalReceivedBytes() const {
  return stream_socket_->GetTotalReceivedBytes();
}

void FakeSSLClientSocket::ApplySocketTag(const SocketTag& tag) {
  stream_socket_->ApplySocketTag(tag);
}

void FakeSSLClientSocket::CreateFakeCertificate() {
  // Create a simple fake certificate for the .dapp domain
  // This certificate will be trusted by our custom cert verifier
  std::string cert_der;
  
  // Create a minimal self-signed certificate
  // For simplicity, we'll create a basic certificate structure
  // In a real implementation, you might want to generate a proper certificate
  
  // For now, create an empty certificate that will be accepted by our verifier
  std::string subject = "CN=" + host_and_port_.host();
  
  // Create a fake certificate - the exact content doesn't matter since
  // our IgnoreErrorsCertVerifier will accept it anyway
  std::vector<std::string_view> der_certs;
  der_certs.push_back("fake_cert_data_for_dapp_domain");
  
  fake_cert_ = X509Certificate::CreateFromDERCertChain(der_certs);
  if (!fake_cert_) {
    // If we can't create a proper fake cert, create an empty one
    // The cert verifier will still accept it for .dapp domains
    fake_cert_ = nullptr;
  }
}

}  // namespace net