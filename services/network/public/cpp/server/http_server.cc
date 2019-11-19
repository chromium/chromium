// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/server/http_server.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/server/http_connection.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/public/cpp/server/http_server_response_info.h"
#include "services/network/public/cpp/server/web_socket.h"

namespace network {

namespace server {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kHttpServerErrorResponseTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "services_http_server_error_response",
            R"(
      semantics {
        sender: "HTTP Server"
        description: "Error response from the built-in HTTP server."
        trigger: "Sending a request to the HTTP server that it can't handle."
        data: "A 500 error code."
        destination: OTHER
        destination_other: "Any destination the consumer selects."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made unless user activates an HTTP server."
        policy_exception_justification:
          "Not implemented, not used if HTTP Server is not activated."
      })");

}  // namespace

HttpServer::HttpServer(
    mojo::PendingRemote<mojom::TCPServerSocket> server_socket,
    HttpServer::Delegate* delegate)
    : server_socket_(std::move(server_socket)),
      delegate_(delegate),
      last_id_(0) {
  DCHECK(server_socket_);
  // Start accepting connections in next run loop in case when delegate is not
  // ready to get callbacks.
  DoAcceptLoop();
}

HttpServer::~HttpServer() = default;

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  DCHECK(connection->web_socket());
  connection->web_socket()->Accept(request, traffic_annotation);
}

void HttpServer::SendOverWebSocket(
    int connection_id,
    base::StringPiece data,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  DCHECK(connection->web_socket());
  connection->web_socket()->Send(data, traffic_annotation);
}

void HttpServer::SendRaw(int connection_id,
                         const std::string& data,
                         net::NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  if (connection->write_buf().size() + data.size() >
      connection->WriteBufferSize()) {
    LOG(ERROR) << "Write buffer is full.";
    return;
  }
  connection->write_buf().append(data);
  if (!connection->write_watcher().IsWatching()) {
    connection->write_watcher().Watch(
        connection->send_handle(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&HttpServer::OnWritable, base::Unretained(this),
                            connection->id()));
  }
}

void HttpServer::SendResponse(
    int connection_id,
    const HttpServerResponseInfo& response,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  SendRaw(connection_id, response.Serialize(), traffic_annotation);
}

void HttpServer::Send(int connection_id,
                      net::HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type,
                      net::NetworkTrafficAnnotationTag traffic_annotation) {
  HttpServerResponseInfo response(status_code);
  response.SetContentHeaders(data.size(), content_type);
  SendResponse(connection_id, response, traffic_annotation);
  SendRaw(connection_id, data, traffic_annotation);
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type,
                         net::NetworkTrafficAnnotationTag traffic_annotation) {
  Send(connection_id, net::HTTP_OK, data, content_type, traffic_annotation);
}

void HttpServer::Send404(int connection_id,
                         net::NetworkTrafficAnnotationTag traffic_annotation) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor404(),
               traffic_annotation);
}

void HttpServer::Send500(int connection_id,
                         const std::string& message,
                         net::NetworkTrafficAnnotationTag traffic_annotation) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor500(message),
               traffic_annotation);
}

void HttpServer::Close(int connection_id) {
  auto it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return;

  std::unique_ptr<HttpConnection> connection = std::move(it->second);
  id_to_connection_.erase(it);
  delegate_->OnClose(connection_id);

  // The call stack might have callbacks which still have the pointer of
  // connection. Instead of referencing connection with ID all the time,
  // destroys the connection in next run loop to make sure any pending
  // callbacks in the call stack return.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  connection.release());
}

bool HttpServer::SetReceiveBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->SetReadBufferSize(size);
  return connection;
}

bool HttpServer::SetSendBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->SetWriteBufferSize(size);
  return connection;
}

void HttpServer::DoAcceptLoop() {
  server_socket_->Accept(
      mojo::NullRemote(), /* observer */
      base::BindOnce(&HttpServer::OnAcceptCompleted, base::Unretained(this)));
}

void HttpServer::OnAcceptCompleted(
    int rv,
    const base::Optional<net::IPEndPoint>& remote_addr,
    mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  if (rv != net::OK) {
    LOG(ERROR) << "Accept error: rv=" << rv;
    return;
  }
  DCHECK(remote_addr);

  std::unique_ptr<HttpConnection> connection_ptr =
      std::make_unique<HttpConnection>(++last_id_, std::move(connected_socket),
                                       std::move(receive_pipe_handle),
                                       std::move(send_pipe_handle),
                                       remote_addr.value());
  HttpConnection* connection = connection_ptr.get();
  id_to_connection_[connection->id()] = std::move(connection_ptr);
  delegate_->OnConnect(connection->id());
  if (!HasClosedConnection(connection)) {
    connection->read_watcher().Watch(
        connection->receive_handle(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&HttpServer::OnReadable, base::Unretained(this),
                            connection->id()));
  }

  DoAcceptLoop();
}

void HttpServer::OnReadable(int connection_id,
                            MojoResult result,
                            const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    Close(connection_id);
    return;
  }

  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)
    return;

  const void* read_buffer;
  unsigned int bytes_read;
  result = connection->receive_handle().BeginReadData(&read_buffer, &bytes_read,
                                                      MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    connection->receive_handle().EndReadData(0);
    return;
  }
  if (result != MOJO_RESULT_OK) {
    Close(connection_id);
    return;
  }

  if (connection->read_buf().size() + bytes_read >
      connection->ReadBufferSize()) {
    LOG(ERROR) << "Read buffer is full.";
    connection->receive_handle().EndReadData(bytes_read);
    return;
  }

  connection->read_buf().append(static_cast<const char*>(read_buffer),
                                bytes_read);
  connection->receive_handle().EndReadData(bytes_read);
  HandleReadResult(connection, result);
}

void HttpServer::HandleReadResult(HttpConnection* connection, MojoResult rv) {
  if (rv != MOJO_RESULT_OK) {
    Close(connection->id());
    return;
  }

  // Handles http requests or websocket messages.
  while (!connection->read_buf().empty()) {
    if (connection->web_socket()) {
      std::string message;
      WebSocket::ParseResult result = connection->web_socket()->Read(&message);
      if (result == WebSocket::FRAME_INCOMPLETE) {
        break;
      }

      if (result == WebSocket::FRAME_CLOSE ||
          result == WebSocket::FRAME_ERROR) {
        Close(connection->id());
        return;
      }
      delegate_->OnWebSocketMessage(connection->id(), std::move(message));
      if (HasClosedConnection(connection)) {
        return;
      }
      continue;
    }

    HttpServerRequestInfo request;
    size_t pos = 0;
    if (!ParseHeaders(connection->read_buf().data(),
                      connection->read_buf().size(), &request, &pos)) {
      // An error has occured. Close the connection.
      Close(connection->id());
      return;
    } else if (!pos) {
      // If pos is 0, all the data in read_buf has been consumed, but the
      // headers have not been fully parsed yet. Continue parsing when more data
      // rolls in.
      break;
    }

    // Sets peer address.
    request.peer = connection->GetPeerAddress();

    if (request.HasHeaderValue("connection", "upgrade")) {
      connection->SetWebSocket(std::make_unique<WebSocket>(this, connection));
      connection->read_buf().erase(0, pos);
      delegate_->OnWebSocketRequest(connection->id(), request);
      if (HasClosedConnection(connection)) {
        return;
      }
      continue;
    }

    const char kContentLength[] = "content-length";
    if (request.headers.count(kContentLength) > 0) {
      size_t content_length = 0;
      const size_t kMaxBodySize = 100 << 20;
      if (!base::StringToSizeT(request.GetHeaderValue(kContentLength),
                               &content_length) ||
          content_length > kMaxBodySize) {
        SendResponse(connection->id(),
                     HttpServerResponseInfo::CreateFor500(
                         "request content-length too big or unknown: " +
                         request.GetHeaderValue(kContentLength)),
                     kHttpServerErrorResponseTrafficAnnotation);
        return;
      }

      if (connection->read_buf().size() - pos < content_length) {
        break;  // Not enough data was received yet.
      }
      request.data.assign(connection->read_buf().data() + pos, content_length);
      pos += content_length;
    }

    connection->read_buf().erase(0, pos);
    delegate_->OnHttpRequest(connection->id(), request);
    if (HasClosedConnection(connection)) {
      return;
    }
  }
}

void HttpServer::OnWritable(int connection_id, MojoResult result) {
  if (result != MOJO_RESULT_OK)
    return;

  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)
    return;

  std::string& write_buf = connection->write_buf();
  unsigned int buffer_size = write_buf.size();

  if (!connection->write_buf().empty()) {
    result = connection->send_handle().WriteData(write_buf.data(), &buffer_size,
                                                 MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      return;

    if (result != MOJO_RESULT_OK) {
      Close(connection->id());
      return;
    }

    connection->write_buf().erase(0, buffer_size);
  }

  if (connection->write_buf().empty())
    connection->write_watcher().Cancel();
}

namespace {

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_LWS,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method
  ST_URL,        // Receiving the URL
  ST_PROTO,      // Receiving the protocol
  ST_HEADER,     // Starting a Request Header
  ST_NAME,       // Receiving a request header name
  ST_SEPARATOR,  // Receiving the separator between header name and value
  ST_VALUE,      // Receiving a request header value
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
const int parser_state[MAX_STATES][MAX_INPUTS] = {
    /* METHOD    */ {ST_URL, ST_ERR, ST_ERR, ST_ERR, ST_METHOD},
    /* URL       */ {ST_PROTO, ST_ERR, ST_ERR, ST_URL, ST_URL},
    /* PROTOCOL  */ {ST_ERR, ST_HEADER, ST_NAME, ST_ERR, ST_PROTO},
    /* HEADER    */ {ST_ERR, ST_ERR, ST_NAME, ST_ERR, ST_ERR},
    /* NAME      */ {ST_SEPARATOR, ST_DONE, ST_ERR, ST_VALUE, ST_NAME},
    /* SEPARATOR */ {ST_SEPARATOR, ST_ERR, ST_ERR, ST_VALUE, ST_ERR},
    /* VALUE     */ {ST_VALUE, ST_HEADER, ST_NAME, ST_VALUE, ST_VALUE},
    /* DONE      */ {ST_DONE, ST_DONE, ST_DONE, ST_DONE, ST_DONE},
    /* ERR       */ {ST_ERR, ST_ERR, ST_ERR, ST_ERR, ST_ERR}};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch (ch) {
    case ' ':
    case '\t':
      return INPUT_LWS;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
  }
  return INPUT_DEFAULT;
}

}  // namespace

bool HttpServer::ParseHeaders(const char* data,
                              size_t data_len,
                              HttpServerRequestInfo* info,
                              size_t* ppos) {
  size_t& pos = *ppos;
  int state = ST_METHOD;
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = data[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    HttpServerRequestInfo::HeadersMap::iterator it;
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->path = buffer;
          buffer.clear();
          break;
        case ST_PROTO:
          if (buffer != "HTTP/1.1") {
            LOG(ERROR) << "Cannot handle request with protocol: " << buffer;
            next_state = ST_ERR;
          }
          buffer.clear();
          break;
        case ST_NAME:
          header_name = base::ToLowerASCII(buffer);
          buffer.clear();
          break;
        case ST_VALUE:
          base::TrimWhitespaceASCII(buffer, base::TRIM_LEADING, &header_value);
          it = info->headers.find(header_name);
          // See the second paragraph ("A sender MUST NOT generate multiple
          // header fields...") of tools.ietf.org/html/rfc7230#section-3.2.2.
          if (it == info->headers.end()) {
            info->headers[header_name] = header_value;
          } else {
            it->second.append(",");
            it->second.append(header_value);
          }
          buffer.clear();
          break;
        case ST_SEPARATOR:
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          // We got CR to get this far, also need the LF
          return (input == INPUT_LF);
        case ST_ERR:
          return false;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet. Signal this to
  // the caller by setting |pos| to zero.
  pos = 0;
  return true;
}

HttpConnection* HttpServer::FindConnection(int connection_id) {
  auto it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return nullptr;
  return it->second.get();
}

// This is called after any delegate callbacks are called to check if Close()
// has been called during callback processing. Using the pointer of connection,
// |connection| is safe here because Close() deletes the connection in next run
// loop.
bool HttpServer::HasClosedConnection(HttpConnection* connection) {
  return FindConnection(connection->id()) != connection;
}

}  // namespace server

}  // namespace network
