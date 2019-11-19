// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {

class StreamSocket;

namespace test_server {

class HttpConnection;

// Calblack called when a request is parsed. Response should be sent
// using HttpConnection::SendResponse() on the |connection| argument.
typedef base::Callback<void(HttpConnection* connection,
                            std::unique_ptr<HttpRequest> request)>
    HandleRequestCallback;

// Wraps the connection socket. Accepts incoming data and sends responses.
// If a valid request is parsed, then |callback_| is invoked.
class HttpConnection {
 public:
  HttpConnection(std::unique_ptr<StreamSocket> socket,
                 const HandleRequestCallback& callback);
  ~HttpConnection();

  // Sends the |response_string| to the client and calls |callback| once done.
  void SendResponseBytes(const std::string& response_string,
                         const SendCompleteCallback& callback);

  // Accepts raw chunk of data from the client. Internally, passes it to the
  // HttpRequestParser class. If a request is parsed, then |callback_| is
  // called.
  int ReadData(CompletionOnceCallback callback);

  bool ConsumeData(int size);

 private:
  friend class EmbeddedTestServer;

  void SendInternal(const base::Closure& callback,
                    scoped_refptr<DrainableIOBuffer> buffer);
  void OnSendInternalDone(const base::Closure& callback,
                          scoped_refptr<DrainableIOBuffer> buffer,
                          int rv);

  base::WeakPtr<HttpConnection> GetWeakPtr();

  std::unique_ptr<StreamSocket> socket_;
  const HandleRequestCallback callback_;
  HttpRequestParser request_parser_;
  scoped_refptr<IOBufferWithSize> read_buf_;

  base::WeakPtrFactory<HttpConnection> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECTION_H_
