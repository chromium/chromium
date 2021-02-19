// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/quic_transport.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/quic/platform/impl/quic_mem_slice_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice_span.h"
#include "net/third_party/quiche/src/quic/quic_transport/quic_transport_stream.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/quic_transport.mojom.h"

namespace network {

namespace {

net::QuicTransportClient::Parameters CreateParameters(
    const std::vector<mojom::QuicTransportCertificateFingerprintPtr>&
        fingerprints) {
  net::QuicTransportClient::Parameters params;

  for (const auto& fingerprint : fingerprints) {
    params.server_certificate_fingerprints.push_back(
        quic::CertificateFingerprint{.algorithm = fingerprint->algorithm,
                                     .fingerprint = fingerprint->fingerprint});
  }
  return params;
}

}  // namespace

class QuicTransport::Stream final {
 public:
  class StreamVisitor final : public quic::QuicTransportStream::Visitor {
   public:
    explicit StreamVisitor(Stream* stream)
        : stream_(stream->weak_factory_.GetWeakPtr()) {}
    ~StreamVisitor() override {
      if (stream_) {
        if (stream_->incoming_) {
          stream_->writable_watcher_.Cancel();
          stream_->writable_.reset();
          stream_->transport_->client_->OnIncomingStreamClosed(
              stream_->id_,
              /*fin_received=*/false);
          stream_->incoming_ = nullptr;
        }
        if (stream_->outgoing_) {
          stream_->readable_watcher_.Cancel();
          stream_->readable_.reset();
          stream_->outgoing_ = nullptr;
        }
        stream_->MayDisposeLater();
      }
    }

    // Visitor implementation:
    void OnCanRead() override {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Stream::Receive, stream_));
    }
    void OnFinRead() override {
      if (stream_) {
        stream_->OnFinRead();
      }
    }
    void OnCanWrite() override {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Stream::Send, stream_));
    }

   private:
    const base::WeakPtr<Stream> stream_;
  };

  // Bidirectional
  Stream(QuicTransport* transport,
         quic::QuicTransportStream* stream,
         mojo::ScopedDataPipeConsumerHandle readable,
         mojo::ScopedDataPipeProducerHandle writable)
      : transport_(transport),
        id_(stream->id()),
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
  Stream(QuicTransport* transport,
         quic::QuicTransportStream* outgoing,
         mojo::ScopedDataPipeConsumerHandle readable)
      : transport_(transport),
        id_(outgoing->id()),
        outgoing_(outgoing),
        readable_(std::move(readable)),
        readable_watcher_(FROM_HERE, ArmingPolicy::MANUAL),
        writable_watcher_(FROM_HERE, ArmingPolicy::MANUAL) {
    DCHECK(outgoing_);
    DCHECK(readable_);
    Init();
  }

  // Unidirectional: incoming
  Stream(QuicTransport* transport,
         quic::QuicTransportStream* incoming,
         mojo::ScopedDataPipeProducerHandle writable)
      : transport_(transport),
        id_(incoming->id()),
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

  void Abort(quic::QuicRstStreamErrorCode code) {
    auto* stream = incoming_ ? incoming_ : outgoing_;
    if (!stream) {
      return;
    }
    stream->Reset(code);
    incoming_ = nullptr;
    outgoing_ = nullptr;
    readable_watcher_.Cancel();
    readable_.reset();
    MayDisposeLater();
  }

  ~Stream() {
    auto* stream = incoming_ ? incoming_ : outgoing_;
    if (!stream) {
      return;
    }
    stream->Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
  }

 private:
  using ArmingPolicy = mojo::SimpleWatcher::ArmingPolicy;

  void Init() {
    if (outgoing_) {
      DCHECK(readable_);
      outgoing_->set_visitor(std::make_unique<StreamVisitor>(this));
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
        incoming_->set_visitor(std::make_unique<StreamVisitor>(this));
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
    while (outgoing_ && outgoing_->CanWrite()) {
      const void* data = nullptr;
      uint32_t available = 0;
      MojoResult result = readable_->BeginReadData(
          &data, &available, MOJO_BEGIN_READ_DATA_FLAG_NONE);
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

      bool send_result = outgoing_->Write(
          absl::string_view(reinterpret_cast<const char*>(data), available));
      if (!send_result) {
        // TODO(yhirano): Handle this failure.
        readable_->EndReadData(0);
        return;
      }
      readable_->EndReadData(available);
    }
  }

  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state) {
    DCHECK_EQ(result, MOJO_RESULT_OK);
    Receive();
  }

  void MaySendFin() {
    if (!outgoing_) {
      return;
    }
    if (!has_seen_end_of_pipe_for_readable_ || !has_received_fin_from_client_) {
      return;
    }
    if (outgoing_->SendFin()) {
      outgoing_ = nullptr;
      readable_watcher_.Cancel();
      readable_.reset();
      MayDisposeLater();
    }
    // Otherwise, retry in Send().
  }

  void Receive() {
    while (incoming_ && incoming_->ReadableBytes() > 0) {
      void* buffer = nullptr;
      uint32_t available = 0;
      base::AutoReset<bool> auto_reset(&in_two_phase_write_, true);
      MojoResult result = writable_->BeginWriteData(
          &buffer, &available, MOJO_BEGIN_WRITE_DATA_FLAG_NONE);
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

      const size_t num_read_bytes =
          incoming_->Read(reinterpret_cast<char*>(buffer), available);
      writable_->EndWriteData(num_read_bytes);
      if (!incoming_) {
        // |incoming_| can be null here, because OnFinRead can be called in
        // QuicTransportStream::Read.
        writable_watcher_.Cancel();
        writable_.reset();
        MayDisposeLater();
        return;
      }
    }
  }

  void OnFinRead() {
    incoming_ = nullptr;
    transport_->client_->OnIncomingStreamClosed(id_, /*fin_received=*/true);
    if (in_two_phase_write_) {
      return;
    }
    writable_watcher_.Cancel();
    writable_.reset();
  }

  void Dispose() {
    transport_->streams_.erase(id_);
    // Deletes |this|.
  }
  void MayDisposeLater() {
    if (outgoing_ || incoming_) {
      return;
    }

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Stream::Dispose, weak_factory_.GetWeakPtr()));
  }

  QuicTransport* const transport_;  // outlives |this|.
  const uint32_t id_;
  // |outgoing_| and |incoming_| point to the same stream when this is a
  // bidirectional stream. They are owned by |transport_| (via
  // quic::QuicSession), and the properties will be null-set when the streams
  // are gone (via StreamVisitor).
  quic::QuicTransportStream* outgoing_ = nullptr;
  quic::QuicTransportStream* incoming_ = nullptr;
  mojo::ScopedDataPipeConsumerHandle readable_;  // for |outgoing|
  mojo::ScopedDataPipeProducerHandle writable_;  // for |incoming|

  mojo::SimpleWatcher readable_watcher_;
  mojo::SimpleWatcher writable_watcher_;

  bool in_two_phase_write_ = false;
  bool has_seen_end_of_pipe_for_readable_ = false;
  bool has_received_fin_from_client_ = false;

  // This must be the last member.
  base::WeakPtrFactory<Stream> weak_factory_{this};
};  // namespace network

QuicTransport::QuicTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkIsolationKey& key,
    const std::vector<mojom::QuicTransportCertificateFingerprintPtr>&
        fingerprints,
    NetworkContext* context,
    mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client)
    : transport_(std::make_unique<net::QuicTransportClient>(
          url,
          origin,
          this,
          key,
          context->url_request_context(),
          CreateParameters(fingerprints))),
      context_(context),
      receiver_(this),
      handshake_client_(std::move(handshake_client)) {
  handshake_client_.set_disconnect_handler(
      base::BindOnce(&QuicTransport::Dispose, base::Unretained(this)));

  transport_->Connect();
}

QuicTransport::~QuicTransport() = default;

void QuicTransport::SendDatagram(base::span<const uint8_t> data,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!torn_down_);

  datagram_callbacks_.emplace(std::move(callback));

  auto buffer = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buffer->data(), data.data(), data.size());
  quic::QuicMemSlice slice(
      quic::QuicMemSliceImpl(std::move(buffer), data.size()));
  transport_->session()->datagram_queue()->SendOrQueueDatagram(
      std::move(slice));
}

void QuicTransport::CreateStream(
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

  quic::QuicTransportClientSession* const session = transport_->session();

  if (writable) {
    // Bidirectional
    if (!session->CanOpenNextOutgoingBidirectionalStream()) {
      // TODO(crbug.com/104236): Instead of rejecting the creation request, we
      // should wait in this case.
      std::move(callback).Run(false, 0);
      return;
    }
    quic::QuicTransportStream* const stream =
        session->OpenOutgoingBidirectionalStream();
    DCHECK(stream);
    streams_.insert(std::make_pair(
        stream->id(),
        std::make_unique<Stream>(this, stream, std::move(readable),
                                 std::move(writable))));
    std::move(callback).Run(true, stream->id());
    return;
  }

  // Unidirectional
  if (!session->CanOpenNextOutgoingUnidirectionalStream()) {
    // TODO(crbug.com/104236): Instead of rejecting the creation request, we
    // should wait in this case.
    std::move(callback).Run(false, 0);
    return;
  }

  quic::QuicTransportStream* const stream =
      session->OpenOutgoingUnidirectionalStream();
  DCHECK(stream);
  streams_.insert(std::make_pair(
      stream->id(),
      std::make_unique<Stream>(this, stream, std::move(readable))));
  std::move(callback).Run(true, stream->id());
}

void QuicTransport::AcceptBidirectionalStream(
    BidirectionalStreamAcceptanceCallback acceptance) {
  bidirectional_stream_acceptances_.push(std::move(acceptance));

  OnIncomingBidirectionalStreamAvailable();
}

void QuicTransport::AcceptUnidirectionalStream(
    UnidirectionalStreamAcceptanceCallback acceptance) {
  unidirectional_stream_acceptances_.push(std::move(acceptance));

  OnIncomingUnidirectionalStreamAvailable();
}

void QuicTransport::SendFin(uint32_t stream) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  it->second->NotifyFinFromClient();
}

void QuicTransport::AbortStream(uint32_t stream, uint64_t code) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  auto code_to_pass = quic::QuicRstStreamErrorCode::QUIC_STREAM_NO_ERROR;
  if (code < quic::QuicRstStreamErrorCode::QUIC_STREAM_LAST_ERROR) {
    code_to_pass = static_cast<quic::QuicRstStreamErrorCode>(code);
  }
  it->second->Abort(code_to_pass);
}

void QuicTransport::SetOutgoingDatagramExpirationDuration(
    base::TimeDelta duration) {
  if (torn_down_) {
    return;
  }

  transport_->session()->datagram_queue()->SetMaxTimeInQueue(
      quic::QuicTime::Delta::FromMicroseconds(duration.InMicroseconds()));
}

void QuicTransport::OnConnected() {
  if (torn_down_) {
    return;
  }

  DCHECK(handshake_client_);

  handshake_client_->OnConnectionEstablished(
      receiver_.BindNewPipeAndPassRemote(),
      client_.BindNewPipeAndPassReceiver());

  handshake_client_.reset();
  client_.set_disconnect_handler(
      base::BindOnce(&QuicTransport::Dispose, base::Unretained(this)));
}

void QuicTransport::OnConnectionFailed() {
  if (torn_down_) {
    return;
  }

  DCHECK(handshake_client_);

  // Here we assume that the error is not going to handed to the
  // initiator renderer.
  handshake_client_->OnHandshakeFailed(transport_->error());

  TearDown();
}

void QuicTransport::OnClosed() {
  if (torn_down_) {
    return;
  }

  DCHECK(!handshake_client_);

  TearDown();
}

void QuicTransport::OnError() {
  if (torn_down_) {
    return;
  }

  DCHECK(!handshake_client_);

  TearDown();
}

void QuicTransport::OnIncomingBidirectionalStreamAvailable() {
  DCHECK(!handshake_client_);
  DCHECK(client_);

  while (!bidirectional_stream_acceptances_.empty()) {
    quic::QuicTransportStream* const stream =
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
      stream->Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
      // TODO(yhirano): Error the entire connection.
      return;
    }
    if (mojo::CreateDataPipe(&options, writable_for_incoming,
                             readable_for_incoming) != MOJO_RESULT_OK) {
      stream->Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
      // TODO(yhirano): Error the entire connection.
      return;
    }

    streams_.insert(std::make_pair(
        stream->id(),
        std::make_unique<Stream>(this, stream, std::move(readable_for_outgoing),
                                 std::move(writable_for_incoming))));
    std::move(acceptance)
        .Run(stream->id(), std::move(readable_for_incoming),
             std::move(writable_for_outgoing));
  }
}

void QuicTransport::OnIncomingUnidirectionalStreamAvailable() {
  DCHECK(!handshake_client_);
  DCHECK(client_);

  while (!unidirectional_stream_acceptances_.empty()) {
    quic::QuicTransportStream* const stream =
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
      stream->Reset(quic::QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
      // TODO(yhirano): Error the entire connection.
      return;
    }

    streams_.insert(std::make_pair(
        stream->id(), std::make_unique<Stream>(
                          this, stream, std::move(writable_for_incoming))));
    std::move(acceptance).Run(stream->id(), std::move(readable_for_incoming));
  }
}

void QuicTransport::OnDatagramReceived(base::StringPiece datagram) {
  if (torn_down_) {
    return;
  }

  client_->OnDatagramReceived(base::make_span(
      reinterpret_cast<const uint8_t*>(datagram.data()), datagram.size()));
}

void QuicTransport::OnCanCreateNewOutgoingBidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void QuicTransport::OnCanCreateNewOutgoingUnidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void QuicTransport::OnDatagramProcessed(
    base::Optional<quic::MessageStatus> status) {
  DCHECK(!datagram_callbacks_.empty());

  std::move(datagram_callbacks_.front())
      .Run(status == quic::MESSAGE_STATUS_SUCCESS);
  datagram_callbacks_.pop();
}

void QuicTransport::TearDown() {
  torn_down_ = true;
  receiver_.reset();
  handshake_client_.reset();
  client_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicTransport::Dispose, weak_factory_.GetWeakPtr()));
}

void QuicTransport::Dispose() {
  receiver_.reset();

  context_->Remove(this);
  // |this| is deleted.
}

}  // namespace network
