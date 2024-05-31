// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP1_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP1_CONNECTION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

class StreamSocket;

namespace test_server {

class EmbeddedTestServer;

// Wraps the connection socket. Accepts incoming data and sends responses via
// HTTP/1.1.
//
// Should be owned by the EmbeddedTestServer passed to the constructor.
//
// Lifetime of this connection (and the delegate) is one request/response pair.
// Calling FinishResponse will immediately send a signal to the owning
// EmbeddedTestServer that the connection can be safely destroyed and the socket
// can taken by a connection listener (if it has not already closed and a
// listener exists). The connection will also immediately signal the server
// to destroy the connection if the socket closes early or returns an error on
// read or write.
class Http1Connection : public HttpConnection, public HttpResponseDelegate {
 public:
  Http1Connection(std::unique_ptr<StreamSocket> socket,
                  EmbeddedTestServerConnectionListener* connection_listener,
                  EmbeddedTestServer* server_delegate);
  ~Http1Connection() override;
  Http1Connection(Http1Connection&) = delete;
  Http1Connection& operator=(Http1Connection&) = delete;

  // HttpConnection
  void OnSocketReady() override;
  std::unique_ptr<StreamSocket> TakeSocket() override;
  StreamSocket* Socket() override;
  base::WeakPtr<HttpConnection> GetWeakPtr() override;

  // HttpResponseDelegate
  void AddResponse(std::unique_ptr<HttpResponse> response) override;
  void SendResponseHeaders(HttpStatusCode status,
                           const std::string& status_reason,
                           const base::StringPairs& headers) override;
  void SendRawResponseHeaders(const std::string& headers) override;
  void SendContents(const std::string& contents,
                    base::OnceClosure callback) override;
  void FinishResponse() override;
  void SendContentsAndFinish(const std::string& contents) override;
  void SendHeadersContentAndFinish(HttpStatusCode status,
                                   const std::string& status_reason,
                                   const base::StringPairs& headers,
                                   const std::string& contents) override;

 private:
  void ReadData();
  void OnReadCompleted(int rv);
  bool HandleReadResult(int rv);
  void SendInternal(base::OnceClosure callback,
                    scoped_refptr<DrainableIOBuffer> buffer);
  void OnSendInternalDone(base::OnceClosure callback,
                          scoped_refptr<DrainableIOBuffer> buffer,
                          int rv);

  std::unique_ptr<StreamSocket> socket_;
  raw_ptr<EmbeddedTestServerConnectionListener, AcrossTasksDanglingUntriaged>
      connection_listener_;
  raw_ptr<EmbeddedTestServer> server_delegate_;
  HttpRequestParser request_parser_;
  scoped_refptr<IOBufferWithSize> read_buf_;
  std::vector<std::unique_ptr<HttpResponse>> responses_;

  base::WeakPtrFactory<Http1Connection> weak_factory_{this};
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP1_CONNECTION_H_
