// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http2_connection.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net {

namespace {

std::vector<http2::adapter::Header> GenerateHeaders(HttpStatusCode status,
                                                    base::StringPairs headers) {
  std::vector<http2::adapter::Header> response_vector;
  response_vector.emplace_back(
      http2::adapter::HeaderRep(std::string(":status")),
      http2::adapter::HeaderRep(base::NumberToString(status)));
  for (const auto& header : headers) {
    // Connection (and related) headers are considered malformed and will
    // result in a client error
    if (base::EqualsCaseInsensitiveASCII(header.first, "connection"))
      continue;
    response_vector.emplace_back(
        http2::adapter::HeaderRep(base::ToLowerASCII(header.first)),
        http2::adapter::HeaderRep(header.second));
  }

  return response_vector;
}

}  // namespace

namespace test_server {

class Http2Connection::DataFrameSource
    : public http2::adapter::DataFrameSource {
 public:
  explicit DataFrameSource(Http2Connection* connection,
                           const StreamId& stream_id)
      : connection_(connection), stream_id_(stream_id) {}
  ~DataFrameSource() override = default;
  DataFrameSource(const DataFrameSource&) = delete;
  DataFrameSource& operator=(const DataFrameSource&) = delete;

  std::pair<int64_t, bool> SelectPayloadLength(size_t max_length) override {
    if (chunks_.empty())
      return {kBlocked, last_frame_};

    bool finished = (chunks_.size() <= 1) &&
                    (chunks_.front().size() <= max_length) && last_frame_;

    return {std::min(chunks_.front().size(), max_length), finished};
  }

  bool Send(std::string_view frame_header, size_t payload_length) override {
    std::string concatenated =
        base::StrCat({frame_header, chunks_.front().substr(0, payload_length)});
    const int64_t result = connection_->OnReadyToSend(concatenated);
    // Write encountered error.
    if (result < 0) {
      connection_->OnConnectionError(ConnectionError::kSendError);
      return false;
    }

    // Write blocked.
    if (result == 0) {
      connection_->blocked_streams_.insert(*stream_id_);
      return false;
    }

    if (static_cast<const size_t>(result) < concatenated.size()) {
      // Probably need to handle this better within this test class.
      QUICHE_LOG(DFATAL)
          << "DATA frame not fully flushed. Connection will be corrupt!";
      connection_->OnConnectionError(ConnectionError::kSendError);
      return false;
    }

    chunks_.front().erase(0, payload_length);

    if (chunks_.front().empty())
      chunks_.pop();

    if (chunks_.empty() && send_completion_callback_) {
      std::move(send_completion_callback_).Run();
    }

    return true;
  }

  bool send_fin() const override { return true; }

  void AddChunk(std::string chunk) { chunks_.push(std::move(chunk)); }
  void set_last_frame(bool last_frame) { last_frame_ = last_frame; }
  void SetSendCompletionCallback(base::OnceClosure callback) {
    send_completion_callback_ = std::move(callback);
  }

 private:
  const raw_ptr<Http2Connection> connection_;
  const raw_ref<const StreamId, DanglingUntriaged> stream_id_;
  std::queue<std::string> chunks_;
  bool last_frame_ = false;
  base::OnceClosure send_completion_callback_;
};

// Corresponds to an HTTP/2 stream
class Http2Connection::ResponseDelegate : public HttpResponseDelegate {
 public:
  ResponseDelegate(Http2Connection* connection, StreamId stream_id)
      : stream_id_(stream_id), connection_(connection) {}
  ~ResponseDelegate() override = default;
  ResponseDelegate(const ResponseDelegate&) = delete;
  ResponseDelegate& operator=(const ResponseDelegate&) = delete;

  void AddResponse(std::unique_ptr<HttpResponse> response) override {
    responses_.push_back(std::move(response));
  }

  void SendResponseHeaders(HttpStatusCode status,
                           const std::string& status_reason,
                           const base::StringPairs& headers) override {
    std::unique_ptr<DataFrameSource> data_frame =
        std::make_unique<DataFrameSource>(connection_, stream_id_);
    data_frame_ = data_frame.get();
    connection_->adapter()->SubmitResponse(
        stream_id_, GenerateHeaders(status, headers), std::move(data_frame),
        /*end_stream=*/false);
    connection_->SendIfNotProcessing();
  }

  void SendRawResponseHeaders(const std::string& headers) override {
    scoped_refptr<HttpResponseHeaders> parsed_headers =
        HttpResponseHeaders::TryToCreate(headers);
    if (parsed_headers->response_code() == 0) {
      connection_->OnConnectionError(ConnectionError::kParseError);
      LOG(ERROR) << "raw headers could not be parsed";
    }
    base::StringPairs header_pairs;
    size_t iter = 0;
    std::string key, value;
    while (parsed_headers->EnumerateHeaderLines(&iter, &key, &value))
      header_pairs.emplace_back(key, value);
    SendResponseHeaders(
        static_cast<HttpStatusCode>(parsed_headers->response_code()),
        /*status_reason=*/"", header_pairs);
  }

  void SendContents(const std::string& contents,
                    base::OnceClosure callback) override {
    DCHECK(data_frame_);
    data_frame_->AddChunk(contents);
    data_frame_->SetSendCompletionCallback(std::move(callback));
    connection_->adapter()->ResumeStream(stream_id_);
    connection_->SendIfNotProcessing();
  }

  void FinishResponse() override {
    data_frame_->set_last_frame(true);
    connection_->adapter()->ResumeStream(stream_id_);
    connection_->SendIfNotProcessing();
  }

  void SendContentsAndFinish(const std::string& contents) override {
    data_frame_->set_last_frame(true);
    SendContents(contents, base::DoNothing());
  }

  void SendHeadersContentAndFinish(HttpStatusCode status,
                                   const std::string& status_reason,
                                   const base::StringPairs& headers,
                                   const std::string& contents) override {
    std::unique_ptr<DataFrameSource> data_frame =
        std::make_unique<DataFrameSource>(connection_, stream_id_);
    data_frame->AddChunk(contents);
    data_frame->set_last_frame(true);
    connection_->adapter()->SubmitResponse(
        stream_id_, GenerateHeaders(status, headers), std::move(data_frame),
        /*end_stream=*/false);
    connection_->SendIfNotProcessing();
  }
  base::WeakPtr<ResponseDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::vector<std::unique_ptr<HttpResponse>> responses_;
  StreamId stream_id_;
  const raw_ptr<Http2Connection> connection_;
  raw_ptr<DataFrameSource, DanglingUntriaged> data_frame_;
  base::WeakPtrFactory<ResponseDelegate> weak_factory_{this};
};

Http2Connection::Http2Connection(
    std::unique_ptr<StreamSocket> socket,
    EmbeddedTestServerConnectionListener* connection_listener,
    EmbeddedTestServer* embedded_test_server)
    : socket_(std::move(socket)),
      connection_listener_(connection_listener),
      embedded_test_server_(embedded_test_server),
      read_buf_(base::MakeRefCounted<IOBufferWithSize>(4096)) {
  http2::adapter::OgHttp2Adapter::Options options;
  options.perspective = http2::adapter::Perspective::kServer;
  adapter_ = http2::adapter::OgHttp2Adapter::Create(*this, options);
}

Http2Connection::~Http2Connection() = default;

void Http2Connection::OnSocketReady() {
  ReadData();
}

void Http2Connection::ReadData() {
  while (true) {
    int rv = socket_->Read(
        read_buf_.get(), read_buf_->size(),
        base::BindOnce(&Http2Connection::OnDataRead, base::Unretained(this)));
    if (rv == ERR_IO_PENDING)
      return;
    if (!HandleData(rv))
      return;
  }
}

void Http2Connection::OnDataRead(int rv) {
  if (HandleData(rv))
    ReadData();
}

bool Http2Connection::HandleData(int rv) {
  if (rv <= 0) {
    embedded_test_server_->RemoveConnection(this);
    return false;
  }

  if (connection_listener_)
    connection_listener_->ReadFromSocket(*socket_, rv);

  std::string_view remaining_buffer(read_buf_->data(), rv);
  while (!remaining_buffer.empty()) {
    int result = adapter_->ProcessBytes(remaining_buffer);
    if (result < 0)
      return false;
    remaining_buffer = remaining_buffer.substr(result);
  }

  // Any frames and data sources will be queued up and sent all at once below
  DCHECK(!processing_responses_);
  processing_responses_ = true;
  while (!ready_streams_.empty()) {
    StreamId stream_id = ready_streams_.front();
    ready_streams_.pop();
    auto delegate = std::make_unique<ResponseDelegate>(this, stream_id);
    ResponseDelegate* delegate_ptr = delegate.get();
    response_map_[stream_id] = std::move(delegate);
    embedded_test_server_->HandleRequest(delegate_ptr->GetWeakPtr(),
                                         std::move(request_map_[stream_id]),
                                         socket_.get());
    request_map_.erase(stream_id);
  }
  adapter_->Send();
  processing_responses_ = false;
  return true;
}

StreamSocket* Http2Connection::Socket() {
  return socket_.get();
}

std::unique_ptr<StreamSocket> Http2Connection::TakeSocket() {
  return std::move(socket_);
}

base::WeakPtr<HttpConnection> Http2Connection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int64_t Http2Connection::OnReadyToSend(std::string_view serialized) {
  if (write_buf_)
    return kSendBlocked;

  write_buf_ = base::MakeRefCounted<DrainableIOBuffer>(
      base::MakeRefCounted<StringIOBuffer>(std::string(serialized)),
      serialized.size());
  SendInternal();
  return serialized.size();
}

bool Http2Connection::OnCloseStream(StreamId stream_id,
                                    http2::adapter::Http2ErrorCode error_code) {
  response_map_.erase(stream_id);
  return true;
}

void Http2Connection::SendInternal() {
  DCHECK(socket_);
  DCHECK(write_buf_);
  while (write_buf_->BytesRemaining() > 0) {
    int rv = socket_->Write(write_buf_.get(), write_buf_->BytesRemaining(),
                            base::BindOnce(&Http2Connection::OnSendInternalDone,
                                           base::Unretained(this)),
                            TRAFFIC_ANNOTATION_FOR_TESTS);
    if (rv == ERR_IO_PENDING)
      return;

    if (rv < 0) {
      embedded_test_server_->RemoveConnection(this);
      break;
    }

    write_buf_->DidConsume(rv);
  }
  write_buf_ = nullptr;
}

void Http2Connection::OnSendInternalDone(int rv) {
  DCHECK(write_buf_);
  if (rv < 0) {
    embedded_test_server_->RemoveConnection(this);
    write_buf_ = nullptr;
    return;
  }
  write_buf_->DidConsume(rv);

  SendInternal();

  if (!write_buf_) {
    // Now that writing is no longer blocked, any blocked streams can be
    // resumed.
    for (const auto& stream_id : blocked_streams_)
      adapter_->ResumeStream(stream_id);

    if (adapter_->want_write()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Http2Connection::SendIfNotProcessing,
                                    weak_factory_.GetWeakPtr()));
    }
  }
}

void Http2Connection::SendIfNotProcessing() {
  if (!processing_responses_) {
    processing_responses_ = true;
    adapter_->Send();
    processing_responses_ = false;
  }
}

http2::adapter::Http2VisitorInterface::OnHeaderResult
Http2Connection::OnHeaderForStream(http2::adapter::Http2StreamId stream_id,
                                   std::string_view key,
                                   std::string_view value) {
  header_map_[stream_id][std::string(key)] = std::string(value);
  return http2::adapter::Http2VisitorInterface::HEADER_OK;
}

bool Http2Connection::OnEndHeadersForStream(
    http2::adapter::Http2StreamId stream_id) {
  HttpRequest::HeaderMap header_map = header_map_[stream_id];
  auto request = std::make_unique<HttpRequest>();
  // TODO(crbug.com/40242862): Handle proxy cases.
  request->relative_url = header_map[":path"];
  request->base_url = GURL(header_map[":authority"]);
  request->method_string = header_map[":method"];
  request->method = HttpRequestParser::GetMethodType(request->method_string);
  request->headers = header_map;

  request->has_content = false;

  SSLInfo ssl_info;
  DCHECK(socket_->GetSSLInfo(&ssl_info));
  request->ssl_info = ssl_info;
  request_map_[stream_id] = std::move(request);

  return true;
}

bool Http2Connection::OnEndStream(http2::adapter::Http2StreamId stream_id) {
  ready_streams_.push(stream_id);
  return true;
}

bool Http2Connection::OnFrameHeader(StreamId /*stream_id*/,
                                    size_t /*length*/,
                                    uint8_t /*type*/,
                                    uint8_t /*flags*/) {
  return true;
}

bool Http2Connection::OnBeginHeadersForStream(StreamId stream_id) {
  return true;
}

bool Http2Connection::OnBeginDataForStream(StreamId stream_id,
                                           size_t payload_length) {
  return true;
}

bool Http2Connection::OnDataForStream(StreamId stream_id,
                                      std::string_view data) {
  auto request = request_map_.find(stream_id);
  if (request == request_map_.end()) {
    // We should not receive data before receiving headers.
    return false;
  }

  request->second->has_content = true;
  request->second->content.append(data);
  adapter_->MarkDataConsumedForStream(stream_id, data.size());
  return true;
}

bool Http2Connection::OnDataPaddingLength(StreamId stream_id,
                                          size_t padding_length) {
  adapter_->MarkDataConsumedForStream(stream_id, padding_length);
  return true;
}

bool Http2Connection::OnGoAway(StreamId last_accepted_stream_id,
                               http2::adapter::Http2ErrorCode error_code,
                               std::string_view opaque_data) {
  return true;
}

int Http2Connection::OnBeforeFrameSent(uint8_t frame_type,
                                       StreamId stream_id,
                                       size_t length,
                                       uint8_t flags) {
  return 0;
}

int Http2Connection::OnFrameSent(uint8_t frame_type,
                                 StreamId stream_id,
                                 size_t length,
                                 uint8_t flags,
                                 uint32_t error_code) {
  return 0;
}

bool Http2Connection::OnInvalidFrame(StreamId stream_id,
                                     InvalidFrameError error) {
  return true;
}

bool Http2Connection::OnMetadataForStream(StreamId stream_id,
                                          std::string_view metadata) {
  return true;
}

bool Http2Connection::OnMetadataEndForStream(StreamId stream_id) {
  return true;
}

}  // namespace test_server

}  // namespace net
