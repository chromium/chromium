// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/server/http_server.h"

#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/server/web_socket.h"
#include "net/server/web_socket_parse_result.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace net {

namespace {

constexpr NetworkTrafficAnnotationTag
    kHttpServerErrorResponseTrafficAnnotation =
        DefineNetworkTrafficAnnotation("http_server_error_response",
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

HttpServer::HttpServer(std::unique_ptr<ServerSocket> server_socket,
                       HttpServer::Delegate* delegate)
    : server_socket_(std::move(server_socket)), delegate_(delegate) {
  DCHECK(server_socket_);
  // Start accepting connections in next run loop in case when delegate is not
  // ready to get callbacks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HttpServer::DoAcceptLoop,
                                weak_ptr_factory_.GetWeakPtr()));
}

HttpServer::~HttpServer() = default;

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request,
    NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == nullptr)
    return;
  DCHECK(connection->web_socket());
  connection->web_socket()->Accept(request, traffic_annotation);
}

void HttpServer::SendOverWebSocket(
    int connection_id,
    std::string_view data,
    NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == nullptr)
    return;
  DCHECK(connection->web_socket());
  connection->web_socket()->Send(
      data, WebSocketFrameHeader::OpCodeEnum::kOpCodeText, traffic_annotation);
}

void HttpServer::SendRaw(int connection_id,
                         const std::string& data,
                         NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == nullptr)
    return;

  bool writing_in_progress = !connection->write_buf()->IsEmpty();
  if (connection->write_buf()->Append(data) && !writing_in_progress)
    DoWriteLoop(connection, traffic_annotation);
}

void HttpServer::SendResponse(int connection_id,
                              const HttpServerResponseInfo& response,
                              NetworkTrafficAnnotationTag traffic_annotation) {
  SendRaw(connection_id, response.Serialize(), traffic_annotation);
}

void HttpServer::Send(int connection_id,
                      HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type,
                      NetworkTrafficAnnotationTag traffic_annotation) {
  HttpServerResponseInfo response(status_code);
  response.SetContentHeaders(data.size(), content_type);
  SendResponse(connection_id, response, traffic_annotation);
  SendRaw(connection_id, data, traffic_annotation);
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type,
                         NetworkTrafficAnnotationTag traffic_annotation) {
  Send(connection_id, HTTP_OK, data, content_type, traffic_annotation);
}

void HttpServer::Send404(int connection_id,
                         NetworkTrafficAnnotationTag traffic_annotation) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor404(),
               traffic_annotation);
}

void HttpServer::Send500(int connection_id,
                         const std::string& message,
                         NetworkTrafficAnnotationTag traffic_annotation) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor500(message),
               traffic_annotation);
}

void HttpServer::Close(int connection_id) {
  auto it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return;

  closed_connections_.emplace_back(std::move(it->second));
  id_to_connection_.erase(it);
  delegate_->OnClose(connection_id);

  // The call stack might have callbacks which still have the pointer of
  // connection. Instead of referencing connection with ID all the time,
  // destroys the connection in next run loop to make sure any pending
  // callbacks in the call stack return. List of closed Connections is owned
  // by `this` in case `this` is destroyed before the task runs. Connections may
  // not outlive `this`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HttpServer::DestroyClosedConnections,
                                weak_ptr_factory_.GetWeakPtr()));
}

int HttpServer::GetLocalAddress(IPEndPoint* address) {
  return server_socket_->GetLocalAddress(address);
}

void HttpServer::SetReceiveBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->read_buf()->set_max_buffer_size(size);
}

void HttpServer::SetSendBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->write_buf()->set_max_buffer_size(size);
}

void HttpServer::DoAcceptLoop() {
  int rv;
  do {
    rv = server_socket_->Accept(&accepted_socket_,
                                base::BindOnce(&HttpServer::OnAcceptCompleted,
                                               weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleAcceptResult(rv);
  } while (rv == OK);
}

void HttpServer::OnAcceptCompleted(int rv) {
  if (HandleAcceptResult(rv) == OK)
    DoAcceptLoop();
}

int HttpServer::HandleAcceptResult(int rv) {
  if (rv < 0) {
    LOG(ERROR) << "Accept error: rv=" << rv;
    return rv;
  }

  std::unique_ptr<HttpConnection> connection_ptr =
      std::make_unique<HttpConnection>(++last_id_, std::move(accepted_socket_));
  HttpConnection* connection = connection_ptr.get();
  id_to_connection_[connection->id()] = std::move(connection_ptr);
  delegate_->OnConnect(connection->id());
  if (!HasClosedConnection(connection))
    DoReadLoop(connection);
  return OK;
}

void HttpServer::DoReadLoop(HttpConnection* connection) {
  int rv;
  do {
    HttpConnection::ReadIOBuffer* read_buf = connection->read_buf();
    // Increases read buffer size if necessary.
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
      Close(connection->id());
      return;
    }

    rv = connection->socket()->Read(
        read_buf, read_buf->RemainingCapacity(),
        base::BindOnce(&HttpServer::OnReadCompleted,
                       weak_ptr_factory_.GetWeakPtr(), connection->id()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleReadResult(connection, rv);
  } while (rv == OK);
}

void HttpServer::OnReadCompleted(int connection_id, int rv) {
  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by write error.
    return;

  if (HandleReadResult(connection, rv) == OK)
    DoReadLoop(connection);
}

int HttpServer::HandleReadResult(HttpConnection* connection, int rv) {
  if (rv <= 0) {
    Close(connection->id());
    return rv == 0 ? ERR_CONNECTION_CLOSED : rv;
  }

  HttpConnection::ReadIOBuffer* read_buf = connection->read_buf();
  read_buf->DidRead(rv);

  // Handles http requests or websocket messages.
  while (read_buf->GetSize() > 0) {
    if (connection->web_socket()) {
      std::string message;
      WebSocketParseResult result = connection->web_socket()->Read(&message);
      if (result == WebSocketParseResult::FRAME_INCOMPLETE) {
        break;
      }

      if (result == WebSocketParseResult::FRAME_CLOSE ||
          result == WebSocketParseResult::FRAME_ERROR) {
        Close(connection->id());
        return ERR_CONNECTION_CLOSED;
      }
      if (result == WebSocketParseResult::FRAME_OK_FINAL) {
        delegate_->OnWebSocketMessage(connection->id(), std::move(message));
      }
      if (HasClosedConnection(connection))
        return ERR_CONNECTION_CLOSED;
      continue;
    }

    // The headers are reparsed from the beginning every time a packet is
    // received. This only really matters if something tries to upload a large
    // request body.
    HttpServerRequestInfo request;
    size_t pos = 0;
    if (!ParseHeaders(read_buf->StartOfBuffer(), read_buf->GetSize(),
                      &request, &pos)) {
      // An error has occured. Close the connection.
      Close(connection->id());
      return ERR_CONNECTION_CLOSED;
    } else if (!pos) {
      // If pos is 0, all the data in read_buf has been consumed, but the
      // headers have not been fully parsed yet. Continue parsing when more data
      // rolls in.
      break;
    }

    // Sets peer address if exists.
    connection->socket()->GetPeerAddress(&request.peer);

    if (request.HasHeaderValue("connection", "upgrade") &&
        request.HasHeaderValue("upgrade", "websocket")) {
      connection->SetWebSocket(std::make_unique<WebSocket>(this, connection));
      read_buf->DidConsume(pos);
      delegate_->OnWebSocketRequest(connection->id(), request);
      if (HasClosedConnection(connection))
        return ERR_CONNECTION_CLOSED;
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
                         "request content-length too big or unknown."),
                     kHttpServerErrorResponseTrafficAnnotation);
        Close(connection->id());
        return ERR_CONNECTION_CLOSED;
      }

      if (read_buf->GetSize() - pos < content_length)
        break;  // Not enough data was received yet.
      request.data.assign(read_buf->StartOfBuffer() + pos, content_length);
      pos += content_length;
    }

    read_buf->DidConsume(pos);
    delegate_->OnHttpRequest(connection->id(), request);
    if (HasClosedConnection(connection))
      return ERR_CONNECTION_CLOSED;
  }

  return OK;
}

void HttpServer::DoWriteLoop(HttpConnection* connection,
                             NetworkTrafficAnnotationTag traffic_annotation) {
  int rv = OK;
  HttpConnection::QueuedWriteIOBuffer* write_buf = connection->write_buf();
  while (rv == OK && write_buf->GetSizeToWrite() > 0) {
    rv = connection->socket()->Write(
        write_buf, write_buf->GetSizeToWrite(),
        base::BindOnce(&HttpServer::OnWriteCompleted,
                       weak_ptr_factory_.GetWeakPtr(), connection->id(),
                       traffic_annotation),
        traffic_annotation);
    if (rv == ERR_IO_PENDING || rv == OK)
      return;
    rv = HandleWriteResult(connection, rv);
  }
}

void HttpServer::OnWriteCompleted(
    int connection_id,
    NetworkTrafficAnnotationTag traffic_annotation,
    int rv) {
  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by read error.
    return;

  if (HandleWriteResult(connection, rv) == OK)
    DoWriteLoop(connection, traffic_annotation);
}

int HttpServer::HandleWriteResult(HttpConnection* connection, int rv) {
  if (rv < 0) {
    Close(connection->id());
    return rv;
  }

  connection->write_buf()->DidConsume(rv);
  return OK;
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
enum HeaderParseInputs {
  INPUT_LWS,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum HeaderParseStates {
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

// This state machine has a number of bugs, for example it considers
// "HTTP/1.1 200 OK\r\n"
// "Foo\r\n"
// to be a correctly terminated set of request headers. It also accepts "\n"
// between header lines but requires "\r\n" at the end of the headers.
// TODO(crbug): Consider using a different request parser. Maybe balsa headers
// from QUICHE, if it doesn't increase the binary size too much.

// State transition table
constexpr int kParserState[MAX_STATES][MAX_INPUTS] = {
    /* METHOD    */ {ST_URL, ST_ERR, ST_ERR, ST_ERR, ST_METHOD},
    /* URL       */ {ST_PROTO, ST_ERR, ST_ERR, ST_URL, ST_URL},
    /* PROTOCOL  */ {ST_ERR, ST_HEADER, ST_NAME, ST_ERR, ST_PROTO},
    /* HEADER    */ {ST_ERR, ST_ERR, ST_NAME, ST_ERR, ST_ERR},
    /* NAME      */ {ST_SEPARATOR, ST_DONE, ST_ERR, ST_VALUE, ST_NAME},
    /* SEPARATOR */ {ST_SEPARATOR, ST_ERR, ST_ERR, ST_VALUE, ST_ERR},
    /* VALUE     */ {ST_VALUE, ST_HEADER, ST_NAME, ST_VALUE, ST_VALUE},
    /* DONE      */ {ST_ERR, ST_ERR, ST_DONE, ST_ERR, ST_ERR},
    /* ERR       */ {ST_ERR, ST_ERR, ST_ERR, ST_ERR, ST_ERR}};

// Convert an input character to the parser's input token.
int CharToInputType(char ch) {
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
  // Copy *ppos to avoid the compiler having to think about pointer aliasing.
  size_t pos = *ppos;
  // Make sure `pos` is always written back to `ppos` even if an extra return is
  // added to the function.
  absl::Cleanup set_ppos = [&pos, ppos]() { *ppos = pos; };
  int state = ST_METHOD;
  // Technically a base::span<const uint8_t> would be more correct, but using a
  // std::string_view makes integration with the rest of the code easier.
  const std::string_view data_view(data, data_len);
  size_t token_start = pos;
  std::string header_name;
  for (; pos < data_len; ++pos) {
    const char ch = data[pos];
    if (ch == '\0') {
      // Lots of code assumes strings don't contain null characters, so disallow
      // them to be on the safe side.
      return false;
    }
    const int input = CharToInputType(ch);
    const int next_state = kParserState[state][input];
    if (next_state == ST_ERR) {
      // No point in continuing.
      return false;
    }

    const bool transition = (next_state != state);
    if (transition) {
      const std::string_view token =
          data_view.substr(token_start, pos - token_start);
      token_start = pos + 1;  // Skip the whitespace or separator.
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = std::string(token);
          break;
        case ST_URL:
          info->path = std::string(token);
          break;
        case ST_PROTO:
          if (token != "HTTP/1.1") {
            LOG(ERROR) << "Cannot handle request with protocol: " << token;
            return false;
          }
          break;
        case ST_NAME:
          header_name = base::ToLowerASCII(token);
          break;
        case ST_VALUE: {
          std::string_view header_value =
              base::TrimWhitespaceASCII(token, base::TRIM_LEADING);
          // See the second paragraph ("A sender MUST NOT generate multiple
          // header fields...") of tools.ietf.org/html/rfc7230#section-3.2.2.
          auto [it, inserted] = info->headers.try_emplace(
              std::move(header_name), std::move(header_value));
          header_name.clear();  // Avoid use-after-move lint error.
          if (!inserted) {
            // Since the insertion did not happen, try_emplace() did not move
            // the contents of `header_value` and we can still use it.
            std::string& value = it->second;
            value.reserve(value.size() + 1 + header_value.size());
            value.push_back(',');
            value.append(header_value);
          }
          break;
        }
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      if (state == ST_DONE) {
        ++pos;  // Point to the first byte of the body.
        return true;
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

void HttpServer::DestroyClosedConnections() {
  closed_connections_.clear();
}

}  // namespace net
