// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/webrtc/fake_ssl_client_socket.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace webrtc {

namespace {

// The constants below were taken from libjingle's socketadapters.cc.
// Basically, we do a "fake" SSL handshake to fool proxies into
// thinking this is a real SSL connection.

// This is a SSL v2 CLIENT_HELLO message.
// TODO(juberti): Should this have a session id? The response doesn't have a
// certificate, so the hello should have a session id.
static const uint8_t kSslClientHello[] = {
    0x80, 0x46,                                            // msg len
    0x01,                                                  // CLIENT_HELLO
    0x03, 0x01,                                            // SSL 3.1
    0x00, 0x2d,                                            // ciphersuite len
    0x00, 0x00,                                            // session id len
    0x00, 0x10,                                            // challenge len
    0x01, 0x00, 0x80, 0x03, 0x00, 0x80, 0x07, 0x00, 0xc0,  // ciphersuites
    0x06, 0x00, 0x40, 0x02, 0x00, 0x80, 0x04, 0x00, 0x80,  //
    0x00, 0x00, 0x04, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x0a,  //
    0x00, 0xfe, 0xfe, 0x00, 0x00, 0x09, 0x00, 0x00, 0x64,  //
    0x00, 0x00, 0x62, 0x00, 0x00, 0x03, 0x00, 0x00, 0x06,  //
    0x1f, 0x17, 0x0c, 0xa6, 0x2f, 0x00, 0x78, 0xfc,        // challenge
    0x46, 0x55, 0x2e, 0xb1, 0x83, 0x39, 0xf1, 0xea         //
};

// This is a TLSv1 SERVER_HELLO message.
static const uint8_t kSslServerHello[] = {
    0x16,                                            // handshake message
    0x03, 0x01,                                      // SSL 3.1
    0x00, 0x4a,                                      // message len
    0x02,                                            // SERVER_HELLO
    0x00, 0x00, 0x46,                                // handshake len
    0x03, 0x01,                                      // SSL 3.1
    0x42, 0x85, 0x45, 0xa7, 0x27, 0xa9, 0x5d, 0xa0,  // server random
    0xb3, 0xc5, 0xe7, 0x53, 0xda, 0x48, 0x2b, 0x3f,  //
    0xc6, 0x5a, 0xca, 0x89, 0xc1, 0x58, 0x52, 0xa1,  //
    0x78, 0x3c, 0x5b, 0x17, 0x46, 0x00, 0x85, 0x3f,  //
    0x20,                                            // session id len
    0x0e, 0xd3, 0x06, 0x72, 0x5b, 0x5b, 0x1b, 0x5f,  // session id
    0x15, 0xac, 0x13, 0xf9, 0x88, 0x53, 0x9d, 0x9b,  //
    0xe8, 0x3d, 0x7b, 0x0c, 0x30, 0x32, 0x6e, 0x38,  //
    0x4d, 0xa2, 0x75, 0x57, 0x41, 0x6c, 0x34, 0x5c,  //
    0x00, 0x04,                                      // RSA/RC4-128/MD5
    0x00                                             // null compression
};

// TODO(crbug.com/40171113): This annotation is not test specific but is for
// test. We should fix it.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "test",
        "Traffic annotation for FakeSSLClientSocket in jingle");

scoped_refptr<net::DrainableIOBuffer> NewDrainableIOBufferWithSize(int size) {
  return base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::IOBufferWithSize>(size), size);
}

}  // namespace

std::string_view FakeSSLClientSocket::GetSslClientHello() {
  return std::string_view(reinterpret_cast<const char*>(kSslClientHello),
                          std::size(kSslClientHello));
}

std::string_view FakeSSLClientSocket::GetSslServerHello() {
  return std::string_view(reinterpret_cast<const char*>(kSslServerHello),
                          std::size(kSslServerHello));
}

FakeSSLClientSocket::FakeSSLClientSocket(
    std::unique_ptr<net::StreamSocket> transport_socket)
    : transport_socket_(std::move(transport_socket)),
      next_handshake_state_(STATE_NONE),
      handshake_completed_(false),
      write_buf_(NewDrainableIOBufferWithSize(std::size(kSslClientHello))),
      read_buf_(NewDrainableIOBufferWithSize(std::size(kSslServerHello))) {
  CHECK(transport_socket_.get());
  std::memcpy(write_buf_->data(), kSslClientHello, std::size(kSslClientHello));
}

FakeSSLClientSocket::~FakeSSLClientSocket() = default;

int FakeSSLClientSocket::Read(net::IOBuffer* buf,
                              int buf_len,
                              net::CompletionOnceCallback callback) {
  DCHECK_EQ(next_handshake_state_, STATE_NONE);
  DCHECK(handshake_completed_);
  return transport_socket_->Read(buf, buf_len, std::move(callback));
}

int FakeSSLClientSocket::ReadIfReady(net::IOBuffer* buf,
                                     int buf_len,
                                     net::CompletionOnceCallback callback) {
  DCHECK_EQ(next_handshake_state_, STATE_NONE);
  DCHECK(handshake_completed_);
  return transport_socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int FakeSSLClientSocket::CancelReadIfReady() {
  DCHECK_EQ(next_handshake_state_, STATE_NONE);
  DCHECK(handshake_completed_);
  return transport_socket_->CancelReadIfReady();
}

int FakeSSLClientSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_EQ(next_handshake_state_, STATE_NONE);
  DCHECK(handshake_completed_);
  return transport_socket_->Write(buf, buf_len, std::move(callback),
                                  traffic_annotation);
}

int FakeSSLClientSocket::SetReceiveBufferSize(int32_t size) {
  return transport_socket_->SetReceiveBufferSize(size);
}

int FakeSSLClientSocket::SetSendBufferSize(int32_t size) {
  return transport_socket_->SetSendBufferSize(size);
}

int FakeSSLClientSocket::Connect(net::CompletionOnceCallback callback) {
  // We don't support synchronous operation, even if
  // |transport_socket_| does.
  DCHECK(!callback.is_null());
  DCHECK_EQ(next_handshake_state_, STATE_NONE);
  DCHECK(!handshake_completed_);
  DCHECK(user_connect_callback_.is_null());
  DCHECK_EQ(write_buf_->BytesConsumed(), 0);
  DCHECK_EQ(read_buf_->BytesConsumed(), 0);

  next_handshake_state_ = STATE_CONNECT;
  int status = DoHandshakeLoop();
  if (status == net::ERR_IO_PENDING)
    user_connect_callback_ = std::move(callback);

  return status;
}

int FakeSSLClientSocket::DoHandshakeLoop() {
  DCHECK_NE(next_handshake_state_, STATE_NONE);
  int status = net::OK;
  do {
    HandshakeState state = next_handshake_state_;
    next_handshake_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT:
        status = DoConnect();
        break;
      case STATE_SEND_CLIENT_HELLO:
        status = DoSendClientHello();
        break;
      case STATE_VERIFY_SERVER_HELLO:
        status = DoVerifyServerHello();
        break;
      default:
        status = net::ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state: " << state;
        break;
    }
  } while ((status != net::ERR_IO_PENDING) &&
           (next_handshake_state_ != STATE_NONE));
  return status;
}

void FakeSSLClientSocket::RunUserConnectCallback(int status) {
  DCHECK_LE(status, net::OK);
  next_handshake_state_ = STATE_NONE;
  std::move(user_connect_callback_).Run(status);
}

void FakeSSLClientSocket::DoHandshakeLoopWithUserConnectCallback() {
  int status = DoHandshakeLoop();
  if (status != net::ERR_IO_PENDING) {
    RunUserConnectCallback(status);
  }
}

int FakeSSLClientSocket::DoConnect() {
  int status = transport_socket_->Connect(base::BindOnce(
      &FakeSSLClientSocket::OnConnectDone, base::Unretained(this)));
  if (status != net::OK) {
    return status;
  }
  ProcessConnectDone();
  return net::OK;
}

void FakeSSLClientSocket::OnConnectDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_LE(status, net::OK);
  DCHECK(!user_connect_callback_.is_null());
  if (status != net::OK) {
    RunUserConnectCallback(status);
    return;
  }
  ProcessConnectDone();
  DoHandshakeLoopWithUserConnectCallback();
}

void FakeSSLClientSocket::ProcessConnectDone() {
  DCHECK_EQ(write_buf_->BytesConsumed(), 0);
  DCHECK_EQ(read_buf_->BytesConsumed(), 0);
  next_handshake_state_ = STATE_SEND_CLIENT_HELLO;
}

int FakeSSLClientSocket::DoSendClientHello() {
  int status = transport_socket_->Write(
      write_buf_.get(), write_buf_->BytesRemaining(),
      base::BindOnce(&FakeSSLClientSocket::OnSendClientHelloDone,
                     base::Unretained(this)),
      kTrafficAnnotation);
  if (status < net::OK) {
    return status;
  }
  ProcessSendClientHelloDone(static_cast<size_t>(status));
  return net::OK;
}

void FakeSSLClientSocket::OnSendClientHelloDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK(!user_connect_callback_.is_null());
  if (status < net::OK) {
    RunUserConnectCallback(status);
    return;
  }
  ProcessSendClientHelloDone(static_cast<size_t>(status));
  DoHandshakeLoopWithUserConnectCallback();
}

void FakeSSLClientSocket::ProcessSendClientHelloDone(size_t written) {
  DCHECK_LE(written, static_cast<size_t>(write_buf_->BytesRemaining()));
  DCHECK_EQ(read_buf_->BytesConsumed(), 0);
  if (written < static_cast<size_t>(write_buf_->BytesRemaining())) {
    next_handshake_state_ = STATE_SEND_CLIENT_HELLO;
    write_buf_->DidConsume(written);
  } else {
    next_handshake_state_ = STATE_VERIFY_SERVER_HELLO;
  }
}

int FakeSSLClientSocket::DoVerifyServerHello() {
  int status = transport_socket_->Read(
      read_buf_.get(), read_buf_->BytesRemaining(),
      base::BindOnce(&FakeSSLClientSocket::OnVerifyServerHelloDone,
                     base::Unretained(this)));
  if (status < net::OK) {
    return status;
  }
  size_t read = static_cast<size_t>(status);
  return ProcessVerifyServerHelloDone(read);
}

void FakeSSLClientSocket::OnVerifyServerHelloDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK(!user_connect_callback_.is_null());
  if (status < net::OK) {
    RunUserConnectCallback(status);
    return;
  }
  size_t read = static_cast<size_t>(status);
  status = ProcessVerifyServerHelloDone(read);
  if (status < net::OK) {
    RunUserConnectCallback(status);
    return;
  }
  if (handshake_completed_) {
    RunUserConnectCallback(net::OK);
  } else {
    DoHandshakeLoopWithUserConnectCallback();
  }
}

net::Error FakeSSLClientSocket::ProcessVerifyServerHelloDone(size_t read) {
  DCHECK_LE(read, static_cast<size_t>(read_buf_->BytesRemaining()));
  if (read == 0U) {
    return net::ERR_UNEXPECTED;
  }
  const uint8_t* expected_data_start =
      &kSslServerHello[std::size(kSslServerHello) -
                       read_buf_->BytesRemaining()];
  if (std::memcmp(expected_data_start, read_buf_->data(), read) != 0) {
    return net::ERR_UNEXPECTED;
  }
  if (read < static_cast<size_t>(read_buf_->BytesRemaining())) {
    next_handshake_state_ = STATE_VERIFY_SERVER_HELLO;
    read_buf_->DidConsume(read);
  } else {
    next_handshake_state_ = STATE_NONE;
    handshake_completed_ = true;
  }
  return net::OK;
}

void FakeSSLClientSocket::Disconnect() {
  transport_socket_->Disconnect();
  next_handshake_state_ = STATE_NONE;
  handshake_completed_ = false;
  user_connect_callback_.Reset();
  write_buf_->SetOffset(0);
  read_buf_->SetOffset(0);
}

bool FakeSSLClientSocket::IsConnected() const {
  return handshake_completed_ && transport_socket_->IsConnected();
}

bool FakeSSLClientSocket::IsConnectedAndIdle() const {
  return handshake_completed_ && transport_socket_->IsConnectedAndIdle();
}

int FakeSSLClientSocket::GetPeerAddress(net::IPEndPoint* address) const {
  return transport_socket_->GetPeerAddress(address);
}

int FakeSSLClientSocket::GetLocalAddress(net::IPEndPoint* address) const {
  return transport_socket_->GetLocalAddress(address);
}

const net::NetLogWithSource& FakeSSLClientSocket::NetLog() const {
  return transport_socket_->NetLog();
}

bool FakeSSLClientSocket::WasEverUsed() const {
  return transport_socket_->WasEverUsed();
}

net::NextProto FakeSSLClientSocket::GetNegotiatedProtocol() const {
  return transport_socket_->GetNegotiatedProtocol();
}

bool FakeSSLClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return transport_socket_->GetSSLInfo(ssl_info);
}

int64_t FakeSSLClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void FakeSSLClientSocket::ApplySocketTag(const net::SocketTag& tag) {
  NOTIMPLEMENTED();
}

}  // namespace webrtc
