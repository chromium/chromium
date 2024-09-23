// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_network_transaction.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/socket/connection_attempts.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#include "services/network/throttling/throttling_upload_data_stream.h"

namespace network {

ThrottlingNetworkTransaction::ThrottlingNetworkTransaction(
    std::unique_ptr<net::HttpTransaction> network_transaction)
    : throttled_byte_count_(0),
      network_transaction_(std::move(network_transaction)) {}

ThrottlingNetworkTransaction::~ThrottlingNetworkTransaction() {
  if (interceptor_ && !throttle_callback_.is_null())
    interceptor_->StopThrottle(throttle_callback_);
}

void ThrottlingNetworkTransaction::IOCallback(
    bool start,
    int result) {
  DCHECK(callback_);
  result = Throttle(start, result);
  if (result != net::ERR_IO_PENDING)
    std::move(callback_).Run(result);
}

int ThrottlingNetworkTransaction::Throttle(
    bool start,
    int result) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_ || result < 0)
    return result;

  base::TimeTicks send_end;
  if (start) {
    throttled_byte_count_ += network_transaction_->GetTotalReceivedBytes();
    net::LoadTimingInfo load_timing_info;
    if (GetLoadTimingInfo(&load_timing_info)) {
      send_end = load_timing_info.send_end;
      if (!load_timing_info.push_start.is_null())
        start = false;
    }
    if (send_end.is_null())
      send_end = base::TimeTicks::Now();
  }
  if (result > 0)
    throttled_byte_count_ += result;

  throttle_callback_ = base::BindRepeating(
      &ThrottlingNetworkTransaction::ThrottleCallback, base::Unretained(this));
  int rv = interceptor_->StartThrottle(result, throttled_byte_count_, send_end,
                                       start, false, throttle_callback_);
  if (rv != net::ERR_IO_PENDING)
    throttle_callback_.Reset();
  if (rv == net::ERR_INTERNET_DISCONNECTED)
    Fail();
  return rv;
}

void ThrottlingNetworkTransaction::ThrottleCallback(
    int result,
    int64_t bytes) {
  DCHECK(callback_);
  DCHECK(!throttle_callback_.is_null());

  throttle_callback_.Reset();
  if (result == net::ERR_INTERNET_DISCONNECTED)
    Fail();
  throttled_byte_count_ = bytes;
  std::move(callback_).Run(result);
}

void ThrottlingNetworkTransaction::Fail() {
  DCHECK(started_);
  DCHECK(!failed_);
  failed_ = true;
  network_transaction_->SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback());
  if (interceptor_)
    interceptor_.reset();
}

bool ThrottlingNetworkTransaction::CheckFailed() {
  if (failed_)
    return true;
  if (interceptor_ && interceptor_->IsOffline()) {
    Fail();
    return true;
  }
  return false;
}

int ThrottlingNetworkTransaction::Start(const net::HttpRequestInfo* request,
                                        net::CompletionOnceCallback callback,
                                        const net::NetLogWithSource& net_log) {
  DCHECK(request);
  started_ = true;

  ThrottlingNetworkInterceptor* interceptor =
      ThrottlingController::GetInterceptor(net_log.source().id);

  if (interceptor) {
    custom_request_ = std::make_unique<net::HttpRequestInfo>(*request);

    if (request->upload_data_stream) {
      custom_upload_data_stream_ = std::make_unique<ThrottlingUploadDataStream>(
          request->upload_data_stream);
      custom_request_->upload_data_stream = custom_upload_data_stream_.get();
    }

    request = custom_request_.get();

    interceptor_ = interceptor->GetWeakPtr();
    if (custom_upload_data_stream_)
      custom_upload_data_stream_->SetInterceptor(interceptor);
  }

  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;

  if (!interceptor_)
    return network_transaction_->Start(request, std::move(callback), net_log);

  callback_ = std::move(callback);
  int result = network_transaction_->Start(
      request,
      base::BindOnce(&ThrottlingNetworkTransaction::IOCallback,
                     base::Unretained(this), true),
      net_log);
  return Throttle(true, result);
}

int ThrottlingNetworkTransaction::RestartIgnoringLastError(
    net::CompletionOnceCallback callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->RestartIgnoringLastError(std::move(callback));

  callback_ = std::move(callback);
  int result = network_transaction_->RestartIgnoringLastError(base::BindOnce(
      &ThrottlingNetworkTransaction::IOCallback, base::Unretained(this), true));
  return Throttle(true, result);
}

int ThrottlingNetworkTransaction::RestartWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key,
    net::CompletionOnceCallback callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_) {
    return network_transaction_->RestartWithCertificate(
        std::move(client_cert), std::move(client_private_key),
        std::move(callback));
  }

  callback_ = std::move(callback);
  int result = network_transaction_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key),
      base::BindOnce(&ThrottlingNetworkTransaction::IOCallback,
                     base::Unretained(this), true));
  return Throttle(true, result);
}

int ThrottlingNetworkTransaction::RestartWithAuth(
    const net::AuthCredentials& credentials,
    net::CompletionOnceCallback callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->RestartWithAuth(credentials,
                                                 std::move(callback));

  callback_ = std::move(callback);
  int result = network_transaction_->RestartWithAuth(
      credentials, base::BindOnce(&ThrottlingNetworkTransaction::IOCallback,
                                  base::Unretained(this), true));
  return Throttle(true, result);
}

bool ThrottlingNetworkTransaction::IsReadyToRestartForAuth() {
  return network_transaction_->IsReadyToRestartForAuth();
}

int ThrottlingNetworkTransaction::Read(net::IOBuffer* buf,
                                       int buf_len,
                                       net::CompletionOnceCallback callback) {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  if (!interceptor_)
    return network_transaction_->Read(buf, buf_len, std::move(callback));

  callback_ = std::move(callback);
  int result = network_transaction_->Read(
      buf, interceptor_->GetReadBufLen(buf_len),
      base::BindOnce(&ThrottlingNetworkTransaction::IOCallback,
                     base::Unretained(this), false));
  // URLRequestJob relies on synchronous end-of-stream notification.
  if (result == 0)
    return result;
  return Throttle(false, result);
}

void ThrottlingNetworkTransaction::StopCaching() {
  network_transaction_->StopCaching();
}

int64_t ThrottlingNetworkTransaction::GetTotalReceivedBytes() const {
  return network_transaction_->GetTotalReceivedBytes();
}

int64_t ThrottlingNetworkTransaction::GetTotalSentBytes() const {
  return network_transaction_->GetTotalSentBytes();
}

int64_t ThrottlingNetworkTransaction::GetReceivedBodyBytes() const {
  return network_transaction_->GetReceivedBodyBytes();
}

void ThrottlingNetworkTransaction::DoneReading() {
  network_transaction_->DoneReading();
}

const net::HttpResponseInfo* ThrottlingNetworkTransaction::GetResponseInfo()
    const {
  return network_transaction_->GetResponseInfo();
}

net::LoadState ThrottlingNetworkTransaction::GetLoadState() const {
  return network_transaction_->GetLoadState();
}

void ThrottlingNetworkTransaction::SetQuicServerInfo(
    net::QuicServerInfo* quic_server_info) {
  network_transaction_->SetQuicServerInfo(quic_server_info);
}

bool ThrottlingNetworkTransaction::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  return network_transaction_->GetLoadTimingInfo(load_timing_info);
}

bool ThrottlingNetworkTransaction::GetRemoteEndpoint(
    net::IPEndPoint* endpoint) const {
  return network_transaction_->GetRemoteEndpoint(endpoint);
}

void ThrottlingNetworkTransaction::PopulateNetErrorDetails(
    net::NetErrorDetails* details) const {
  return network_transaction_->PopulateNetErrorDetails(details);
}

void ThrottlingNetworkTransaction::SetPriority(net::RequestPriority priority) {
  network_transaction_->SetPriority(priority);
}

void ThrottlingNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  network_transaction_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void ThrottlingNetworkTransaction::SetBeforeNetworkStartCallback(
    BeforeNetworkStartCallback callback) {
  network_transaction_->SetBeforeNetworkStartCallback(std::move(callback));
}

void ThrottlingNetworkTransaction::SetRequestHeadersCallback(
    net::RequestHeadersCallback callback) {
  network_transaction_->SetRequestHeadersCallback(std::move(callback));
}

void ThrottlingNetworkTransaction::SetResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  network_transaction_->SetResponseHeadersCallback(std::move(callback));
}

void ThrottlingNetworkTransaction::SetEarlyResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  network_transaction_->SetEarlyResponseHeadersCallback(std::move(callback));
}

void ThrottlingNetworkTransaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  network_transaction_->SetConnectedCallback(callback);
}

void ThrottlingNetworkTransaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(net::HttpRequestHeaders*)> callback) {
  network_transaction_->SetModifyRequestHeadersCallback(std::move(callback));
}

void ThrottlingNetworkTransaction::SetIsSharedDictionaryReadAllowedCallback(
    base::RepeatingCallback<bool()> callback) {
  // This method should not be called for this class.
  NOTREACHED();
}

int ThrottlingNetworkTransaction::ResumeNetworkStart() {
  if (CheckFailed())
    return net::ERR_INTERNET_DISCONNECTED;
  return network_transaction_->ResumeNetworkStart();
}

net::ConnectionAttempts ThrottlingNetworkTransaction::GetConnectionAttempts()
    const {
  return network_transaction_->GetConnectionAttempts();
}

void ThrottlingNetworkTransaction::CloseConnectionOnDestruction() {
  network_transaction_->CloseConnectionOnDestruction();
}

bool ThrottlingNetworkTransaction::IsMdlMatchForMetrics() const {
  return network_transaction_->IsMdlMatchForMetrics();
}

}  // namespace network
