// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_stream.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/spdy/spdy_log_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_write_blocked_list.h"

namespace net {
namespace {
// Sets a boolean to a value, and restores it to the previous value once
// the saver goes out of scope.
class ScopedBoolSaver {
 public:
  ScopedBoolSaver(bool* var, bool new_val) : var_(var), old_val_(*var) {
    *var_ = new_val;
  }

  ~ScopedBoolSaver() { *var_ = old_val_; }

 private:
  raw_ptr<bool> var_;
  bool old_val_;
};
}  // namespace

QuicChromiumClientStream::Handle::Handle(QuicChromiumClientStream* stream)
    : stream_(stream), net_log_(stream->net_log()) {
  SaveState();
}

QuicChromiumClientStream::Handle::~Handle() {
  if (stream_) {
    stream_->ClearHandle();
    // TODO(rch): If stream_ is still valid, it should probably be Reset()
    // so that it does not leak.
    // stream_->Reset(quic::QUIC_STREAM_CANCELLED);
  }
}

void QuicChromiumClientStream::Handle::OnEarlyHintsAvailable() {
  if (first_early_hints_time_.is_null())
    first_early_hints_time_ = base::TimeTicks::Now();

  if (!read_headers_callback_)
    return;  // Wait for ReadInitialHeaders to be called.

  DCHECK(read_headers_buffer_);
  int rv = stream_->DeliverEarlyHints(read_headers_buffer_);
  DCHECK_NE(ERR_IO_PENDING, rv);

  ResetAndRun(std::move(read_headers_callback_), rv);
}

void QuicChromiumClientStream::Handle::OnInitialHeadersAvailable() {
  if (headers_received_start_time_.is_null())
    headers_received_start_time_ = base::TimeTicks::Now();

  if (!read_headers_callback_)
    return;  // Wait for ReadInitialHeaders to be called.

  int rv = stream_->DeliverInitialHeaders(read_headers_buffer_);
  DCHECK_NE(ERR_IO_PENDING, rv);

  ResetAndRun(std::move(read_headers_callback_), rv);
}

void QuicChromiumClientStream::Handle::OnTrailingHeadersAvailable() {
  if (!read_headers_callback_)
    return;  // Wait for ReadInitialHeaders to be called.

  int rv = ERR_QUIC_PROTOCOL_ERROR;
  if (!stream_->DeliverTrailingHeaders(read_headers_buffer_, &rv))
    rv = ERR_QUIC_PROTOCOL_ERROR;

  base::UmaHistogramBoolean(
      "Net.QuicChromiumClientStream.TrailingHeadersProcessSuccess", rv >= 0);
  ResetAndRun(std::move(read_headers_callback_), rv);
}

void QuicChromiumClientStream::Handle::OnDataAvailable() {
  if (!read_body_callback_)
    return;  // Wait for ReadBody to be called.

  DCHECK(read_body_buffer_);
  DCHECK_GT(read_body_buffer_len_, 0);

  int rv = stream_->Read(read_body_buffer_.get(), read_body_buffer_len_);
  if (rv == ERR_IO_PENDING)
    return;  // Spurrious, likely because of trailers?

  read_body_buffer_ = nullptr;
  read_body_buffer_len_ = 0;
  ResetAndRun(std::move(read_body_callback_), rv);
}

void QuicChromiumClientStream::Handle::OnCanWrite() {
  if (!write_callback_)
    return;

  ResetAndRun(std::move(write_callback_), OK);
}

void QuicChromiumClientStream::Handle::OnClose() {
  if (net_error_ == ERR_UNEXPECTED) {
    if (stream_error() == quic::QUIC_STREAM_NO_ERROR &&
        connection_error() == quic::QUIC_NO_ERROR && fin_sent() &&
        fin_received()) {
      net_error_ = ERR_CONNECTION_CLOSED;
    } else {
      net_error_ = ERR_QUIC_PROTOCOL_ERROR;
    }
  }
  base::UmaHistogramSparse("Net.QuicChromiumClientStream.HandleOnCloseNetError",
                           -net_error_);
  base::UmaHistogramSparse(
      "Net.QuicChromiumClientStream.HandleOnCloseStreamError", stream_error());
  base::UmaHistogramSparse(
      "Net.QuicChromiumClientStream.HandleOnCloseConnectionError",
      connection_error());
  OnError(net_error_);
}

void QuicChromiumClientStream::Handle::OnError(int error) {
  net_error_ = error;
  if (stream_)
    SaveState();
  stream_ = nullptr;

  // Post a task to invoke the callbacks to ensure that there is no reentrancy.
  // A ScopedPacketFlusher might cause an error which closes the stream under
  // the call stack of the owner of the handle.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicChromiumClientStream::Handle::InvokeCallbacksOnClose,
                     weak_factory_.GetWeakPtr(), error));
}

void QuicChromiumClientStream::Handle::InvokeCallbacksOnClose(int error) {
  // Invoking a callback may cause |this| to be deleted. If this happens, no
  // more callbacks should be invoked. Guard against this by holding a WeakPtr
  // to |this| and ensuring it's still valid.

  // Free read buffer, if present. Reads are synchronous and pull-based, so
  // there is no ongoing asynchronous read that could write to the buffer.
  read_body_buffer_ = nullptr;
  read_body_buffer_len_ = 0;

  auto guard(weak_factory_.GetWeakPtr());
  for (auto* callback :
       {&read_headers_callback_, &read_body_callback_, &write_callback_}) {
    if (*callback)
      ResetAndRun(std::move(*callback), error);
    if (!guard.get())
      return;
  }
}

int QuicChromiumClientStream::Handle::ReadInitialHeaders(
    quiche::HttpHeaderBlock* header_block,
    CompletionOnceCallback callback) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (!stream_)
    return net_error_;

  // Check Early Hints first.
  int rv = stream_->DeliverEarlyHints(header_block);
  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  rv = stream_->DeliverInitialHeaders(header_block);
  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  read_headers_buffer_ = header_block;
  DCHECK(!read_headers_callback_);
  SetCallback(std::move(callback), &read_headers_callback_);
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::Handle::ReadBody(
    IOBuffer* buffer,
    int buffer_len,
    CompletionOnceCallback callback) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (IsDoneReading())
    return OK;

  if (!stream_)
    return net_error_;

  if (stream_->read_side_closed()) {
    return OK;
  }

  int rv = stream_->Read(buffer, buffer_len);
  if (rv != ERR_IO_PENDING)
    return rv;

  DCHECK(buffer);
  DCHECK_GT(buffer_len, 0);

  SetCallback(std::move(callback), &read_body_callback_);
  read_body_buffer_ = buffer;
  read_body_buffer_len_ = buffer_len;
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::Handle::ReadTrailingHeaders(
    quiche::HttpHeaderBlock* header_block,
    CompletionOnceCallback callback) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (!stream_)
    return net_error_;

  int frame_len = 0;
  if (stream_->DeliverTrailingHeaders(header_block, &frame_len))
    return frame_len;

  read_headers_buffer_ = header_block;
  SetCallback(std::move(callback), &read_headers_callback_);
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::Handle::WriteHeaders(
    quiche::HttpHeaderBlock header_block,
    bool fin,
    quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
        ack_notifier_delegate) {
  if (!stream_)
    return 0;
  return HandleIOComplete(stream_->WriteHeaders(std::move(header_block), fin,
                                                ack_notifier_delegate));
}

int QuicChromiumClientStream::Handle::WriteStreamData(
    std::string_view data,
    bool fin,
    CompletionOnceCallback callback) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (!stream_)
    return net_error_;

  if (stream_->WriteStreamData(data, fin)) {
    return HandleIOComplete(OK);
  }

  SetCallback(std::move(callback), &write_callback_);
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::Handle::WritevStreamData(
    const std::vector<scoped_refptr<IOBuffer>>& buffers,
    const std::vector<int>& lengths,
    bool fin,
    CompletionOnceCallback callback) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (!stream_)
    return net_error_;

  if (stream_->WritevStreamData(buffers, lengths, fin))
    return HandleIOComplete(OK);

  SetCallback(std::move(callback), &write_callback_);
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::Handle::WriteConnectUdpPayload(
    std::string_view packet) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  if (!stream_) {
    return net_error_;
  }

  base::UmaHistogramBoolean(kHttp3DatagramDroppedHistogram,
                            !stream_->SupportsH3Datagram());
  if (!stream_->SupportsH3Datagram()) {
    DLOG(WARNING)
        << "Dropping datagram because the session has either not received "
           "settings frame with H3_DATAGRAM yet or received settings that "
           "indicate datagrams are not supported (i.e., H3_DATAGRAM=0).";
    return OK;
  }
  // Set Context ID to zero as per RFC 9298
  // (https://datatracker.ietf.org/doc/html/rfc9298#name-http-datagram-payload-forma)
  // and copy packet data.
  std::string http_payload;
  http_payload.resize(1 + packet.size());
  http_payload[0] = 0;
  memcpy(&http_payload[1], packet.data(), packet.size());

  // Attempt to send the HTTP payload as a datagram over the stream.
  quic::MessageStatus message_status = stream_->SendHttp3Datagram(http_payload);

  // If the attempt was successful or blocked (e.g., due to buffer
  // constraints), proceed to handle the I/O completion with an OK status.
  if (message_status == quic::MessageStatus::MESSAGE_STATUS_SUCCESS ||
      message_status == quic::MessageStatus::MESSAGE_STATUS_BLOCKED) {
    return HandleIOComplete(OK);
  }
  // If the attempt failed due to a unsupported feature, internal error, or
  // unexpected condition (encryption not established or message too large),
  // reset the stream and close the connection.
  else {
    // These two errors should not be possible here.
    DCHECK(message_status !=
           quic::MessageStatus::MESSAGE_STATUS_ENCRYPTION_NOT_ESTABLISHED);
    DCHECK(message_status != quic::MessageStatus::MESSAGE_STATUS_TOO_LARGE);
    DLOG(ERROR) << "Failed to send Http3 Datagram on " << stream_->id();
    stream_->Reset(quic::QUIC_STREAM_CANCELLED);
    return ERR_CONNECTION_CLOSED;
  }
}

int QuicChromiumClientStream::Handle::Read(IOBuffer* buf, int buf_len) {
  if (!stream_)
    return net_error_;
  return stream_->Read(buf, buf_len);
}

void QuicChromiumClientStream::Handle::OnFinRead() {
  read_headers_callback_.Reset();
  if (stream_)
    stream_->OnFinRead();
}

void QuicChromiumClientStream::Handle::
    DisableConnectionMigrationToCellularNetwork() {
  if (stream_)
    stream_->DisableConnectionMigrationToCellularNetwork();
}

void QuicChromiumClientStream::Handle::SetPriority(
    const quic::QuicStreamPriority& priority) {
  if (stream_) {
    stream_->SetPriority(priority);
  }
}

void QuicChromiumClientStream::Handle::Reset(
    quic::QuicRstStreamErrorCode error_code) {
  if (stream_)
    stream_->Reset(error_code);
}

void QuicChromiumClientStream::Handle::RegisterHttp3DatagramVisitor(
    Http3DatagramVisitor* visitor) {
  if (stream_) {
    stream_->RegisterHttp3DatagramVisitor(visitor);
  }
}

void QuicChromiumClientStream::Handle::UnregisterHttp3DatagramVisitor() {
  if (stream_) {
    stream_->UnregisterHttp3DatagramVisitor();
  }
}

quic::QuicStreamId QuicChromiumClientStream::Handle::id() const {
  if (!stream_)
    return id_;
  return stream_->id();
}

quic::QuicErrorCode QuicChromiumClientStream::Handle::connection_error() const {
  if (!stream_)
    return connection_error_;
  return stream_->connection_error();
}

quic::QuicRstStreamErrorCode QuicChromiumClientStream::Handle::stream_error()
    const {
  if (!stream_)
    return stream_error_;
  return stream_->stream_error();
}

uint64_t QuicChromiumClientStream::Handle::connection_wire_error() const {
  if (!stream_) {
    return connection_wire_error_;
  }
  // TODO(crbug.com/40715622): Don't access session. Instead, modify
  // quic::QuicStream::OnConnectionClosed() to take the wire error code.
  CHECK(stream_->session());
  return stream_->session()->wire_error();
}

uint64_t QuicChromiumClientStream::Handle::ietf_application_error() const {
  if (!stream_) {
    return ietf_application_error_;
  }
  return stream_->ietf_application_error();
}

bool QuicChromiumClientStream::Handle::fin_sent() const {
  if (!stream_)
    return fin_sent_;
  return stream_->fin_sent();
}

bool QuicChromiumClientStream::Handle::fin_received() const {
  if (!stream_)
    return fin_received_;
  return stream_->fin_received();
}

uint64_t QuicChromiumClientStream::Handle::stream_bytes_read() const {
  if (!stream_)
    return stream_bytes_read_;
  return stream_->stream_bytes_read();
}

uint64_t QuicChromiumClientStream::Handle::stream_bytes_written() const {
  if (!stream_)
    return stream_bytes_written_;
  return stream_->stream_bytes_written();
}

size_t QuicChromiumClientStream::Handle::NumBytesConsumed() const {
  if (!stream_)
    return num_bytes_consumed_;
  return stream_->sequencer()->NumBytesConsumed();
}

bool QuicChromiumClientStream::Handle::HasBytesToRead() const {
  if (!stream_)
    return false;
  return stream_->HasBytesToRead();
}

bool QuicChromiumClientStream::Handle::IsDoneReading() const {
  if (!stream_)
    return is_done_reading_;
  return stream_->IsDoneReading();
}

bool QuicChromiumClientStream::Handle::IsFirstStream() const {
  if (!stream_)
    return is_first_stream_;
  return stream_->IsFirstStream();
}

bool QuicChromiumClientStream::Handle::can_migrate_to_cellular_network() {
  if (!stream_)
    return false;
  return stream_->can_migrate_to_cellular_network();
}

const NetLogWithSource& QuicChromiumClientStream::Handle::net_log() const {
  return net_log_;
}

void QuicChromiumClientStream::Handle::SaveState() {
  DCHECK(stream_);
  fin_sent_ = stream_->fin_sent();
  fin_received_ = stream_->fin_received();
  num_bytes_consumed_ = stream_->sequencer()->NumBytesConsumed();
  id_ = stream_->id();
  connection_error_ = stream_->connection_error();
  stream_error_ = stream_->stream_error();
  // TODO(crbug.com/40715622): Don't access stream_->session(). Instead, update
  // quic::QuicStream::OnConnectionClosed() to take the wire error code.
  CHECK(stream_->session());
  connection_wire_error_ = stream_->session()->wire_error();
  ietf_application_error_ = stream_->ietf_application_error();
  is_done_reading_ = stream_->IsDoneReading();
  is_first_stream_ = stream_->IsFirstStream();
  stream_bytes_read_ = stream_->stream_bytes_read();
  stream_bytes_written_ = stream_->stream_bytes_written();
}

void QuicChromiumClientStream::Handle::SetCallback(
    CompletionOnceCallback new_callback,
    CompletionOnceCallback* callback) {
  // TODO(rch): Convert this to a DCHECK once we ensure the API is stable and
  // bug free.
  CHECK(!may_invoke_callbacks_);
  *callback = std::move(new_callback);
}

void QuicChromiumClientStream::Handle::ResetAndRun(
    CompletionOnceCallback callback,
    int rv) {
  // TODO(rch): Convert this to a DCHECK once we ensure the API is stable and
  // bug free.
  CHECK(may_invoke_callbacks_);
  std::move(callback).Run(rv);
}

int QuicChromiumClientStream::Handle::HandleIOComplete(int rv) {
  // If |stream_| is still valid the stream has not been closed. If the stream
  // has not been closed, then just return |rv|.
  if (rv < 0 || stream_)
    return rv;

  if (stream_error_ == quic::QUIC_STREAM_NO_ERROR &&
      connection_error_ == quic::QUIC_NO_ERROR && fin_sent_ && fin_received_) {
    return rv;
  }

  return net_error_;
}

void QuicChromiumClientStream::Handle::SetRequestIdempotency(
    Idempotency idempotency) {
  idempotency_ = idempotency;
}

Idempotency QuicChromiumClientStream::Handle::GetRequestIdempotency() const {
  return idempotency_;
}

quic::QuicPacketLength
QuicChromiumClientStream::Handle::GetGuaranteedLargestMessagePayload() const {
  if (!stream_) {
    return 0;
  }
  return stream_->GetGuaranteedLargestMessagePayload();
}

QuicChromiumClientStream::QuicChromiumClientStream(
    quic::QuicStreamId id,
    quic::QuicSpdyClientSessionBase* session,
    quic::QuicServerId server_id,
    quic::StreamType type,
    const NetLogWithSource& net_log,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : quic::QuicSpdyStream(id, session, type),
      net_log_(net_log),
      session_(session),
      server_id_(std::move(server_id)),
      quic_version_(session->connection()->transport_version()) {}

QuicChromiumClientStream::QuicChromiumClientStream(
    quic::PendingStream* pending,
    quic::QuicSpdyClientSessionBase* session,
    quic::QuicServerId server_id,
    const NetLogWithSource& net_log,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : quic::QuicSpdyStream(pending, session),
      net_log_(net_log),
      session_(session),
      server_id_(std::move(server_id)),
      quic_version_(session->connection()->transport_version()) {}

QuicChromiumClientStream::~QuicChromiumClientStream() {
  if (handle_)
    handle_->OnClose();
}

void QuicChromiumClientStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const quic::QuicHeaderList& header_list) {
  DCHECK(!initial_headers_arrived_);
  quic::QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);

  if (header_decoding_delay().has_value()) {
    const int64_t delay_in_milliseconds =
        header_decoding_delay()->ToMilliseconds();
    base::UmaHistogramTimes("Net.QuicChromiumClientStream.HeaderDecodingDelay",
                            base::Milliseconds(delay_in_milliseconds));
    if (IsGoogleHost(server_id_.host())) {
      base::UmaHistogramTimes(
          "Net.QuicChromiumClientStream.HeaderDecodingDelayGoogle",
          base::Milliseconds(delay_in_milliseconds));
    }
  }

  quiche::HttpHeaderBlock header_block;
  int64_t length = -1;
  if (!quic::SpdyUtils::CopyAndValidateHeaders(header_list, &length,
                                               &header_block)) {
    DLOG(ERROR) << "Failed to parse header list: " << header_list.DebugString();
    ConsumeHeaderList();
    Reset(quic::QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  // Handle informational response. If the response is an Early Hints response,
  // deliver the response to the owner of the handle. Otherwise ignore the
  // response.
  int response_code;
  if (!ParseHeaderStatusCode(header_block, &response_code)) {
    DLOG(ERROR) << "Received invalid response code: '"
                << header_block[":status"].as_string() << "' on stream "
                << id();
    Reset(quic::QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  if (response_code == HTTP_SWITCHING_PROTOCOLS) {
    DLOG(ERROR) << "Received forbidden 101 response code on stream " << id();
    Reset(quic::QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  if (response_code >= 100 && response_code < 200) {
    set_headers_decompressed(false);
    ConsumeHeaderList();
    if (response_code == HTTP_EARLY_HINTS) {
      early_hints_.emplace_back(std::move(header_block), frame_len);
      if (handle_)
        handle_->OnEarlyHintsAvailable();
    } else {
      DVLOG(1) << "Ignore informational response " << response_code
               << " on stream" << id();
    }
    return;
  }

  ConsumeHeaderList();

  // Buffer the headers and deliver them when the handle arrives.
  initial_headers_arrived_ = true;
  initial_headers_ = std::move(header_block);
  initial_headers_frame_len_ = frame_len;

  if (handle_) {
    // The handle will be notified of the headers via a posted task.
    NotifyHandleOfInitialHeadersAvailableLater();
  }
}

void QuicChromiumClientStream::OnTrailingHeadersComplete(
    bool fin,
    size_t frame_len,
    const quic::QuicHeaderList& header_list) {
  quic::QuicSpdyStream::OnTrailingHeadersComplete(fin, frame_len, header_list);
  trailing_headers_frame_len_ = frame_len;
  if (handle_) {
    // The handle will be notified of the headers via a posted task.
    NotifyHandleOfTrailingHeadersAvailableLater();
  }
}

void QuicChromiumClientStream::OnBodyAvailable() {
  if (!FinishedReadingHeaders() || !headers_delivered_) {
    // Buffer the data in the sequencer until the headers have been read.
    return;
  }

  if (!HasBytesToRead() && !FinishedReadingTrailers()) {
    // If there is no data to read, wait until either FIN is received or
    // trailers are delivered.
    return;
  }

  // The handle will read the data via a posted task, and
  // will be able to, potentially, read all data which has queued up.
  if (handle_)
    NotifyHandleOfDataAvailableLater();
}

void QuicChromiumClientStream::OnClose() {
  if (handle_) {
    handle_->OnClose();
    handle_ = nullptr;
  }
  quic::QuicStream::OnClose();
}

void QuicChromiumClientStream::OnCanWrite() {
  quic::QuicStream::OnCanWrite();

  if (!HasBufferedData() && handle_)
    handle_->OnCanWrite();
}

size_t QuicChromiumClientStream::WriteHeaders(
    quiche::HttpHeaderBlock header_block,
    bool fin,
    quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
        ack_listener) {
  if (!session()->OneRttKeysAvailable()) {
    auto entry = header_block.find(":method");
    CHECK(entry != header_block.end(), base::NotFatalUntil::M130);
    DCHECK(
        entry->second != "POST" ||
        (handle_ != nullptr && handle_->GetRequestIdempotency() == IDEMPOTENT));
  }
  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS,
      [&](NetLogCaptureMode capture_mode) {
        return QuicRequestNetLogParams(id(), &header_block, priority(),
                                       capture_mode);
      });
  size_t len = quic::QuicSpdyStream::WriteHeaders(std::move(header_block), fin,
                                                  std::move(ack_listener));
  initial_headers_sent_ = true;
  return len;
}

bool QuicChromiumClientStream::WriteStreamData(std::string_view data,
                                               bool fin) {
  // Writes the data, or buffers it.
  WriteOrBufferBody(data, fin);
  return !HasBufferedData();  // Was all data written?
}

bool QuicChromiumClientStream::WritevStreamData(
    const std::vector<scoped_refptr<IOBuffer>>& buffers,
    const std::vector<int>& lengths,
    bool fin) {
  // Writes the data, or buffers it.
  for (size_t i = 0; i < buffers.size(); ++i) {
    bool is_fin = fin && (i == buffers.size() - 1);
    std::string_view string_data(buffers[i]->data(), lengths[i]);
    WriteOrBufferBody(string_data, is_fin);
  }
  return !HasBufferedData();  // Was all data written?
}

std::unique_ptr<QuicChromiumClientStream::Handle>
QuicChromiumClientStream::CreateHandle() {
  DCHECK(!handle_);
  auto handle = base::WrapUnique(new QuicChromiumClientStream::Handle(this));
  handle_ = handle.get();

  // Should this perhaps be via PostTask to make reasoning simpler?
  if (initial_headers_arrived_) {
    handle_->OnInitialHeadersAvailable();
  }

  return handle;
}

void QuicChromiumClientStream::ClearHandle() {
  handle_ = nullptr;
}

void QuicChromiumClientStream::OnError(int error) {
  if (handle_) {
    QuicChromiumClientStream::Handle* handle = handle_;
    handle_ = nullptr;
    handle->OnError(error);
  }
}

bool QuicChromiumClientStream::SupportsH3Datagram() const {
  return session_->SupportsH3Datagram();
}

int QuicChromiumClientStream::Read(IOBuffer* buf, int buf_len) {
  DCHECK_GT(buf_len, 0);
  DCHECK(buf->data());

  if (IsDoneReading())
    return 0;  // EOF

  if (!HasBytesToRead())
    return ERR_IO_PENDING;

  iovec iov;
  iov.iov_base = buf->data();
  iov.iov_len = buf_len;
  size_t bytes_read = Readv(&iov, 1);
  // Since HasBytesToRead is true, Readv() must of read some data.
  DCHECK_NE(0u, bytes_read);
  return bytes_read;
}

void QuicChromiumClientStream::NotifyHandleOfInitialHeadersAvailableLater() {
  DCHECK(handle_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuicChromiumClientStream::NotifyHandleOfInitialHeadersAvailable,
          weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientStream::NotifyHandleOfInitialHeadersAvailable() {
  if (!handle_)
    return;

  if (!headers_delivered_)
    handle_->OnInitialHeadersAvailable();
}

void QuicChromiumClientStream::NotifyHandleOfTrailingHeadersAvailableLater() {
  DCHECK(handle_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuicChromiumClientStream::NotifyHandleOfTrailingHeadersAvailable,
          weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientStream::NotifyHandleOfTrailingHeadersAvailable() {
  if (!handle_)
    return;

  // If trailers aren't decompressed it means that trailers are invalid
  // (e.g., contain ":status" field). Don't notify to the handle if trailers
  // aren't decompressed since the stream will be closed and
  // `headers_delivered_` won't become true.
  if (!trailers_decompressed())
    return;

  // Notify only after the handle reads initial headers.
  if (!headers_delivered_)
    return;

  // Post an async task to notify handle of the FIN flag.
  NotifyHandleOfDataAvailableLater();
  handle_->OnTrailingHeadersAvailable();
}

int QuicChromiumClientStream::DeliverEarlyHints(
    quiche::HttpHeaderBlock* headers) {
  if (early_hints_.empty()) {
    return ERR_IO_PENDING;
  }

  DCHECK(!headers_delivered_);

  EarlyHints& hints = early_hints_.front();
  *headers = std::move(hints.headers);
  size_t frame_len = hints.frame_len;
  early_hints_.pop_front();

  net_log_.AddEvent(
      NetLogEventType::
          QUIC_CHROMIUM_CLIENT_STREAM_READ_EARLY_HINTS_RESPONSE_HEADERS,
      [&](NetLogCaptureMode capture_mode) {
        return QuicResponseNetLogParams(id(), fin_received(), headers,
                                        capture_mode);
      });

  return frame_len;
}

int QuicChromiumClientStream::DeliverInitialHeaders(
    quiche::HttpHeaderBlock* headers) {
  if (!initial_headers_arrived_) {
    return ERR_IO_PENDING;
  }

  headers_delivered_ = true;

  if (initial_headers_.empty()) {
    return ERR_INVALID_RESPONSE;
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_HEADERS,
      [&](NetLogCaptureMode capture_mode) {
        return QuicResponseNetLogParams(id(), fin_received(), &initial_headers_,
                                        capture_mode);
      });

  *headers = std::move(initial_headers_);
  return initial_headers_frame_len_;
}

bool QuicChromiumClientStream::DeliverTrailingHeaders(
    quiche::HttpHeaderBlock* headers,
    int* frame_len) {
  if (trailing_headers_frame_len_ == 0) {
    return false;
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_TRAILERS,
      [&](NetLogCaptureMode capture_mode) {
        return QuicResponseNetLogParams(id(), fin_received(),
                                        &received_trailers(), capture_mode);
      });

  *headers = received_trailers().Clone();
  *frame_len = trailing_headers_frame_len_;

  MarkTrailersConsumed();
  return true;
}

void QuicChromiumClientStream::NotifyHandleOfDataAvailableLater() {
  DCHECK(handle_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicChromiumClientStream::NotifyHandleOfDataAvailable,
                     weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientStream::NotifyHandleOfDataAvailable() {
  if (handle_)
    handle_->OnDataAvailable();
}

void QuicChromiumClientStream::DisableConnectionMigrationToCellularNetwork() {
  can_migrate_to_cellular_network_ = false;
}

quic::QuicPacketLength
QuicChromiumClientStream::GetGuaranteedLargestMessagePayload() const {
  if (!session()) {
    return 0;
  }
  return session()->GetGuaranteedLargestMessagePayload();
}

bool QuicChromiumClientStream::IsFirstStream() {
  return id() == quic::QuicUtils::GetFirstBidirectionalStreamId(
                     quic_version_, quic::Perspective::IS_CLIENT);
}

}  // namespace net
