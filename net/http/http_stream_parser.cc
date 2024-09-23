// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_log_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace net {

namespace {

const uint64_t kMaxMergedHeaderAndBodySize = 1400;
const size_t kRequestBodyBufferSize = 1 << 14;  // 16KB

std::string GetResponseHeaderLines(const HttpResponseHeaders& headers) {
  std::string raw_headers = headers.raw_headers();
  std::string cr_separated_headers;
  base::StringTokenizer tokenizer(raw_headers, std::string(1, '\0'));
  while (tokenizer.GetNext()) {
    base::StrAppend(&cr_separated_headers, {tokenizer.token_piece(), "\n"});
  }
  return cr_separated_headers;
}

base::Value::Dict NetLogSendRequestBodyParams(uint64_t length,
                                              bool is_chunked,
                                              bool did_merge) {
  base::Value::Dict dict;
  dict.Set("length", static_cast<int>(length));
  dict.Set("is_chunked", is_chunked);
  dict.Set("did_merge", did_merge);
  return dict;
}

void NetLogSendRequestBody(const NetLogWithSource& net_log,
                           uint64_t length,
                           bool is_chunked,
                           bool did_merge) {
  net_log.AddEvent(NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST_BODY, [&] {
    return NetLogSendRequestBodyParams(length, is_chunked, did_merge);
  });
}

// Returns true if |error_code| is an error for which we give the server a
// chance to send a body containing error information, if the error was received
// while trying to upload a request body.
bool ShouldTryReadingOnUploadError(int error_code) {
  return (error_code == ERR_CONNECTION_RESET);
}

}  // namespace

// Similar to DrainableIOBuffer(), but this version comes with its own
// storage. The motivation is to avoid repeated allocations of
// DrainableIOBuffer.
//
// Example:
//
// scoped_refptr<SeekableIOBuffer> buf =
//     base::MakeRefCounted<SeekableIOBuffer>(1024);
// // capacity() == 1024. size() == BytesRemaining() == BytesConsumed() == 0.
// // data() points to the beginning of the buffer.
//
// // Read() takes an IOBuffer.
// int bytes_read = some_reader->Read(buf, buf->capacity());
// buf->DidAppend(bytes_read);
// // size() == BytesRemaining() == bytes_read. data() is unaffected.
//
// while (buf->BytesRemaining() > 0) {
//   // Write() takes an IOBuffer. If it takes const char*, we could
///  // simply use the regular IOBuffer like buf->data() + offset.
//   int bytes_written = Write(buf, buf->BytesRemaining());
//   buf->DidConsume(bytes_written);
// }
// // BytesRemaining() == 0. BytesConsumed() == size().
// // data() points to the end of the consumed bytes (exclusive).
//
// // If you want to reuse the buffer, be sure to clear the buffer.
// buf->Clear();
// // size() == BytesRemaining() == BytesConsumed() == 0.
// // data() points to the beginning of the buffer.
//
class HttpStreamParser::SeekableIOBuffer : public IOBufferWithSize {
 public:
  explicit SeekableIOBuffer(int capacity)
      : IOBufferWithSize(capacity), real_data_(data_), capacity_(capacity) {}

  // DidConsume() changes the |data_| pointer so that |data_| always points
  // to the first unconsumed byte.
  void DidConsume(int bytes) {
    SetOffset(used_ + bytes);
  }

  // Returns the number of unconsumed bytes.
  int BytesRemaining() const {
    return size_ - used_;
  }

  // Seeks to an arbitrary point in the buffer. The notion of bytes consumed
  // and remaining are updated appropriately.
  void SetOffset(int bytes) {
    DCHECK_GE(bytes, 0);
    DCHECK_LE(bytes, size_);
    used_ = bytes;
    data_ = real_data_ + used_;
  }

  // Called after data is added to the buffer. Adds |bytes| added to
  // |size_|. data() is unaffected.
  void DidAppend(int bytes) {
    DCHECK_GE(bytes, 0);
    DCHECK_GE(size_ + bytes, 0);
    DCHECK_LE(size_ + bytes, capacity_);
    size_ += bytes;
  }

  // Changes the logical size to 0, and the offset to 0.
  void Clear() {
    size_ = 0;
    SetOffset(0);
  }

  // Returns the logical size of the buffer (i.e the number of bytes of data
  // in the buffer).
  int size() const { return size_; }

  // Returns the capacity of the buffer. The capacity is the size used when
  // the object is created.
  int capacity() const { return capacity_; }

 private:
  ~SeekableIOBuffer() override {
    // data_ will be deleted in IOBuffer::~IOBuffer().
    data_ = real_data_;
  }

  raw_ptr<char, AllowPtrArithmetic> real_data_;
  const int capacity_;
  int size_ = 0;
  int used_ = 0;
};

// 2 CRLFs + max of 8 hex chars.
const size_t HttpStreamParser::kChunkHeaderFooterSize = 12;

HttpStreamParser::HttpStreamParser(StreamSocket* stream_socket,
                                   bool connection_is_reused,
                                   const GURL& url,
                                   const std::string& method,
                                   UploadDataStream* upload_data_stream,
                                   GrowableIOBuffer* read_buffer,
                                   const NetLogWithSource& net_log)
    : url_(url),
      method_(method),
      upload_data_stream_(upload_data_stream),
      read_buf_(read_buffer),
      response_header_start_offset_(std::string::npos),
      stream_socket_(stream_socket),
      connection_is_reused_(connection_is_reused),
      net_log_(net_log),
      truncate_to_content_length_enabled_(base::FeatureList::IsEnabled(
          features::kTruncateBodyToContentLength)) {
  io_callback_ = base::BindRepeating(&HttpStreamParser::OnIOComplete,
                                     weak_ptr_factory_.GetWeakPtr());
}

HttpStreamParser::~HttpStreamParser() = default;

int HttpStreamParser::SendRequest(
    const std::string& request_line,
    const HttpRequestHeaders& headers,
    const NetworkTrafficAnnotationTag& traffic_annotation,
    HttpResponseInfo* response,
    CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, io_state_);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  DCHECK(response);

  NetLogRequestHeaders(net_log_,
                       NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
                       request_line, &headers);

  DVLOG(1) << __func__ << "() request_line = \"" << request_line << "\""
           << " headers = \"" << headers.ToString() << "\"";
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(traffic_annotation);
  response_ = response;

  // Put the peer's IP address and port into the response.
  IPEndPoint ip_endpoint;
  int result = stream_socket_->GetPeerAddress(&ip_endpoint);
  if (result != OK)
    return result;
  response_->remote_endpoint = ip_endpoint;

  std::string request = request_line + headers.ToString();
  request_headers_length_ = request.size();

  if (upload_data_stream_) {
    request_body_send_buf_ =
        base::MakeRefCounted<SeekableIOBuffer>(kRequestBodyBufferSize);
    if (upload_data_stream_->is_chunked()) {
      // Read buffer is adjusted to guarantee that |request_body_send_buf_| is
      // large enough to hold the encoded chunk.
      request_body_read_buf_ = base::MakeRefCounted<SeekableIOBuffer>(
          kRequestBodyBufferSize - kChunkHeaderFooterSize);
    } else {
      // No need to encode request body, just send the raw data.
      request_body_read_buf_ = request_body_send_buf_;
    }
  }

  io_state_ = STATE_SEND_HEADERS;

  // If we have a small request body, then we'll merge with the headers into a
  // single write.
  bool did_merge = false;
  if (ShouldMergeRequestHeadersAndBody(request, upload_data_stream_)) {
    int merged_size =
        static_cast<int>(request_headers_length_ + upload_data_stream_->size());
    auto merged_request_headers_and_body =
        base::MakeRefCounted<IOBufferWithSize>(merged_size);
    // We'll repurpose |request_headers_| to store the merged headers and
    // body.
    request_headers_ = base::MakeRefCounted<DrainableIOBuffer>(
        merged_request_headers_and_body, merged_size);

    memcpy(request_headers_->data(), request.data(), request_headers_length_);
    request_headers_->DidConsume(request_headers_length_);

    uint64_t todo = upload_data_stream_->size();
    while (todo) {
      int consumed = upload_data_stream_->Read(request_headers_.get(),
                                               static_cast<int>(todo),
                                               CompletionOnceCallback());
      // Read() must succeed synchronously if not chunked and in memory.
      DCHECK_GT(consumed, 0);
      request_headers_->DidConsume(consumed);
      todo -= consumed;
    }
    DCHECK(upload_data_stream_->IsEOF());
    // Reset the offset, so the buffer can be read from the beginning.
    request_headers_->SetOffset(0);
    did_merge = true;

    NetLogSendRequestBody(net_log_, upload_data_stream_->size(),
                          false, /* not chunked */
                          true /* merged */);
  }

  if (!did_merge) {
    // If we didn't merge the body with the headers, then |request_headers_|
    // contains just the HTTP headers.
    size_t request_size = request.size();
    scoped_refptr<StringIOBuffer> headers_io_buf =
        base::MakeRefCounted<StringIOBuffer>(std::move(request));
    request_headers_ = base::MakeRefCounted<DrainableIOBuffer>(
        std::move(headers_io_buf), request_size);
  }

  result = DoLoop(OK);
  if (result == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return result > 0 ? OK : result;
}

int HttpStreamParser::ConfirmHandshake(CompletionOnceCallback callback) {
  int ret = stream_socket_->ConfirmHandshake(
      base::BindOnce(&HttpStreamParser::RunConfirmHandshakeCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  if (ret == ERR_IO_PENDING)
    confirm_handshake_callback_ = std::move(callback);
  return ret;
}

int HttpStreamParser::ReadResponseHeaders(CompletionOnceCallback callback) {
  DCHECK(io_state_ == STATE_NONE || io_state_ == STATE_DONE);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  DCHECK_EQ(0, read_buf_unused_offset_);
  DCHECK(SendRequestBuffersEmpty());

  // This function can be called with io_state_ == STATE_DONE if the
  // connection is closed after seeing just a 1xx response code.
  if (io_state_ == STATE_DONE)
    return ERR_CONNECTION_CLOSED;

  int result = OK;
  io_state_ = STATE_READ_HEADERS;

  if (read_buf_->offset() > 0) {
    // Simulate the state where the data was just read from the socket.
    result = read_buf_->offset();
    read_buf_->set_offset(0);
  }
  if (result > 0)
    io_state_ = STATE_READ_HEADERS_COMPLETE;

  result = DoLoop(result);
  if (result == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return result > 0 ? OK : result;
}

int HttpStreamParser::ReadResponseBody(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  DCHECK(io_state_ == STATE_NONE || io_state_ == STATE_DONE);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  DCHECK_LE(buf_len, kMaxBufSize);
  DCHECK(SendRequestBuffersEmpty());
  // Added to investigate crbug.com/499663.
  CHECK(buf);

  if (io_state_ == STATE_DONE)
    return OK;

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;
  io_state_ = STATE_READ_BODY;

  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return result;
}

void HttpStreamParser::OnIOComplete(int result) {
  result = DoLoop(result);

  // The client callback can do anything, including destroying this class,
  // so any pending callback must be issued after everything else is done.
  if (result != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(result);
  }
}

int HttpStreamParser::DoLoop(int result) {
  do {
    DCHECK_NE(ERR_IO_PENDING, result);
    DCHECK_NE(STATE_DONE, io_state_);
    DCHECK_NE(STATE_NONE, io_state_);
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_SEND_HEADERS:
        DCHECK_EQ(OK, result);
        result = DoSendHeaders();
        DCHECK_NE(STATE_NONE, io_state_);
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        result = DoSendHeadersComplete(result);
        DCHECK_NE(STATE_NONE, io_state_);
        break;
      case STATE_SEND_BODY:
        DCHECK_EQ(OK, result);
        result = DoSendBody();
        DCHECK_NE(STATE_NONE, io_state_);
        break;
      case STATE_SEND_BODY_COMPLETE:
        result = DoSendBodyComplete(result);
        DCHECK_NE(STATE_NONE, io_state_);
        break;
      case STATE_SEND_REQUEST_READ_BODY_COMPLETE:
        result = DoSendRequestReadBodyComplete(result);
        DCHECK_NE(STATE_NONE, io_state_);
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        result = DoSendRequestComplete(result);
        break;
      case STATE_READ_HEADERS:
        net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_PARSER_READ_HEADERS);
        DCHECK_GE(result, 0);
        result = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        result = DoReadHeadersComplete(result);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_STREAM_PARSER_READ_HEADERS, result);
        break;
      case STATE_READ_BODY:
        DCHECK_GE(result, 0);
        result = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        result = DoReadBodyComplete(result);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } while (result != ERR_IO_PENDING &&
           (io_state_ != STATE_DONE && io_state_ != STATE_NONE));

  return result;
}

int HttpStreamParser::DoSendHeaders() {
  int bytes_remaining = request_headers_->BytesRemaining();
  DCHECK_GT(bytes_remaining, 0);

  // Record our best estimate of the 'request time' as the time when we send
  // out the first bytes of the request headers.
  if (bytes_remaining == request_headers_->size())
    response_->request_time = base::Time::Now();

  io_state_ = STATE_SEND_HEADERS_COMPLETE;
  return stream_socket_->Write(
      request_headers_.get(), bytes_remaining, io_callback_,
      NetworkTrafficAnnotationTag(traffic_annotation_));
}

int HttpStreamParser::DoSendHeadersComplete(int result) {
  if (result < 0) {
    // In the unlikely case that the headers and body were merged, all the
    // the headers were sent, but not all of the body way, and |result| is
    // an error that this should try reading after, stash the error for now and
    // act like the request was successfully sent.
    io_state_ = STATE_SEND_REQUEST_COMPLETE;
    if (request_headers_->BytesConsumed() >= request_headers_length_ &&
        ShouldTryReadingOnUploadError(result)) {
      upload_error_ = result;
      return OK;
    }
    return result;
  }

  sent_bytes_ += result;
  request_headers_->DidConsume(result);
  if (request_headers_->BytesRemaining() > 0) {
    io_state_ = STATE_SEND_HEADERS;
    return OK;
  }

  if (upload_data_stream_ &&
      (upload_data_stream_->is_chunked() ||
       // !IsEOF() indicates that the body wasn't merged.
       (upload_data_stream_->size() > 0 && !upload_data_stream_->IsEOF()))) {
    NetLogSendRequestBody(net_log_, upload_data_stream_->size(),
                          upload_data_stream_->is_chunked(),
                          false /* not merged */);
    io_state_ = STATE_SEND_BODY;
    return OK;
  }

  // Finished sending the request.
  io_state_ = STATE_SEND_REQUEST_COMPLETE;
  return OK;
}

int HttpStreamParser::DoSendBody() {
  if (request_body_send_buf_->BytesRemaining() > 0) {
    io_state_ = STATE_SEND_BODY_COMPLETE;
    return stream_socket_->Write(
        request_body_send_buf_.get(), request_body_send_buf_->BytesRemaining(),
        io_callback_, NetworkTrafficAnnotationTag(traffic_annotation_));
  }

  if (upload_data_stream_->is_chunked() && sent_last_chunk_) {
    // Finished sending the request.
    io_state_ = STATE_SEND_REQUEST_COMPLETE;
    return OK;
  }

  request_body_read_buf_->Clear();
  io_state_ = STATE_SEND_REQUEST_READ_BODY_COMPLETE;
  return upload_data_stream_->Read(
      request_body_read_buf_.get(), request_body_read_buf_->capacity(),
      base::BindOnce(&HttpStreamParser::OnIOComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

int HttpStreamParser::DoSendBodyComplete(int result) {
  if (result < 0) {
    // If |result| is an error that this should try reading after, stash the
    // error for now and act like the request was successfully sent.
    io_state_ = STATE_SEND_REQUEST_COMPLETE;
    if (ShouldTryReadingOnUploadError(result)) {
      upload_error_ = result;
      return OK;
    }
    return result;
  }

  sent_bytes_ += result;
  request_body_send_buf_->DidConsume(result);

  io_state_ = STATE_SEND_BODY;
  return OK;
}

int HttpStreamParser::DoSendRequestReadBodyComplete(int result) {
  // |result| is the result of read from the request body from the last call to
  // DoSendBody().
  if (result < 0) {
    io_state_ = STATE_SEND_REQUEST_COMPLETE;
    return result;
  }

  // Chunked data needs to be encoded.
  if (upload_data_stream_->is_chunked()) {
    if (result == 0) {  // Reached the end.
      DCHECK(upload_data_stream_->IsEOF());
      sent_last_chunk_ = true;
    }
    // Encode the buffer as 1 chunk.
    const std::string_view payload(request_body_read_buf_->data(), result);
    request_body_send_buf_->Clear();
    result = EncodeChunk(payload, request_body_send_buf_->span());
  }

  if (result == 0) {  // Reached the end.
    // Reaching EOF means we can finish sending request body unless the data is
    // chunked. (i.e. No need to send the terminal chunk.)
    DCHECK(upload_data_stream_->IsEOF());
    DCHECK(!upload_data_stream_->is_chunked());
    // Finished sending the request.
    io_state_ = STATE_SEND_REQUEST_COMPLETE;
  } else if (result > 0) {
    request_body_send_buf_->DidAppend(result);
    result = 0;
    io_state_ = STATE_SEND_BODY;
  }
  return result;
}

int HttpStreamParser::DoSendRequestComplete(int result) {
  DCHECK_NE(result, ERR_IO_PENDING);
  request_headers_ = nullptr;
  upload_data_stream_ = nullptr;
  request_body_send_buf_ = nullptr;
  request_body_read_buf_ = nullptr;

  return result;
}

int HttpStreamParser::DoReadHeaders() {
  io_state_ = STATE_READ_HEADERS_COMPLETE;

  // Grow the read buffer if necessary.
  if (read_buf_->RemainingCapacity() == 0)
    read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);

  // http://crbug.com/16371: We're seeing |user_buf_->data()| return NULL.
  // See if the user is passing in an IOBuffer with a NULL |data_|.
  CHECK(read_buf_->data());

  return stream_socket_->Read(read_buf_.get(), read_buf_->RemainingCapacity(),
                              io_callback_);
}

int HttpStreamParser::DoReadHeadersComplete(int result) {
  // DoReadHeadersComplete is called with the result of Socket::Read, which is a
  // (byte_count | error), and returns (error | OK).

  result = HandleReadHeaderResult(result);

  // If still reading the headers, just return the result.
  if (io_state_ == STATE_READ_HEADERS) {
    return result;
  }

  // If the result is ERR_IO_PENDING, |io_state_| should be STATE_READ_HEADERS.
  DCHECK_NE(ERR_IO_PENDING, result);

  // TODO(mmenke):  The code below is ugly and hacky.  A much better and more
  // flexible long term solution would be to separate out the read and write
  // loops, though this would involve significant changes, both here and
  // elsewhere (WebSockets, for instance).

  // If there was an error uploading the request body, may need to adjust the
  // result.
  if (upload_error_ != OK) {
    // On errors, use the original error received when sending the request.
    // The main cases where these are different is when there's a header-related
    // error code, or when there's an ERR_CONNECTION_CLOSED, which can result in
    // special handling of partial responses and HTTP/0.9 responses.
    if (result < 0) {
      // Nothing else to do.  In the HTTP/0.9 or only partial headers received
      // cases, can normally go to other states after an error reading headers.
      io_state_ = STATE_DONE;
      // Don't let caller see the headers.
      response_->headers = nullptr;
      result = upload_error_;
    } else {
      // Skip over 1xx responses as usual, and allow 4xx/5xx error responses to
      // override the error received while uploading the body. For other status
      // codes, return the original error received when trying to upload the
      // request body, to make sure the consumer has some indication there was
      // an error.
      int response_code_class = response_->headers->response_code() / 100;
      if (response_code_class != 1 && response_code_class != 4 &&
          response_code_class != 5) {
        // Nothing else to do.
        io_state_ = STATE_DONE;
        // Don't let caller see the headers.
        response_->headers = nullptr;
        result = upload_error_;
      }
    }
  }

  // If there will be no more header reads, clear the request and response
  // pointers, as they're no longer needed, and in some cases the body may
  // be read after the parent class destroyed the underlying objects (See
  // HttpResponseBodyDrainer).
  //
  // This is the last header read if HttpStreamParser is done, no response
  // headers were received, or if the response code is not in the 1xx range.
  if (io_state_ == STATE_DONE || !response_->headers ||
      response_->headers->response_code() / 100 != 1) {
    response_ = nullptr;
  }

  return result;
}

int HttpStreamParser::DoReadBody() {
  io_state_ = STATE_READ_BODY_COMPLETE;

  // Added to investigate crbug.com/499663.
  CHECK(user_read_buf_.get());

  // There may be additional data after the end of the body waiting in
  // the socket, but in order to find out, we need to read as much as possible.
  // If there is additional data, discard it and close the connection later.
  int64_t remaining_read_len = user_read_buf_len_;
  int64_t remaining_body = 0;
  if (truncate_to_content_length_enabled_ && !chunked_decoder_.get() &&
      response_body_length_ >= 0) {
    remaining_body = response_body_length_ - response_body_read_;
    remaining_read_len = std::min(remaining_read_len, remaining_body);
  }

  // There may be some data left over from reading the response headers.
  if (read_buf_->offset()) {
    int64_t available = read_buf_->offset() - read_buf_unused_offset_;
    if (available) {
      CHECK_GT(available, 0);
      int64_t bytes_from_buffer = std::min(available, remaining_read_len);
      user_read_buf_->span().copy_prefix_from(read_buf_->everything().subspan(
          read_buf_unused_offset_, bytes_from_buffer));
      read_buf_unused_offset_ += bytes_from_buffer;
      // Clear out the remaining data if we've reached the end of the body.
      if (truncate_to_content_length_enabled_ &&
          (remaining_body == bytes_from_buffer) &&
          (available > bytes_from_buffer)) {
        read_buf_->SetCapacity(0);
        read_buf_unused_offset_ = 0;
        discarded_extra_data_ = true;
      } else if (bytes_from_buffer == available) {
        read_buf_->SetCapacity(0);
        read_buf_unused_offset_ = 0;
      }
      return bytes_from_buffer;
    } else {
      read_buf_->SetCapacity(0);
      read_buf_unused_offset_ = 0;
    }
  }

  // Check to see if we're done reading.
  if (IsResponseBodyComplete())
    return 0;

  // DoReadBodyComplete will truncate the amount read if necessary whether the
  // read completes synchronously or asynchronously.
  DCHECK_EQ(0, read_buf_->offset());
  return stream_socket_->Read(user_read_buf_.get(), user_read_buf_len_,
                              io_callback_);
}

int HttpStreamParser::DoReadBodyComplete(int result) {
  // Check to see if we've read too much and need to discard data before we
  // increment received_bytes_ and response_body_read_ or otherwise start
  // processing the data.
  if (truncate_to_content_length_enabled_ && !chunked_decoder_.get() &&
      response_body_length_ >= 0) {
    // Calculate how much we should have been allowed to read to not go beyond
    // the Content-Length.
    int64_t remaining_body = response_body_length_ - response_body_read_;
    int64_t remaining_read_len =
        std::min(static_cast<int64_t>(user_read_buf_len_), remaining_body);
    if (result > remaining_read_len) {
      // Truncate to only what is in the body.
      result = remaining_read_len;
      discarded_extra_data_ = true;
    }
  }

  // When the connection is closed, there are numerous ways to interpret it.
  //
  //  - If a Content-Length header is present and the body contains exactly that
  //    number of bytes at connection close, the response is successful.
  //
  //  - If a Content-Length header is present and the body contains fewer bytes
  //    than promised by the header at connection close, it may indicate that
  //    the connection was closed prematurely, or it may indicate that the
  //    server sent an invalid Content-Length header. Unfortunately, the invalid
  //    Content-Length header case does occur in practice and other browsers are
  //    tolerant of it. We choose to treat it as an error for now, but the
  //    download system treats it as a non-error, and URLRequestHttpJob also
  //    treats it as OK if the Content-Length is the post-decoded body content
  //    length.
  //
  //  - If chunked encoding is used and the terminating chunk has been processed
  //    when the connection is closed, the response is successful.
  //
  //  - If chunked encoding is used and the terminating chunk has not been
  //    processed when the connection is closed, it may indicate that the
  //    connection was closed prematurely or it may indicate that the server
  //    sent an invalid chunked encoding. We choose to treat it as
  //    an invalid chunked encoding.
  //
  //  - If a Content-Length is not present and chunked encoding is not used,
  //    connection close is the only way to signal that the response is
  //    complete. Unfortunately, this also means that there is no way to detect
  //    early close of a connection. No error is returned.
  if (result == 0 && !IsResponseBodyComplete() && CanFindEndOfResponse()) {
    if (chunked_decoder_.get())
      result = ERR_INCOMPLETE_CHUNKED_ENCODING;
    else
      result = ERR_CONTENT_LENGTH_MISMATCH;
  }

  if (result > 0)
    received_bytes_ += result;

  // Filter incoming data if appropriate.  FilterBuf may return an error.
  if (result > 0 && chunked_decoder_.get()) {
    result = chunked_decoder_->FilterBuf(user_read_buf_->data(), result);
    if (result == 0 && !chunked_decoder_->reached_eof()) {
      // Don't signal completion of the Read call yet or else it'll look like
      // we received end-of-file.  Wait for more data.
      io_state_ = STATE_READ_BODY;
      return OK;
    }
  }

  if (result > 0)
    response_body_read_ += result;

  if (result <= 0 || IsResponseBodyComplete()) {
    io_state_ = STATE_DONE;

    // Save the overflow data, which can be in two places.  There may be
    // some left over in |user_read_buf_|, plus there may be more
    // in |read_buf_|.  But the part left over in |user_read_buf_| must have
    // come from the |read_buf_|, so there's room to put it back at the
    // start first.
    int additional_save_amount = read_buf_->offset() - read_buf_unused_offset_;
    int save_amount = 0;
    if (chunked_decoder_.get()) {
      save_amount = chunked_decoder_->bytes_after_eof();
    } else if (response_body_length_ >= 0) {
      int64_t extra_data_read = response_body_read_ - response_body_length_;
      if (extra_data_read > 0) {
        save_amount = static_cast<int>(extra_data_read);
        if (result > 0)
          result -= save_amount;
      }
    }

    CHECK_LE(save_amount + additional_save_amount, kMaxBufSize);
    if (read_buf_->capacity() < save_amount + additional_save_amount) {
      read_buf_->SetCapacity(save_amount + additional_save_amount);
    }

    if (save_amount) {
      received_bytes_ -= save_amount;
      read_buf_->everything().copy_prefix_from(
          user_read_buf_->span().subspan(result, save_amount));
    }
    read_buf_->set_offset(save_amount);
    if (additional_save_amount) {
      read_buf_->span().copy_prefix_from(read_buf_->everything().subspan(
          read_buf_unused_offset_, additional_save_amount));
      read_buf_->set_offset(save_amount + additional_save_amount);
    }
    read_buf_unused_offset_ = 0;
  } else {
    // Now waiting for more of the body to be read.
    user_read_buf_ = nullptr;
    user_read_buf_len_ = 0;
  }

  return result;
}

int HttpStreamParser::HandleReadHeaderResult(int result) {
  DCHECK_EQ(0, read_buf_unused_offset_);

  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result == ERR_CONNECTION_CLOSED) {
    // The connection closed without getting any more data.
    if (read_buf_->offset() == 0) {
      io_state_ = STATE_DONE;
      // If the connection has not been reused, it may have been a 0-length
      // HTTP/0.9 responses, but it was most likely an error, so just return
      // ERR_EMPTY_RESPONSE instead. If the connection was reused, just pass
      // on the original connection close error, as rather than being an
      // empty HTTP/0.9 response it's much more likely the server closed the
      // socket before it received the request.
      if (!connection_is_reused_)
        return ERR_EMPTY_RESPONSE;
      return result;
    }

    // Accepting truncated headers over HTTPS is a potential security
    // vulnerability, so just return an error in that case.
    //
    // If response_header_start_offset_ is std::string::npos, this may be a < 8
    // byte HTTP/0.9 response. However, accepting such a response over HTTPS
    // would allow a MITM to truncate an HTTP/1.x status line to look like a
    // short HTTP/0.9 response if the peer put a record boundary at the first 8
    // bytes. To ensure that all response headers received over HTTPS are
    // pristine, treat such responses as errors.
    //
    // TODO(mmenke):  Returning ERR_RESPONSE_HEADERS_TRUNCATED when a response
    // looks like an HTTP/0.9 response is weird.  Should either come up with
    // another error code, or, better, disable HTTP/0.9 over HTTPS (and give
    // that a new error code).
    if (url_.SchemeIsCryptographic()) {
      io_state_ = STATE_DONE;
      return ERR_RESPONSE_HEADERS_TRUNCATED;
    }

    // Parse things as well as we can and let the caller decide what to do.
    int end_offset;
    if (response_header_start_offset_ != std::string::npos) {
      // The response looks to be a truncated set of HTTP headers.
      io_state_ = STATE_READ_BODY_COMPLETE;
      end_offset = read_buf_->offset();
    } else {
      // The response is apparently using HTTP/0.9.  Treat the entire response
      // as the body.
      end_offset = 0;
    }
    int rv = ParseResponseHeaders(end_offset);
    if (rv < 0)
      return rv;
    return result;
  }

  if (result < 0) {
    if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
      CHECK(url_.SchemeIsCryptographic());
      response_->cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
      stream_socket_->GetSSLCertRequestInfo(response_->cert_request_info.get());
    }
    io_state_ = STATE_DONE;
    return result;
  }

  // Record our best estimate of the 'response time' as the time when we read
  // the first bytes of the response headers.
  if (read_buf_->offset() == 0) {
    response_->response_time = base::Time::Now();
    // Also keep the time as base::TimeTicks for `first_response_start_time_`
    // and `non_informational_response_start_time_`.
    current_response_start_time_ = base::TimeTicks::Now();
  }

  // For |first_response_start_time_|, use the time that we received the first
  // byte of *any* response- including 1XX, as per the resource timing spec for
  // responseStart (see note at
  // https://www.w3.org/TR/resource-timing-2/#dom-performanceresourcetiming-responsestart).
  if (first_response_start_time_.is_null())
    first_response_start_time_ = current_response_start_time_;

  read_buf_->set_offset(read_buf_->offset() + result);
  DCHECK_LE(read_buf_->offset(), read_buf_->capacity());
  DCHECK_GT(result, 0);

  int end_of_header_offset = FindAndParseResponseHeaders(result);

  // Note: -1 is special, it indicates we haven't found the end of headers.
  // Anything less than -1 is a Error, so we bail out.
  if (end_of_header_offset < -1)
    return end_of_header_offset;

  if (end_of_header_offset == -1) {
    io_state_ = STATE_READ_HEADERS;
    // Prevent growing the headers buffer indefinitely.
    if (read_buf_->offset() >= kMaxHeaderBufSize) {
      io_state_ = STATE_DONE;
      return ERR_RESPONSE_HEADERS_TOO_BIG;
    }
  } else {
    CalculateResponseBodySize();

    // Record the response start time if this response is not informational
    // (non-1xx).
    if (response_->headers->response_code() / 100 != 1) {
      DCHECK(non_informational_response_start_time_.is_null());
      non_informational_response_start_time_ = current_response_start_time_;
    }

    // If the body is zero length, the caller may not call ReadResponseBody,
    // which is where any extra data is copied to read_buf_, so we move the
    // data here.
    if (response_body_length_ == 0) {
      base::span<uint8_t> extra_bytes =
          read_buf_->span_before_offset().subspan(end_of_header_offset);
      if (!extra_bytes.empty()) {
        read_buf_->everything().copy_prefix_from(extra_bytes);
      }
      read_buf_->SetCapacity(extra_bytes.size());
      if (response_->headers->response_code() / 100 == 1) {
        // After processing a 1xx response, the caller will ask for the next
        // header, so reset state to support that. We don't completely ignore a
        // 1xx response because it cannot be returned in reply to a CONNECT
        // request so we return OK here, which lets the caller inspect the
        // response and reject it in the event that we're setting up a CONNECT
        // tunnel.
        response_header_start_offset_ = std::string::npos;
        response_body_length_ = -1;
        // Record the timing of the 103 Early Hints response for the experiment
        // (https://crbug.com/1093693).
        if (response_->headers->response_code() == HTTP_EARLY_HINTS &&
            first_early_hints_time_.is_null()) {
          first_early_hints_time_ = current_response_start_time_;
        }
        // Now waiting for the second set of headers to be read.
      } else {
        // Only set keep-alive based on final set of headers.
        response_is_keep_alive_ = response_->headers->IsKeepAlive();

        io_state_ = STATE_DONE;
      }
      return OK;
    }

    // Only set keep-alive based on final set of headers.
    response_is_keep_alive_ = response_->headers->IsKeepAlive();

    // Note where the headers stop.
    read_buf_unused_offset_ = end_of_header_offset;
    // Now waiting for the body to be read.
  }
  return OK;
}

void HttpStreamParser::RunConfirmHandshakeCallback(int rv) {
  std::move(confirm_handshake_callback_).Run(rv);
}

int HttpStreamParser::FindAndParseResponseHeaders(int new_bytes) {
  DCHECK_GT(new_bytes, 0);
  DCHECK_EQ(0, read_buf_unused_offset_);
  size_t end_offset = std::string::npos;

  // Look for the start of the status line, if it hasn't been found yet.
  if (response_header_start_offset_ == std::string::npos) {
    response_header_start_offset_ =
        HttpUtil::LocateStartOfStatusLine(read_buf_->span_before_offset());
  }

  if (response_header_start_offset_ != std::string::npos) {
    // LocateEndOfHeaders looks for two line breaks in a row (With or without
    // carriage returns). So the end of the headers includes at most the last 3
    // bytes of the buffer from the past read. This optimization avoids O(n^2)
    // performance in the case each read only returns a couple bytes. It's not
    // too important in production, but for fuzzers with memory instrumentation,
    // it's needed to avoid timing out.
    size_t lower_bound =
        (base::ClampedNumeric<size_t>(read_buf_->offset()) - new_bytes - 3)
            .RawValue();
    size_t search_start = std::max(response_header_start_offset_, lower_bound);
    end_offset = HttpUtil::LocateEndOfHeaders(read_buf_->span_before_offset(),
                                              search_start);
  } else if (read_buf_->offset() >= 8) {
    // Enough data to decide that this is an HTTP/0.9 response.
    // 8 bytes = (4 bytes of junk) + "http".length()
    end_offset = 0;
  }

  if (end_offset == std::string::npos)
    return -1;

  int rv = ParseResponseHeaders(end_offset);
  if (rv < 0)
    return rv;
  return end_offset;
}

int HttpStreamParser::ParseResponseHeaders(size_t end_offset) {
  scoped_refptr<HttpResponseHeaders> headers;
  DCHECK_EQ(0, read_buf_unused_offset_);

  if (response_header_start_offset_ != std::string::npos) {
    received_bytes_ += end_offset;
    headers = HttpResponseHeaders::TryToCreate(
        base::as_string_view(read_buf_->everything().first(end_offset)));
    if (!headers)
      return ERR_INVALID_HTTP_RESPONSE;
    has_seen_status_line_ = true;
  } else {
    // Enough data was read -- there is no status line, so this is HTTP/0.9, or
    // the server is broken / doesn't speak HTTP.

    if (has_seen_status_line_) {
      // If we saw a status line previously, the server can speak HTTP/1.x so it
      // is not reasonable to interpret the response as an HTTP/0.9 response.
      return ERR_INVALID_HTTP_RESPONSE;
    }

    std::string_view scheme = url_.scheme_piece();
    if (url::DefaultPortForScheme(scheme) != url_.EffectiveIntPort()) {
      // If the port is not the default for the scheme, assume it's not a real
      // HTTP/0.9 response, and fail the request.

      // Allow Shoutcast responses over HTTP, as it's somewhat common and relies
      // on HTTP/0.9 on weird ports to work.
      // See
      // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/qS63pYso4P0
      if (read_buf_->offset() < 3 || scheme != "http" ||
          !base::EqualsCaseInsensitiveASCII(
              base::as_string_view(read_buf_->everything().first(3u)), "icy")) {
        return ERR_INVALID_HTTP_RESPONSE;
      }
    }

    headers = base::MakeRefCounted<HttpResponseHeaders>(
        std::string("HTTP/0.9 200 OK"));
  }

  // Check for multiple Content-Length headers when the response is not
  // chunked-encoded.  If they exist, and have distinct values, it's a potential
  // response smuggling attack.
  if (!headers->IsChunkEncoded()) {
    if (HttpUtil::HeadersContainMultipleCopiesOfField(*headers,
                                                      "Content-Length"))
      return ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH;
  }

  // Check for multiple Content-Disposition or Location headers.  If they exist,
  // it's also a potential response smuggling attack.
  if (HttpUtil::HeadersContainMultipleCopiesOfField(*headers,
                                                    "Content-Disposition"))
    return ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION;
  if (HttpUtil::HeadersContainMultipleCopiesOfField(*headers, "Location"))
    return ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION;

  response_->headers = headers;
  if (headers->GetHttpVersion() == HttpVersion(0, 9)) {
    response_->connection_info = HttpConnectionInfo::kHTTP0_9;
  } else if (headers->GetHttpVersion() == HttpVersion(1, 0)) {
    response_->connection_info = HttpConnectionInfo::kHTTP1_0;
  } else if (headers->GetHttpVersion() == HttpVersion(1, 1)) {
    response_->connection_info = HttpConnectionInfo::kHTTP1_1;
  }
  DVLOG(1) << __func__ << "() content_length = \""
           << response_->headers->GetContentLength() << "\n\""
           << " headers = \"" << GetResponseHeaderLines(*response_->headers)
           << "\"";
  return OK;
}

void HttpStreamParser::CalculateResponseBodySize() {
  // Figure how to determine EOF:

  // For certain responses, we know the content length is always 0. From
  // RFC 7230 Section 3.3 Message Body:
  //
  // The presence of a message body in a response depends on both the
  // request method to which it is responding and the response status code
  // (Section 3.1.2).  Responses to the HEAD request method (Section 4.3.2
  // of [RFC7231]) never include a message body because the associated
  // response header fields (e.g., Transfer-Encoding, Content-Length,
  // etc.), if present, indicate only what their values would have been if
  // the request method had been GET (Section 4.3.1 of [RFC7231]). 2xx
  // (Successful) responses to a CONNECT request method (Section 4.3.6 of
  // [RFC7231]) switch to tunnel mode instead of having a message body.
  // All 1xx (Informational), 204 (No Content), and 304 (Not Modified)
  // responses do not include a message body.  All other responses do
  // include a message body, although the body might be of zero length.
  //
  // From RFC 7231 Section 6.3.6 205 Reset Content:
  //
  // Since the 205 status code implies that no additional content will be
  // provided, a server MUST NOT generate a payload in a 205 response.
  if (response_->headers->response_code() / 100 == 1) {
    response_body_length_ = 0;
  } else {
    switch (response_->headers->response_code()) {
      case HTTP_NO_CONTENT:     // No Content
      case HTTP_RESET_CONTENT:  // Reset Content
      case HTTP_NOT_MODIFIED:   // Not Modified
        response_body_length_ = 0;
        break;
    }
  }

  if (method_ == "HEAD") {
    response_body_length_ = 0;
  }

  if (response_body_length_ == -1) {
    // "Transfer-Encoding: chunked" trumps "Content-Length: N"
    if (response_->headers->IsChunkEncoded()) {
      chunked_decoder_ = std::make_unique<HttpChunkedDecoder>();
    } else {
      response_body_length_ = response_->headers->GetContentLength();
      // If response_body_length_ is still -1, then we have to wait
      // for the server to close the connection.
    }
  }
}

bool HttpStreamParser::IsResponseBodyComplete() const {
  if (chunked_decoder_.get())
    return chunked_decoder_->reached_eof();
  if (response_body_length_ != -1)
    return response_body_read_ >= response_body_length_;

  return false;  // Must read to EOF.
}

bool HttpStreamParser::CanFindEndOfResponse() const {
  return chunked_decoder_.get() || response_body_length_ >= 0;
}

bool HttpStreamParser::IsMoreDataBuffered() const {
  return read_buf_->offset() > read_buf_unused_offset_;
}

bool HttpStreamParser::CanReuseConnection() const {
  if (!CanFindEndOfResponse())
    return false;

  if (!response_is_keep_alive_)
    return false;

  // Check if extra data was received after reading the entire response body. If
  // extra data was received, reusing the socket is not a great idea. This does
  // have the down side of papering over certain server bugs, but seems to be
  // the best option here.
  //
  // TODO(mmenke): Consider logging this - hard to decipher socket reuse
  //     behavior makes NetLogs harder to read.
  if ((IsResponseBodyComplete() && IsMoreDataBuffered()) ||
      discarded_extra_data_) {
    return false;
  }

  return stream_socket_->IsConnected();
}

void HttpStreamParser::OnConnectionClose() {
  // This is to ensure `stream_socket_` doesn't get dangling on connection
  // close.
  stream_socket_ = nullptr;
}

int HttpStreamParser::EncodeChunk(std::string_view payload,
                                  base::span<uint8_t> output) {
  if (output.size() < payload.size() + kChunkHeaderFooterSize) {
    return ERR_INVALID_ARGUMENT;
  }

  auto span_writer = base::SpanWriter(output);
  // Add the header.
  const std::string header =
      base::StringPrintf("%X\r\n", static_cast<int>(payload.size()));
  span_writer.Write(base::as_byte_span(header));
  // Add the payload if any.
  if (payload.size() > 0) {
    span_writer.Write(base::as_byte_span(payload));
  }
  // Add the trailing CRLF.
  span_writer.Write(base::byte_span_from_cstring("\r\n"));

  return span_writer.num_written();
}

// static
bool HttpStreamParser::ShouldMergeRequestHeadersAndBody(
    const std::string& request_headers,
    const UploadDataStream* request_body) {
  if (request_body != nullptr &&
      // IsInMemory() ensures that the request body is not chunked.
      request_body->IsInMemory() && request_body->size() > 0) {
    uint64_t merged_size = request_headers.size() + request_body->size();
    if (merged_size <= kMaxMergedHeaderAndBodySize)
      return true;
  }
  return false;
}

bool HttpStreamParser::SendRequestBuffersEmpty() {
  return request_headers_ == nullptr && request_body_send_buf_ == nullptr &&
         request_body_read_buf_ == nullptr;
}

}  // namespace net
