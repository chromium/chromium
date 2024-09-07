// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/web_transport.h"

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_mem_slice.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/web_transport.mojom.h"

namespace network {

namespace {

net::WebTransportParameters CreateParameters(
    const std::vector<mojom::WebTransportCertificateFingerprintPtr>&
        fingerprints) {
  net::WebTransportParameters params;
  params.enable_web_transport_http3 = true;

  for (const auto& fingerprint : fingerprints) {
    params.server_certificate_fingerprints.push_back(
        quic::CertificateFingerprint{.algorithm = fingerprint->algorithm,
                                     .fingerprint = fingerprint->fingerprint});
  }
  return params;
}

base::TimeDelta ToTimeDelta(absl::Duration duration) {
  return base::Microseconds(absl::ToInt64Microseconds(duration));
}

mojom::WebTransportStatsPtr StatsToMojom(
    const webtransport::SessionStats& stats) {
  mojom::WebTransportStatsPtr result = mojom::WebTransportStats::New();
  result->timestamp = base::Time::Now();
  result->min_rtt = ToTimeDelta(stats.min_rtt);
  result->smoothed_rtt = ToTimeDelta(stats.smoothed_rtt);
  result->rtt_variation = ToTimeDelta(stats.rtt_variation);
  result->estimated_send_rate_bps = stats.estimated_send_rate_bps;
  result->datagrams_expired_outgoing = stats.datagram_stats.expired_outgoing;
  result->datagrams_lost_outgoing = stats.datagram_stats.lost_outgoing;
  return result;
}

}  // namespace

class WebTransport::Stream final {
 public:
  class StreamVisitor final : public quic::WebTransportStreamVisitor {
   public:
    explicit StreamVisitor(Stream* stream)
        : stream_(stream->weak_factory_.GetWeakPtr()) {}
    ~StreamVisitor() override {
      Stream* stream = stream_.get();
      if (!stream) {
        return;
      }
      if (stream->incoming_) {
        stream->writable_watcher_.Cancel();
        stream->writable_.reset();
        if (stream->transport_->client_) {
          stream->transport_->client_->OnIncomingStreamClosed(
              stream->id_,
              /*fin_received=*/false);
        }
        stream->incoming_ = nullptr;
      }
      if (stream->outgoing_) {
        stream->readable_watcher_.Cancel();
        stream->readable_.reset();
        stream->outgoing_ = nullptr;
      }
      stream->MayDisposeLater();
    }

    // Visitor implementation:
    void OnCanRead() override {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Stream::Receive, stream_));
    }
    void OnCanWrite() override {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Stream::Send, stream_));
    }
    void OnResetStreamReceived(quic::WebTransportStreamError error) override {
      if (auto* stream = stream_.get()) {
        stream->OnResetStreamReceived(error);
      }
    }
    void OnStopSendingReceived(quic::WebTransportStreamError error) override {
      if (auto* stream = stream_.get()) {
        stream->OnStopSendingReceived(error);
      }
    }
    void OnWriteSideInDataRecvdState() override {
      if (auto* stream = stream_.get()) {
        stream->OnWriteSideInDataRecvdState();
      }
    }

   private:
    const base::WeakPtr<Stream> stream_;
  };

  // Bidirectional
  Stream(WebTransport* transport,
         quic::WebTransportStream* stream,
         mojo::ScopedDataPipeConsumerHandle readable,
         mojo::ScopedDataPipeProducerHandle writable)
      : transport_(transport),
        id_(stream->GetStreamId()),
        outgoing_(stream),
        incoming_(stream),
        readable_(std::move(readable)),
        writable_(std::move(writable)),
        readable_watcher_(FROM_HERE, ArmingPolicy::MANUAL),
        writable_watcher_(FROM_HERE, ArmingPolicy::MANUAL) {
    DCHECK(outgoing_);
    DCHECK(incoming_);
    DCHECK(readable_);
    DCHECK(writable_);
    Init();
  }

  // Unidirectional: outgoing
  Stream(WebTransport* transport,
         quic::WebTransportStream* outgoing,
         mojo::ScopedDataPipeConsumerHandle readable)
      : transport_(transport),
        id_(outgoing->GetStreamId()),
        outgoing_(outgoing),
        readable_(std::move(readable)),
        readable_watcher_(FROM_HERE, ArmingPolicy::MANUAL),
        writable_watcher_(FROM_HERE, ArmingPolicy::MANUAL) {
    DCHECK(outgoing_);
    DCHECK(readable_);
    Init();
  }

  // Unidirectional: incoming
  Stream(WebTransport* transport,
         quic::WebTransportStream* incoming,
         mojo::ScopedDataPipeProducerHandle writable)
      : transport_(transport),
        id_(incoming->GetStreamId()),
        incoming_(incoming),
        writable_(std::move(writable)),
        readable_watcher_(FROM_HERE, ArmingPolicy::MANUAL),
        writable_watcher_(FROM_HERE, ArmingPolicy::MANUAL) {
    DCHECK(incoming_);
    DCHECK(writable_);
    Init();
  }

  void NotifyFinFromClient() {
    has_received_fin_from_client_ = true;
    MaySendFin();
  }

  void Abort(uint8_t code) {
    if (!outgoing_) {
      return;
    }
    outgoing_->ResetWithUserCode(code);
    outgoing_ = nullptr;
    readable_watcher_.Cancel();
    readable_.reset();
    MayDisposeLater();
  }

  void StopSending(uint8_t code) {
    if (!incoming_) {
      return;
    }
    incoming_->SendStopSending(code);
    incoming_ = nullptr;
    writable_watcher_.Cancel();
    writable_.reset();
    MayDisposeLater();
  }

  ~Stream() {
    auto* stream = incoming_ ? incoming_.get() : outgoing_.get();
    if (!stream || transport_->closing_ || transport_->torn_down_) {
      return;
    }
    stream->MaybeResetDueToStreamObjectGone();
  }

 private:
  using ArmingPolicy = mojo::SimpleWatcher::ArmingPolicy;

  void Init() {
    if (outgoing_) {
      DCHECK(readable_);
      outgoing_->SetVisitor(std::make_unique<StreamVisitor>(this));
      readable_watcher_.Watch(
          readable_.get(),
          MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&Stream::OnReadable, base::Unretained(this)));
      readable_watcher_.ArmOrNotify();
    }

    if (incoming_) {
      DCHECK(writable_);
      if (incoming_ != outgoing_) {
        incoming_->SetVisitor(std::make_unique<StreamVisitor>(this));
      }
      writable_watcher_.Watch(
          writable_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&Stream::OnWritable, base::Unretained(this)));
      writable_watcher_.ArmOrNotify();
    }
  }

  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state) {
    DCHECK_EQ(result, MOJO_RESULT_OK);
    Send();
  }

  void Send() {
    MaySendFin();
    while (readable_ && outgoing_ && outgoing_->CanWrite()) {
      base::span<const uint8_t> data;
      MojoResult result =
          readable_->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, data);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        readable_watcher_.Arm();
        return;
      }
      if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        has_seen_end_of_pipe_for_readable_ = true;
        MaySendFin();
        return;
      }
      DCHECK_EQ(result, MOJO_RESULT_OK);

      bool send_result = outgoing_->Write(base::as_string_view(data));
      if (!send_result) {
        // TODO(yhirano): Handle this failure.
        readable_->EndReadData(0);
        return;
      }
      readable_->EndReadData(data.size());
    }
  }

  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state) {
    Receive();
  }

  void MaySendFin() {
    if (!readable_ || !outgoing_) {
      return;
    }
    if (!has_seen_end_of_pipe_for_readable_ || !has_received_fin_from_client_) {
      return;
    }
    if (outgoing_->SendFin()) {
      // We don't reset `outgoing_` as we want to wait for the ACK signal.
      readable_watcher_.Cancel();
      readable_.reset();
    }
    // Otherwise, retry in Send().
  }

  void Receive() {
    while (incoming_) {
      quic::WebTransportStream::ReadResult read_result;
      if (incoming_->ReadableBytes() > 0) {
        base::span<uint8_t> buffer;
        MojoResult result =
            writable_->BeginWriteData(mojo::DataPipeProducerHandle::kNoSizeHint,
                                      MOJO_BEGIN_WRITE_DATA_FLAG_NONE, buffer);
        if (result == MOJO_RESULT_SHOULD_WAIT) {
          writable_watcher_.Arm();
          return;
        }
        if (result == MOJO_RESULT_FAILED_PRECONDITION) {
          // The client doesn't want further data.
          writable_watcher_.Cancel();
          writable_.reset();
          incoming_ = nullptr;
          MayDisposeLater();
          return;
        }
        DCHECK_EQ(result, MOJO_RESULT_OK);

        base::span<char> chars = base::as_writable_chars(buffer);
        read_result = incoming_->Read(absl::MakeSpan(chars));
        writable_->EndWriteData(read_result.bytes_read);
      } else {
        // Even if ReadableBytes() == 0, we may need to read the FIN at the end
        // of the stream.
        read_result = incoming_->Read(absl::Span<char>());
        if (!read_result.fin) {
          return;
        }
      }
      if (read_result.fin) {
        if (transport_->client_) {
          transport_->client_->OnIncomingStreamClosed(id_,
                                                      /*fin_received=*/true);
        }
        writable_watcher_.Cancel();
        writable_.reset();
        incoming_ = nullptr;
        MayDisposeLater();
        return;
      }
    }
  }

  void OnResetStreamReceived(quic::WebTransportStreamError error) {
    if (transport_->client_) {
      transport_->client_->OnReceivedResetStream(id_, error);
    }
    incoming_ = nullptr;
    writable_watcher_.Cancel();
    writable_.reset();
    MayDisposeLater();
  }

  void OnStopSendingReceived(quic::WebTransportStreamError error) {
    if (transport_->client_) {
      transport_->client_->OnReceivedStopSending(id_, error);
    }
    outgoing_ = nullptr;
    readable_watcher_.Cancel();
    readable_.reset();
    MayDisposeLater();
  }

  void OnWriteSideInDataRecvdState() {
    if (transport_->client_) {
      transport_->client_->OnOutgoingStreamClosed(id_);
    }

    outgoing_ = nullptr;
    readable_watcher_.Cancel();
    readable_.reset();
    MayDisposeLater();
  }

  void Dispose() {
    transport_->streams_.erase(id_);
    // Deletes |this|.
  }

  void MayDisposeLater() {
    if (outgoing_ || incoming_) {
      return;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Stream::Dispose, weak_factory_.GetWeakPtr()));
  }

  const raw_ptr<WebTransport> transport_;  // outlives |this|.
  const uint32_t id_;
  // |outgoing_| and |incoming_| point to the same stream when this is a
  // bidirectional stream. They are owned by |transport_| (via
  // quic::QuicSession), and the properties will be null-set when the streams
  // are gone (via StreamVisitor).
  raw_ptr<quic::WebTransportStream> outgoing_ = nullptr;
  raw_ptr<quic::WebTransportStream> incoming_ = nullptr;
  mojo::ScopedDataPipeConsumerHandle readable_;  // for |outgoing|
  mojo::ScopedDataPipeProducerHandle writable_;  // for |incoming|

  mojo::SimpleWatcher readable_watcher_;
  mojo::SimpleWatcher writable_watcher_;

  bool has_seen_end_of_pipe_for_readable_ = false;
  bool has_received_fin_from_client_ = false;

  // This must be the last member.
  base::WeakPtrFactory<Stream> weak_factory_{this};
};

WebTransport::WebTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkAnonymizationKey& key,
    const std::vector<mojom::WebTransportCertificateFingerprintPtr>&
        fingerprints,
    NetworkContext* context,
    mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client)
    : transport_(net::CreateWebTransportClient(url,
                                               origin,
                                               this,
                                               key,
                                               context->url_request_context(),
                                               CreateParameters(fingerprints))),
      context_(context),
      receiver_(this),
      handshake_client_(std::move(handshake_client)) {
  handshake_client_.set_disconnect_handler(
      base::BindOnce(&WebTransport::Dispose, base::Unretained(this)));

  transport_->Connect();
}

WebTransport::~WebTransport() {
  // Ensure that we ignore all callbacks while mid-destruction.
  torn_down_ = true;
}

void WebTransport::SendDatagram(base::span<const uint8_t> data,
                                base::OnceCallback<void(bool)> callback) {
  DCHECK(!torn_down_);

  datagram_callbacks_.emplace(std::move(callback));

  CHECK(transport_->session());
  transport_->session()->SendOrQueueDatagram(base::as_string_view(data));
}

void WebTransport::CreateStream(
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable,
    base::OnceCallback<void(bool, uint32_t)> callback) {
  // |readable| is non-nullable, |writable| is nullable.
  DCHECK(readable);

  if (handshake_client_) {
    // Invalid request.
    std::move(callback).Run(false, 0);
    return;
  }

  quic::WebTransportSession* const session = transport_->session();
  CHECK(session);

  if (writable) {
    // Bidirectional
    if (!session->CanOpenNextOutgoingBidirectionalStream()) {
      // TODO(crbug.com/40114825): Instead of rejecting the creation request, we
      // should wait in this case.
      std::move(callback).Run(false, 0);
      return;
    }
    quic::WebTransportStream* const stream =
        session->OpenOutgoingBidirectionalStream();
    DCHECK(stream);
    streams_.insert(std::make_pair(
        stream->GetStreamId(),
        std::make_unique<Stream>(this, stream, std::move(readable),
                                 std::move(writable))));
    std::move(callback).Run(true, stream->GetStreamId());
    return;
  }

  // Unidirectional
  if (!session->CanOpenNextOutgoingUnidirectionalStream()) {
    // TODO(crbug.com/40114825): Instead of rejecting the creation request, we
    // should wait in this case.
    std::move(callback).Run(false, 0);
    return;
  }

  quic::WebTransportStream* const stream =
      session->OpenOutgoingUnidirectionalStream();
  DCHECK(stream);
  streams_.insert(std::make_pair(
      stream->GetStreamId(),
      std::make_unique<Stream>(this, stream, std::move(readable))));
  std::move(callback).Run(true, stream->GetStreamId());
}

void WebTransport::AcceptBidirectionalStream(
    BidirectionalStreamAcceptanceCallback acceptance) {
  bidirectional_stream_acceptances_.push(std::move(acceptance));

  OnIncomingBidirectionalStreamAvailable();
}

void WebTransport::AcceptUnidirectionalStream(
    UnidirectionalStreamAcceptanceCallback acceptance) {
  unidirectional_stream_acceptances_.push(std::move(acceptance));

  OnIncomingUnidirectionalStreamAvailable();
}

void WebTransport::SendFin(uint32_t stream) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  it->second->NotifyFinFromClient();
}

void WebTransport::AbortStream(uint32_t stream, uint8_t code) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  it->second->Abort(code);
}

void WebTransport::StopSending(uint32_t stream, uint8_t code) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  it->second->StopSending(code);
}

void WebTransport::SetOutgoingDatagramExpirationDuration(
    base::TimeDelta duration) {
  if (torn_down_ || closing_) {
    return;
  }

  CHECK(transport_->session());
  transport_->session()->SetDatagramMaxTimeInQueue(
      absl::Microseconds(duration.InMicroseconds()));
}

void WebTransport::Close(mojom::WebTransportCloseInfoPtr close_info) {
  if (torn_down_ || closing_) {
    return;
  }
  closing_ = true;

  receiver_.reset();
  handshake_client_.reset();
  client_.reset();

  std::optional<net::WebTransportCloseInfo> close_info_to_pass;
  if (close_info) {
    close_info_to_pass =
        std::make_optional<net::WebTransportCloseInfo>(close_info->code, "");

    // As described at
    // https://w3c.github.io/webtransport/#dom-webtransport-close,
    // the size of the reason string must not exceed 1024.
    constexpr size_t kMaxSize = 1024;
    if (close_info->reason.size() > kMaxSize) {
      base::TruncateUTF8ToByteSize(close_info->reason, kMaxSize,
                                   &close_info_to_pass->reason);
    } else {
      close_info_to_pass->reason = std::move(close_info->reason);
    }
  }

  transport_->Close(close_info_to_pass);
}

void WebTransport::OnConnected(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(handshake_client_);

  handshake_client_->OnConnectionEstablished(
      receiver_.BindNewPipeAndPassRemote(),
      client_.BindNewPipeAndPassReceiver(), std::move(response_headers),
      StatsToMojom(transport_->session()->GetSessionStats()));

  handshake_client_.reset();
  // We set the disconnect handler for `receiver_`, not `client_`, in order
  // to make the closing sequence consistent: The client calls Close() and
  // then resets the mojo endpoints.
  receiver_.set_disconnect_handler(
      base::BindOnce(&WebTransport::Dispose, base::Unretained(this)));
}

void WebTransport::OnConnectionFailed(const net::WebTransportError& error) {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(handshake_client_);

  // Here we assume that the error is not going to handed to the
  // initiator renderer.
  handshake_client_->OnHandshakeFailed(error);

  TearDown();
}

void WebTransport::OnClosed(
    const std::optional<net::WebTransportCloseInfo>& close_info) {
  if (torn_down_) {
    return;
  }

  DCHECK(!handshake_client_);
  if (closing_) {
    closing_ = false;
  } else {
    mojom::WebTransportCloseInfoPtr close_info_to_pass;
    if (close_info) {
      close_info_to_pass = mojom::WebTransportCloseInfo::New(
          close_info->code, close_info->reason);
    }
    mojom::WebTransportStatsPtr final_stats;
    if (transport_ != nullptr && transport_->session() != nullptr) {
      final_stats = StatsToMojom(transport_->session()->GetSessionStats());
    }
    client_->OnClosed(std::move(close_info_to_pass), std::move(final_stats));
  }

  TearDown();
}

void WebTransport::OnError(const net::WebTransportError& error) {
  if (torn_down_) {
    return;
  }

  if (closing_) {
    closing_ = false;
  }

  DCHECK(!handshake_client_);

  TearDown();
}

void WebTransport::OnIncomingBidirectionalStreamAvailable() {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(!handshake_client_);
  DCHECK(client_);

  while (!bidirectional_stream_acceptances_.empty()) {
    CHECK(transport_->session());
    quic::WebTransportStream* const stream =
        transport_->session()->AcceptIncomingBidirectionalStream();
    if (!stream) {
      return;
    }
    auto acceptance = std::move(bidirectional_stream_acceptances_.front());
    bidirectional_stream_acceptances_.pop();

    mojo::ScopedDataPipeConsumerHandle readable_for_outgoing;
    mojo::ScopedDataPipeProducerHandle writable_for_outgoing;
    mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
    mojo::ScopedDataPipeProducerHandle writable_for_incoming;
    const MojoCreateDataPipeOptions options = {
        sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 256 * 1024};
    if (mojo::CreateDataPipe(&options, writable_for_outgoing,
                             readable_for_outgoing) != MOJO_RESULT_OK) {
      stream->ResetDueToInternalError();
      // TODO(yhirano): Error the entire connection.
      return;
    }
    if (mojo::CreateDataPipe(&options, writable_for_incoming,
                             readable_for_incoming) != MOJO_RESULT_OK) {
      stream->ResetDueToInternalError();
      // TODO(yhirano): Error the entire connection.
      return;
    }

    streams_.insert(std::make_pair(
        stream->GetStreamId(),
        std::make_unique<Stream>(this, stream, std::move(readable_for_outgoing),
                                 std::move(writable_for_incoming))));
    std::move(acceptance)
        .Run(stream->GetStreamId(), std::move(readable_for_incoming),
             std::move(writable_for_outgoing));
  }
}

void WebTransport::OnIncomingUnidirectionalStreamAvailable() {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(!handshake_client_);
  DCHECK(client_);

  while (!unidirectional_stream_acceptances_.empty()) {
    CHECK(transport_->session());
    quic::WebTransportStream* const stream =
        transport_->session()->AcceptIncomingUnidirectionalStream();

    if (!stream) {
      return;
    }
    auto acceptance = std::move(unidirectional_stream_acceptances_.front());
    unidirectional_stream_acceptances_.pop();

    mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
    mojo::ScopedDataPipeProducerHandle writable_for_incoming;
    const MojoCreateDataPipeOptions options = {
        sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 256 * 1024};
    if (mojo::CreateDataPipe(&options, writable_for_incoming,
                             readable_for_incoming) != MOJO_RESULT_OK) {
      stream->ResetDueToInternalError();
      // TODO(yhirano): Error the entire connection.
      return;
    }

    streams_.insert(
        std::make_pair(stream->GetStreamId(),
                       std::make_unique<Stream>(
                           this, stream, std::move(writable_for_incoming))));
    std::move(acceptance)
        .Run(stream->GetStreamId(), std::move(readable_for_incoming));
  }
}

void WebTransport::OnDatagramReceived(std::string_view datagram) {
  if (torn_down_ || closing_) {
    return;
  }

  client_->OnDatagramReceived(base::make_span(
      reinterpret_cast<const uint8_t*>(datagram.data()), datagram.size()));
}

void WebTransport::OnCanCreateNewOutgoingBidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void WebTransport::OnCanCreateNewOutgoingUnidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void WebTransport::OnDatagramProcessed(
    std::optional<quic::MessageStatus> status) {
  DCHECK(!datagram_callbacks_.empty());

  std::move(datagram_callbacks_.front())
      .Run(status == quic::MESSAGE_STATUS_SUCCESS);
  datagram_callbacks_.pop();
}

void WebTransport::GetStats(GetStatsCallback callback) {
  webtransport::Session* const session = transport_->session();

  if (torn_down_ || session == nullptr) {
    std::move(callback).Run(nullptr);
    return;
  }

  webtransport::SessionStats stats = session->GetSessionStats();
  std::move(callback).Run(StatsToMojom(stats));
}

void WebTransport::TearDown() {
  torn_down_ = true;
  receiver_.reset();
  handshake_client_.reset();
  client_.reset();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebTransport::Dispose, weak_factory_.GetWeakPtr()));
}

void WebTransport::Dispose() {
  receiver_.reset();

  context_->Remove(this);
  // |this| is deleted.
}

}  // namespace network
