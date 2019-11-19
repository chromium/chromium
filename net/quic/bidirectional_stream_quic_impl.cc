// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/bidirectional_stream_quic_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_util.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "quic_http_stream.h"

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
  bool* var_;
  bool old_val_;
};
}  // namespace

BidirectionalStreamQuicImpl::BidirectionalStreamQuicImpl(
    std::unique_ptr<QuicChromiumClientSession::Handle> session)
    : session_(std::move(session)),
      stream_(nullptr),
      request_info_(nullptr),
      delegate_(nullptr),
      response_status_(OK),
      negotiated_protocol_(kProtoUnknown),
      read_buffer_len_(0),
      headers_bytes_received_(0),
      headers_bytes_sent_(0),
      closed_stream_received_bytes_(0),
      closed_stream_sent_bytes_(0),
      closed_is_first_stream_(false),
      has_sent_headers_(false),
      send_request_headers_automatically_(true),
      may_invoke_callbacks_(true) {}

BidirectionalStreamQuicImpl::~BidirectionalStreamQuicImpl() {
  if (stream_) {
    delegate_ = nullptr;
    stream_->Reset(quic::QUIC_STREAM_CANCELLED);
  }
}

void BidirectionalStreamQuicImpl::Start(
    const BidirectionalStreamRequestInfo* request_info,
    const NetLogWithSource& net_log,
    bool send_request_headers_automatically,
    BidirectionalStreamImpl::Delegate* delegate,
    std::unique_ptr<base::OneShotTimer> timer,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  DCHECK(!stream_);
  CHECK(delegate);
  DLOG_IF(WARNING, !session_->IsConnected())
      << "Trying to start request headers after session has been closed.";

  net_log.AddEventReferencingSource(
      NetLogEventType::BIDIRECTIONAL_STREAM_BOUND_TO_QUIC_SESSION,
      session_->net_log().source());

  send_request_headers_automatically_ = send_request_headers_automatically;
  delegate_ = delegate;
  request_info_ = request_info;

  // Only allow SAFE methods to use early data, unless overriden by the caller.
  bool use_early_data = !HttpUtil::IsMethodSafe(request_info_->method);
  use_early_data |= request_info_->allow_early_data_override;

  int rv = session_->RequestStream(
      use_early_data,
      base::BindOnce(&BidirectionalStreamQuicImpl::OnStreamReady,
                     weak_factory_.GetWeakPtr()),
      traffic_annotation);
  if (rv == ERR_IO_PENDING)
    return;

  if (rv != OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::NotifyError,
                                  weak_factory_.GetWeakPtr(),
                                  session_->IsCryptoHandshakeConfirmed()
                                      ? rv
                                      : ERR_QUIC_HANDSHAKE_FAILED));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::OnStreamReady,
                                weak_factory_.GetWeakPtr(), rv));
}

void BidirectionalStreamQuicImpl::SendRequestHeaders() {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  int rv = WriteHeaders();
  if (rv < 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::NotifyError,
                                  weak_factory_.GetWeakPtr(), rv));
  }
}

int BidirectionalStreamQuicImpl::WriteHeaders() {
  DCHECK(!has_sent_headers_);

  spdy::SpdyHeaderBlock headers;
  HttpRequestInfo http_request_info;
  http_request_info.url = request_info_->url;
  http_request_info.method = request_info_->method;
  http_request_info.extra_headers = request_info_->extra_headers;

  CreateSpdyHeadersFromHttpRequest(http_request_info,
                                   http_request_info.extra_headers, &headers);
  int rv = stream_->WriteHeaders(std::move(headers),
                                 request_info_->end_stream_on_headers, nullptr);
  if (rv >= 0) {
    headers_bytes_sent_ += rv;
    has_sent_headers_ = true;
  }
  return rv;
}

int BidirectionalStreamQuicImpl::ReadData(IOBuffer* buffer, int buffer_len) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  DCHECK(buffer);
  DCHECK(buffer_len);

  int rv = stream_->ReadBody(
      buffer, buffer_len,
      base::BindOnce(&BidirectionalStreamQuicImpl::OnReadDataComplete,
                     weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    read_buffer_ = buffer;
    read_buffer_len_ = buffer_len;
    return ERR_IO_PENDING;
  }

  if (rv < 0)
    return rv;

  // If the write side is closed, OnFinRead() will call
  // BidirectionalStreamQuicImpl::OnClose().
  if (stream_->IsDoneReading())
    stream_->OnFinRead();

  return rv;
}

void BidirectionalStreamQuicImpl::SendvData(
    const std::vector<scoped_refptr<IOBuffer>>& buffers,
    const std::vector<int>& lengths,
    bool end_stream) {
  ScopedBoolSaver saver(&may_invoke_callbacks_, false);
  DCHECK_EQ(buffers.size(), lengths.size());

  if (!stream_->IsOpen()) {
    LOG(ERROR) << "Trying to send data after stream has been closed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::NotifyError,
                                  weak_factory_.GetWeakPtr(), ERR_UNEXPECTED));
    return;
  }

  std::unique_ptr<quic::QuicConnection::ScopedPacketFlusher> bundler(
      session_->CreatePacketBundler());
  if (!has_sent_headers_) {
    DCHECK(!send_request_headers_automatically_);
    int rv = WriteHeaders();
    if (rv < 0) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::NotifyError,
                                    weak_factory_.GetWeakPtr(), rv));
      return;
    }
  }

  int rv = stream_->WritevStreamData(
      buffers, lengths, end_stream,
      base::BindOnce(&BidirectionalStreamQuicImpl::OnSendDataComplete,
                     weak_factory_.GetWeakPtr()));

  if (rv != ERR_IO_PENDING) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BidirectionalStreamQuicImpl::OnSendDataComplete,
                       weak_factory_.GetWeakPtr(), rv));
  }
}

NextProto BidirectionalStreamQuicImpl::GetProtocol() const {
  return negotiated_protocol_;
}

int64_t BidirectionalStreamQuicImpl::GetTotalReceivedBytes() const {
  // When QPACK is enabled, headers are sent and received on the stream, so
  // the headers bytes do not need to be accounted for independently.
  int64_t total_received_bytes =
      quic::VersionUsesHttp3(session_->GetQuicVersion().transport_version)
          ? 0
          : headers_bytes_received_;
  if (stream_) {
    DCHECK_LE(stream_->NumBytesConsumed(), stream_->stream_bytes_read());
    // Only count the uniquely received bytes.
    total_received_bytes += stream_->NumBytesConsumed();
  } else {
    total_received_bytes += closed_stream_received_bytes_;
  }
  return total_received_bytes;
}

int64_t BidirectionalStreamQuicImpl::GetTotalSentBytes() const {
  // When QPACK is enabled, headers are sent and received on the stream, so
  // the headers bytes do not need to be accounted for independently.
  int64_t total_sent_bytes =
      quic::VersionUsesHttp3(session_->GetQuicVersion().transport_version)
          ? 0
          : headers_bytes_sent_;
  if (stream_) {
    total_sent_bytes += stream_->stream_bytes_written();
  } else {
    total_sent_bytes += closed_stream_sent_bytes_;
  }
  return total_sent_bytes;
}

bool BidirectionalStreamQuicImpl::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  bool is_first_stream = closed_is_first_stream_;
  if (stream_)
    is_first_stream = stream_->IsFirstStream();
  if (is_first_stream) {
    load_timing_info->socket_reused = false;
    load_timing_info->connect_timing = connect_timing_;
  } else {
    load_timing_info->socket_reused = true;
  }
  return true;
}

void BidirectionalStreamQuicImpl::PopulateNetErrorDetails(
    NetErrorDetails* details) {
  DCHECK(details);
  details->connection_info =
      QuicHttpStream::ConnectionInfoFromQuicVersion(session_->GetQuicVersion());
  session_->PopulateNetErrorDetails(details);
  if (session_->IsCryptoHandshakeConfirmed() && stream_)
    details->quic_connection_error = stream_->connection_error();
}

void BidirectionalStreamQuicImpl::OnStreamReady(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(!stream_);
  if (rv != OK) {
    NotifyError(rv);
    return;
  }

  stream_ = session_->ReleaseStream();
  DCHECK(stream_);

  if (!stream_->IsOpen()) {
    NotifyError(ERR_CONNECTION_CLOSED);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BidirectionalStreamQuicImpl::ReadInitialHeaders,
                     weak_factory_.GetWeakPtr()));

  NotifyStreamReady();
}

void BidirectionalStreamQuicImpl::OnSendDataComplete(int rv) {
  CHECK(may_invoke_callbacks_);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    NotifyError(rv);
    return;
  }

  if (delegate_)
    delegate_->OnDataSent();
}

void BidirectionalStreamQuicImpl::OnReadInitialHeadersComplete(int rv) {
  CHECK(may_invoke_callbacks_);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    NotifyError(rv);
    return;
  }

  headers_bytes_received_ += rv;
  negotiated_protocol_ = kProtoQUIC;
  connect_timing_ = session_->GetConnectTiming();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BidirectionalStreamQuicImpl::ReadTrailingHeaders,
                     weak_factory_.GetWeakPtr()));
  if (delegate_)
    delegate_->OnHeadersReceived(initial_headers_);
}

void BidirectionalStreamQuicImpl::ReadInitialHeaders() {
  int rv = stream_->ReadInitialHeaders(
      &initial_headers_,
      base::BindOnce(&BidirectionalStreamQuicImpl::OnReadInitialHeadersComplete,
                     weak_factory_.GetWeakPtr()));

  if (rv != ERR_IO_PENDING)
    OnReadInitialHeadersComplete(rv);
}

void BidirectionalStreamQuicImpl::ReadTrailingHeaders() {
  int rv = stream_->ReadTrailingHeaders(
      &trailing_headers_,
      base::BindOnce(
          &BidirectionalStreamQuicImpl::OnReadTrailingHeadersComplete,
          weak_factory_.GetWeakPtr()));

  if (rv != ERR_IO_PENDING)
    OnReadTrailingHeadersComplete(rv);
}

void BidirectionalStreamQuicImpl::OnReadTrailingHeadersComplete(int rv) {
  CHECK(may_invoke_callbacks_);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    NotifyError(rv);
    return;
  }

  headers_bytes_received_ += rv;

  if (delegate_)
    delegate_->OnTrailersReceived(trailing_headers_);
}

void BidirectionalStreamQuicImpl::OnReadDataComplete(int rv) {
  CHECK(may_invoke_callbacks_);

  read_buffer_ = nullptr;
  read_buffer_len_ = 0;

  // If the write side is closed, OnFinRead() will call
  // BidirectionalStreamQuicImpl::OnClose().
  if (stream_->IsDoneReading())
    stream_->OnFinRead();

  if (!delegate_)
    return;

  if (rv < 0)
    NotifyError(rv);
  else
    delegate_->OnDataRead(rv);
}

void BidirectionalStreamQuicImpl::NotifyError(int error) {
  NotifyErrorImpl(error, /*notify_delegate_later*/ false);
}

void BidirectionalStreamQuicImpl::NotifyErrorImpl(int error,
                                                  bool notify_delegate_later) {
  DCHECK_NE(OK, error);
  DCHECK_NE(ERR_IO_PENDING, error);

  ResetStream();
  if (delegate_) {
    response_status_ = error;
    BidirectionalStreamImpl::Delegate* delegate = delegate_;
    delegate_ = nullptr;
    // Cancel any pending callback.
    weak_factory_.InvalidateWeakPtrs();
    if (notify_delegate_later) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&BidirectionalStreamQuicImpl::NotifyFailure,
                         weak_factory_.GetWeakPtr(), delegate, error));
    } else {
      NotifyFailure(delegate, error);
      // |this| might be destroyed at this point.
    }
  }
}

void BidirectionalStreamQuicImpl::NotifyFailure(
    BidirectionalStreamImpl::Delegate* delegate,
    int error) {
  CHECK(may_invoke_callbacks_);
  delegate->OnFailed(error);
  // |this| might be destroyed at this point.
}

void BidirectionalStreamQuicImpl::NotifyStreamReady() {
  CHECK(may_invoke_callbacks_);
  if (send_request_headers_automatically_) {
    int rv = WriteHeaders();
    if (rv < 0) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BidirectionalStreamQuicImpl::NotifyError,
                                    weak_factory_.GetWeakPtr(), rv));
      return;
    }
  }

  if (delegate_)
    delegate_->OnStreamReady(has_sent_headers_);
}

void BidirectionalStreamQuicImpl::ResetStream() {
  if (!stream_)
    return;
  closed_stream_received_bytes_ = stream_->stream_bytes_read();
  closed_stream_sent_bytes_ = stream_->stream_bytes_written();
  closed_is_first_stream_ = stream_->IsFirstStream();
}

}  // namespace net
