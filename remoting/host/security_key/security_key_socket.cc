// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace remoting {

namespace {

const size_t kRequestSizeBytes = 4;
const size_t kMaxRequestLength = 16384;
const size_t kRequestReadBufferLength = kRequestSizeBytes + kMaxRequestLength;

// SSH Failure Code
const char kSshError[] = {0x05};

}  // namespace

SecurityKeySocket::SecurityKeySocket(std::unique_ptr<net::StreamSocket> socket,
                                     base::TimeDelta timeout,
                                     const base::Closure& timeout_callback)
    : socket_(std::move(socket)),
      read_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
          kRequestReadBufferLength)) {
  timer_.reset(new base::OneShotTimer());
  timer_->Start(FROM_HERE, timeout, timeout_callback);
}

SecurityKeySocket::~SecurityKeySocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool SecurityKeySocket::GetAndClearRequestData(std::string* data_out) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!waiting_for_request_);

  if (!IsRequestComplete() || IsRequestTooLarge()) {
    return false;
  }
  // The request size is not part of the data; don't send it.
  data_out->assign(request_data_.begin() + kRequestSizeBytes,
                   request_data_.end());
  request_data_.clear();
  return true;
}

void SecurityKeySocket::SendResponse(const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!write_buffer_);

  std::string response_length_string = GetResponseLengthAsBytes(response_data);
  int response_len = response_length_string.size() + response_data.size();
  std::unique_ptr<std::string> response(
      new std::string(response_length_string + response_data));
  write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(std::move(response)),
      response_len);

  DCHECK(write_buffer_->BytesRemaining());
  DoWrite();
}

void SecurityKeySocket::SendSshError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  SendResponse(std::string(kSshError, base::size(kSshError)));
}

void SecurityKeySocket::StartReadingRequest(
    const base::Closure& request_received_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request_received_callback_.is_null());

  waiting_for_request_ = true;
  request_received_callback_ = request_received_callback;

  DoRead();
}

void SecurityKeySocket::OnDataWritten(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(write_buffer_);

  if (result < 0) {
    LOG(ERROR) << "Error sending response: " << result;
    return;
  }
  ResetTimer();
  write_buffer_->DidConsume(result);

  if (!write_buffer_->BytesRemaining()) {
    write_buffer_ = nullptr;
    return;
  }

  DoWrite();
}

void SecurityKeySocket::DoWrite() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(write_buffer_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("security_key_socket", R"(
        semantics {
          sender: "Security Key Socket"
          description:
            "This request performs the communication between processes when "
            "handling security key (gnubby) authentication."
          trigger:
            "Performing an action (such as signing into a website with "
            "two-factor authentication enabled) that requires a security key "
            "touch."
          data: "Security key protocol data."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in Settings."
          chrome_policy {
            RemoteAccessHostAllowGnubbyAuth {
              RemoteAccessHostAllowGnubbyAuth: false
            }
          }
        })");
  int result = socket_->Write(
      write_buffer_.get(), write_buffer_->BytesRemaining(),
      base::Bind(&SecurityKeySocket::OnDataWritten, base::Unretained(this)),
      traffic_annotation);
  if (result != net::ERR_IO_PENDING) {
    OnDataWritten(result);
  }
}

void SecurityKeySocket::OnDataRead(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result <= 0) {
    if (result < 0) {
      LOG(ERROR) << "Error reading request: " << result;
      socket_read_error_ = true;
    }
    waiting_for_request_ = false;
    std::move(request_received_callback_).Run();
    return;
  }

  ResetTimer();
  // TODO(joedow): If there are multiple requests in a burst, it is possible
  // that we could read too many bytes from the buffer (e.g. all of request #1
  // and some of request #2).  We should consider using the request header to
  // determine the request length and only read that amount from buffer.
  request_data_.insert(request_data_.end(), read_buffer_->data(),
                       read_buffer_->data() + result);
  if (IsRequestComplete()) {
    waiting_for_request_ = false;
    std::move(request_received_callback_).Run();
    return;
  }

  DoRead();
}

void SecurityKeySocket::DoRead() {
  DCHECK(thread_checker_.CalledOnValidThread());

  int result = socket_->Read(
      read_buffer_.get(), kRequestReadBufferLength,
      base::Bind(&SecurityKeySocket::OnDataRead, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    OnDataRead(result);
  }
}

bool SecurityKeySocket::IsRequestComplete() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (request_data_.size() < kRequestSizeBytes) {
    return false;
  }
  return GetRequestLength() <= request_data_.size();
}

bool SecurityKeySocket::IsRequestTooLarge() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (request_data_.size() < kRequestSizeBytes) {
    return false;
  }
  return GetRequestLength() > kMaxRequestLength;
}

size_t SecurityKeySocket::GetRequestLength() const {
  DCHECK(request_data_.size() >= kRequestSizeBytes);

  return ((request_data_[0] & 255) << 24) + ((request_data_[1] & 255) << 16) +
         ((request_data_[2] & 255) << 8) + (request_data_[3] & 255) +
         kRequestSizeBytes;
}

std::string SecurityKeySocket::GetResponseLengthAsBytes(
    const std::string& response) const {
  std::string response_len;
  response_len.reserve(kRequestSizeBytes);
  int len = response.size();

  response_len.push_back((len >> 24) & 255);
  response_len.push_back((len >> 16) & 255);
  response_len.push_back((len >> 8) & 255);
  response_len.push_back(len & 255);

  return response_len;
}

void SecurityKeySocket::ResetTimer() {
  if (timer_->IsRunning()) {
    timer_->Reset();
  }
}

}  // namespace remoting
