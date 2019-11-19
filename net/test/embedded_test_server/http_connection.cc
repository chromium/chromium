// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_connection.h"

#include <utility>

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net {
namespace test_server {

HttpConnection::HttpConnection(std::unique_ptr<StreamSocket> socket,
                               const HandleRequestCallback& callback)
    : socket_(std::move(socket)),
      callback_(callback),
      read_buf_(base::MakeRefCounted<IOBufferWithSize>(4096)) {}

HttpConnection::~HttpConnection() {
  weak_factory_.InvalidateWeakPtrs();
}

void HttpConnection::SendResponseBytes(const std::string& response_string,
                                       const SendCompleteCallback& callback) {
  if (response_string.length() > 0) {
    scoped_refptr<DrainableIOBuffer> write_buf =
        base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<StringIOBuffer>(response_string),
            response_string.length());

    SendInternal(callback, write_buf);
  } else {
    callback.Run();
  }
}

int HttpConnection::ReadData(CompletionOnceCallback callback) {
  return socket_->Read(read_buf_.get(), read_buf_->size(), std::move(callback));
}

bool HttpConnection::ConsumeData(int size) {
  request_parser_.ProcessChunk(base::StringPiece(read_buf_->data(), size));
  if (request_parser_.ParseRequest() == HttpRequestParser::ACCEPTED) {
    callback_.Run(this, request_parser_.GetRequest());
    return true;
  }
  return false;
}

void HttpConnection::SendInternal(const base::Closure& callback,
                                  scoped_refptr<DrainableIOBuffer> buf) {
  while (buf->BytesRemaining() > 0) {
    int rv =
        socket_->Write(buf.get(), buf->BytesRemaining(),
                       base::BindOnce(&HttpConnection::OnSendInternalDone,
                                      base::Unretained(this), callback, buf),
                       TRAFFIC_ANNOTATION_FOR_TESTS);
    if (rv == ERR_IO_PENDING)
      return;

    if (rv < 0)
      break;
    buf->DidConsume(rv);
  }

  // The HttpConnection will be deleted by the callback since we only need to
  // serve a single request.
  callback.Run();
}

void HttpConnection::OnSendInternalDone(const base::Closure& callback,
                                        scoped_refptr<DrainableIOBuffer> buf,
                                        int rv) {
  if (rv < 0) {
    callback.Run();
    return;
  }
  buf->DidConsume(rv);
  SendInternal(callback, buf);
}

base::WeakPtr<HttpConnection> HttpConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace test_server
}  // namespace net
