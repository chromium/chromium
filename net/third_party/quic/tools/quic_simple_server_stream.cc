// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_simple_server_stream.h"

#include <list>
#include <utility>

#include "net/third_party/quic/core/http/quic_spdy_stream.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/tools/quic_simple_server_session.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

using spdy::SpdyHeaderBlock;

namespace quic {

QuicSimpleServerStream::QuicSimpleServerStream(
    QuicStreamId id,
    QuicSpdySession* session,
    StreamType type,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSpdyServerStreamBase(id, session, type),
      content_length_(-1),
      quic_simple_server_backend_(quic_simple_server_backend) {}

QuicSimpleServerStream::~QuicSimpleServerStream() {
  quic_simple_server_backend_->CloseBackendResponseStream(this);
}

void QuicSimpleServerStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &content_length_,
                                         &request_headers_)) {
    QUIC_DVLOG(1) << "Invalid headers";
    SendErrorResponse();
  }
  ConsumeHeaderList();
}

void QuicSimpleServerStream::OnTrailingHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QUIC_BUG << "Server does not support receiving Trailers.";
  SendErrorResponse();
}

void QuicSimpleServerStream::OnDataAvailable() {
  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    QUIC_DVLOG(1) << "Stream " << id() << " processed " << iov.iov_len
                  << " bytes.";
    body_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        body_.size() > static_cast<uint64_t>(content_length_)) {
      QUIC_DVLOG(1) << "Body size (" << body_.size() << ") > content length ("
                    << content_length_ << ").";
      SendErrorResponse();
      return;
    }
    MarkConsumed(iov.iov_len);
  }
  if (!sequencer()->IsClosed()) {
    sequencer()->SetUnblocked();
    return;
  }

  // If the sequencer is closed, then all the body, including the fin, has been
  // consumed.
  OnFinRead();

  if (write_side_closed() || fin_buffered()) {
    return;
  }

  SendResponse();
}

void QuicSimpleServerStream::PushResponse(
    SpdyHeaderBlock push_request_headers) {
  if (QuicUtils::IsClientInitiatedStreamId(
          session()->connection()->transport_version(), id())) {
    QUIC_BUG << "Client initiated stream shouldn't be used as promised stream.";
    return;
  }
  // Change the stream state to emulate a client request.
  request_headers_ = std::move(push_request_headers);
  content_length_ = 0;
  QUIC_DVLOG(1) << "Stream " << id()
                << " ready to receive server push response.";
  DCHECK(reading_stopped());

  // Directly send response based on the emulated request_headers_.
  SendResponse();
}

void QuicSimpleServerStream::SendResponse() {
  if (request_headers_.empty()) {
    QUIC_DVLOG(1) << "Request headers empty.";
    SendErrorResponse();
    return;
  }

  if (content_length_ > 0 &&
      static_cast<uint64_t>(content_length_) != body_.size()) {
    QUIC_DVLOG(1) << "Content length (" << content_length_ << ") != body size ("
                  << body_.size() << ").";
    SendErrorResponse();
    return;
  }

  if (!QuicContainsKey(request_headers_, ":authority") ||
      !QuicContainsKey(request_headers_, ":path")) {
    QUIC_DVLOG(1) << "Request headers do not contain :authority or :path.";
    SendErrorResponse();
    return;
  }

  // Fetch the response from the backend interface and wait for callback once
  // response is ready
  quic_simple_server_backend_->FetchResponseFromBackend(request_headers_, body_,
                                                        this);
}

QuicConnectionId QuicSimpleServerStream::connection_id() const {
  return spdy_session()->connection_id();
}

QuicStreamId QuicSimpleServerStream::stream_id() const {
  return id();
}

QuicString QuicSimpleServerStream::peer_host() const {
  return spdy_session()->peer_address().host().ToString();
}

void QuicSimpleServerStream::OnResponseBackendComplete(
    const QuicBackendResponse* response,
    std::list<QuicBackendResponse::ServerPushInfo> resources) {
  if (response == nullptr) {
    QUIC_DVLOG(1) << "Response not found in cache.";
    SendNotFoundResponse();
    return;
  }

  if (response->response_type() == QuicBackendResponse::CLOSE_CONNECTION) {
    QUIC_DVLOG(1) << "Special response: closing connection.";
    CloseConnectionWithDetails(QUIC_NO_ERROR, "Toy server forcing close");
    return;
  }

  if (response->response_type() == QuicBackendResponse::IGNORE_REQUEST) {
    QUIC_DVLOG(1) << "Special response: ignoring request.";
    return;
  }

  if (response->response_type() == QuicBackendResponse::BACKEND_ERR_RESPONSE) {
    QUIC_DVLOG(1) << "Quic Proxy: Backend connection error.";
    /*502 Bad Gateway
      The server was acting as a gateway or proxy and received an
      invalid response from the upstream server.*/
    SendErrorResponse(502);
    return;
  }

  // Examing response status, if it was not pure integer as typical h2
  // response status, send error response. Notice that
  // QuicHttpResponseCache push urls are strictly authority + path only,
  // scheme is not included (see |QuicHttpResponseCache::GetKey()|).
  QuicString request_url = request_headers_[":authority"].as_string() +
                           request_headers_[":path"].as_string();
  int response_code;
  const SpdyHeaderBlock& response_headers = response->headers();
  if (!ParseHeaderStatusCode(response_headers, &response_code)) {
    auto status = response_headers.find(":status");
    if (status == response_headers.end()) {
      QUIC_LOG(WARNING)
          << ":status not present in response from cache for request "
          << request_url;
    } else {
      QUIC_LOG(WARNING) << "Illegal (non-integer) response :status from cache: "
                        << status->second << " for request " << request_url;
    }
    SendErrorResponse();
    return;
  }

  if (QuicUtils::IsServerInitiatedStreamId(
          session()->connection()->transport_version(), id())) {
    // A server initiated stream is only used for a server push response,
    // and only 200 and 30X response codes are supported for server push.
    // This behavior mirrors the HTTP/2 implementation.
    bool is_redirection = response_code / 100 == 3;
    if (response_code != 200 && !is_redirection) {
      QUIC_LOG(WARNING) << "Response to server push request " << request_url
                        << " result in response code " << response_code;
      Reset(QUIC_STREAM_CANCELLED);
      return;
    }
  }

  if (!resources.empty()) {
    QUIC_DVLOG(1) << "Stream " << id() << " found " << resources.size()
                  << " push resources.";
    QuicSimpleServerSession* session =
        static_cast<QuicSimpleServerSession*>(spdy_session());
    session->PromisePushResources(request_url, resources, id(),
                                  request_headers_);
  }

  if (response->response_type() == QuicBackendResponse::INCOMPLETE_RESPONSE) {
    QUIC_DVLOG(1)
        << "Stream " << id()
        << " sending an incomplete response, i.e. no trailer, no fin.";
    SendIncompleteResponse(response->headers().Clone(), response->body());
    return;
  }

  QUIC_DVLOG(1) << "Stream " << id() << " sending response.";
  SendHeadersAndBodyAndTrailers(response->headers().Clone(), response->body(),
                                response->trailers().Clone());
}

void QuicSimpleServerStream::SendNotFoundResponse() {
  QUIC_DVLOG(1) << "Stream " << id() << " sending not found response.";
  SpdyHeaderBlock headers;
  headers[":status"] = "404";
  headers["content-length"] =
      QuicTextUtils::Uint64ToString(strlen(kNotFoundResponseBody));
  SendHeadersAndBody(std::move(headers), kNotFoundResponseBody);
}

void QuicSimpleServerStream::SendErrorResponse() {
  SendErrorResponse(0);
}

void QuicSimpleServerStream::SendErrorResponse(int resp_code) {
  QUIC_DVLOG(1) << "Stream " << id() << " sending error response.";
  SpdyHeaderBlock headers;
  if (resp_code <= 0) {
    headers[":status"] = "500";
  } else {
    headers[":status"] = QuicTextUtils::Uint64ToString(resp_code);
  }
  headers["content-length"] =
      QuicTextUtils::Uint64ToString(strlen(kErrorResponseBody));
  SendHeadersAndBody(std::move(headers), kErrorResponseBody);
}

void QuicSimpleServerStream::SendIncompleteResponse(
    SpdyHeaderBlock response_headers,
    QuicStringPiece body) {
  QUIC_DLOG(INFO) << "Stream " << id() << " writing headers (fin = false) : "
                  << response_headers.DebugString();
  WriteHeaders(std::move(response_headers), /*fin=*/false, nullptr);

  QUIC_DLOG(INFO) << "Stream " << id()
                  << " writing body (fin = false) with size: " << body.size();
  if (!body.empty()) {
    WriteOrBufferData(body, /*fin=*/false, nullptr);
  }
}

void QuicSimpleServerStream::SendHeadersAndBody(
    SpdyHeaderBlock response_headers,
    QuicStringPiece body) {
  SendHeadersAndBodyAndTrailers(std::move(response_headers), body,
                                SpdyHeaderBlock());
}

void QuicSimpleServerStream::SendHeadersAndBodyAndTrailers(
    SpdyHeaderBlock response_headers,
    QuicStringPiece body,
    SpdyHeaderBlock response_trailers) {
  // Send the headers, with a FIN if there's nothing else to send.
  bool send_fin = (body.empty() && response_trailers.empty());
  QUIC_DLOG(INFO) << "Stream " << id() << " writing headers (fin = " << send_fin
                  << ") : " << response_headers.DebugString();
  WriteHeaders(std::move(response_headers), send_fin, nullptr);
  if (send_fin) {
    // Nothing else to send.
    return;
  }

  // Send the body, with a FIN if there's no trailers to send.
  send_fin = response_trailers.empty();
  QUIC_DLOG(INFO) << "Stream " << id() << " writing body (fin = " << send_fin
                  << ") with size: " << body.size();
  if (!body.empty() || send_fin) {
    WriteOrBufferData(body, send_fin, nullptr);
  }
  if (send_fin) {
    // Nothing else to send.
    return;
  }

  // Send the trailers. A FIN is always sent with trailers.
  QUIC_DLOG(INFO) << "Stream " << id() << " writing trailers (fin = true): "
                  << response_trailers.DebugString();
  WriteTrailers(std::move(response_trailers), nullptr);
}

const char* const QuicSimpleServerStream::kErrorResponseBody = "bad";
const char* const QuicSimpleServerStream::kNotFoundResponseBody =
    "file not found";

}  // namespace quic
