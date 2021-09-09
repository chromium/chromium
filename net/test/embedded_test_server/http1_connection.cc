// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http1_connection.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net {
namespace test_server {

Http1Connection::Http1Connection(
    std::unique_ptr<StreamSocket> socket,
    EmbeddedTestServerConnectionListener* connection_listener,
    EmbeddedTestServer* server_delegate)
    : socket_(std::move(socket)),
      connection_listener_(connection_listener),
      server_delegate_(server_delegate),
      read_buf_(base::MakeRefCounted<IOBufferWithSize>(4096)) {}

Http1Connection::~Http1Connection() {
  weak_factory_.InvalidateWeakPtrs();
}

void Http1Connection::SendResponseBytes(const std::string& response_string,
                                        SendCompleteCallback callback) {
  if (response_string.length() > 0) {
    scoped_refptr<DrainableIOBuffer> write_buf =
        base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<StringIOBuffer>(response_string),
            response_string.length());

    SendInternal(std::move(callback), write_buf);
  } else {
    std::move(callback).Run();
  }
}

void Http1Connection::OnSocketReady() {
  ReadData();
}

std::unique_ptr<StreamSocket> Http1Connection::TakeSocket() {
  return std::move(socket_);
}

const StreamSocket& Http1Connection::Socket() {
  return *socket_.get();
}

base::WeakPtr<HttpConnection> Http1Connection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void Http1Connection::ReadData() {
  while (true) {
    int rv = socket_->Read(read_buf_.get(), read_buf_->size(),
                           base::BindOnce(&Http1Connection::OnReadCompleted,
                                          weak_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;

    if (HandleReadResult(rv)) {
      return;
    }
  }
}

void Http1Connection::OnReadCompleted(int rv) {
  if (!HandleReadResult(rv))
    ReadData();
}

bool Http1Connection::HandleReadResult(int rv) {
  if (rv <= 0) {
    server_delegate_->RemoveConnection(this);
    return true;
  }

  if (connection_listener_)
    connection_listener_->ReadFromSocket(*socket_, rv);

  request_parser_.ProcessChunk(base::StringPiece(read_buf_->data(), rv));
  if (request_parser_.ParseRequest() != HttpRequestParser::ACCEPTED)
    return false;

  std::unique_ptr<HttpRequest> request = request_parser_.GetRequest();

  SSLInfo ssl_info;
  if (socket_->GetSSLInfo(&ssl_info)) {
    request->ssl_info = ssl_info;
    if (ssl_info.early_data_received)
      request->headers["Early-Data"] = "1";
  }

  server_delegate_->HandleRequest(this, std::move(request));
  return true;
}

void Http1Connection::SendInternal(base::OnceClosure callback,
                                   scoped_refptr<DrainableIOBuffer> buf) {
  while (buf->BytesRemaining() > 0) {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    callback = std::move(split_callback.first);
    int rv =
        socket_->Write(buf.get(), buf->BytesRemaining(),
                       base::BindOnce(&Http1Connection::OnSendInternalDone,
                                      base::Unretained(this),
                                      std::move(split_callback.second), buf),
                       TRAFFIC_ANNOTATION_FOR_TESTS);
    if (rv == ERR_IO_PENDING)
      return;

    if (rv < 0)
      break;
    buf->DidConsume(rv);
  }

  // The Http1Connection will be deleted by the callback since we only need
  // to serve a single request.
  std::move(callback).Run();
}

void Http1Connection::OnSendInternalDone(base::OnceClosure callback,
                                         scoped_refptr<DrainableIOBuffer> buf,
                                         int rv) {
  if (rv < 0) {
    std::move(callback).Run();
    return;
  }
  buf->DidConsume(rv);
  SendInternal(std::move(callback), buf);
}

}  // namespace test_server
}  // namespace net
