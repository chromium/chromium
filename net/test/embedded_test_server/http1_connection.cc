// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http1_connection.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net::test_server {

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

void Http1Connection::OnSocketReady() {
  ReadData();
}

std::unique_ptr<StreamSocket> Http1Connection::TakeSocket() {
  return std::move(socket_);
}

StreamSocket* Http1Connection::Socket() {
  return socket_.get();
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

  request_parser_.ProcessChunk(std::string_view(read_buf_->data(), rv));
  if (request_parser_.ParseRequest() != HttpRequestParser::ACCEPTED)
    return false;

  std::unique_ptr<HttpRequest> request = request_parser_.GetRequest();

  SSLInfo ssl_info;
  if (socket_->GetSSLInfo(&ssl_info))
    request->ssl_info = ssl_info;

  server_delegate_->HandleRequest(weak_factory_.GetWeakPtr(),
                                  std::move(request), socket_.get());
  return true;
}

void Http1Connection::AddResponse(std::unique_ptr<HttpResponse> response) {
  responses_.push_back(std::move(response));
}

void Http1Connection::SendResponseHeaders(HttpStatusCode status,
                                          const std::string& status_reason,
                                          const base::StringPairs& headers) {
  std::string response_builder;

  base::StringAppendF(&response_builder, "HTTP/1.1 %d %s\r\n", status,
                      status_reason.c_str());
  for (const auto& header_pair : headers) {
    const std::string& header_name = header_pair.first;
    const std::string& header_value = header_pair.second;
    base::StringAppendF(&response_builder, "%s: %s\r\n", header_name.c_str(),
                        header_value.c_str());
  }

  base::StringAppendF(&response_builder, "\r\n");
  SendRawResponseHeaders(response_builder);
}

void Http1Connection::SendRawResponseHeaders(const std::string& headers) {
  SendContents(headers, base::DoNothing());
}

void Http1Connection::SendContents(const std::string& contents,
                                   base::OnceClosure callback) {
  if (contents.empty()) {
    std::move(callback).Run();
    return;
  }

  scoped_refptr<DrainableIOBuffer> buf =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<StringIOBuffer>(contents), contents.length());

  SendInternal(std::move(callback), buf);
}

void Http1Connection::FinishResponse() {
  server_delegate_->RemoveConnection(this, connection_listener_);
}

void Http1Connection::SendContentsAndFinish(const std::string& contents) {
  SendContents(contents, base::BindOnce(&HttpResponseDelegate::FinishResponse,
                                        weak_factory_.GetWeakPtr()));
}

void Http1Connection::SendHeadersContentAndFinish(
    HttpStatusCode status,
    const std::string& status_reason,
    const base::StringPairs& headers,
    const std::string& contents) {
  SendResponseHeaders(status, status_reason, headers);
  SendContentsAndFinish(contents);
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

}  // namespace net::test_server
