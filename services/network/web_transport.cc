// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_transport.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/network_handle.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "services/network/local_network_access_checker.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/local_network_access_check_result.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-shared.h"
#include "services/network/public/mojom/web_transport.mojom.h"

namespace network {

namespace {

constexpr size_t kMaxFinalReceiveStreamStatsEntries = 512;
constexpr size_t kMaxDatagramsPerPump = 64;
constexpr base::TimeDelta kMinDefaultDatagramExpirationDuration =
    base::Milliseconds(4);

// QuicDatagramQueue computes an expiry with unchecked signed addition. Clamp
// the untrusted renderer duration here to keep that sum representable.
constexpr base::TimeDelta kMaxOutgoingDatagramExpirationDuration =
    base::TimeDelta::Max() / 2;

net::WebTransportParameters CreateParameters(
    const std::vector<mojom::WebTransportCertificateFingerprintPtr>&
        fingerprints,
    std::vector<std::string> application_protocols,
    mojom::WebTransportCongestionControl congestion_control,
    std::optional<uint16_t>
        anticipated_concurrent_incoming_unidirectional_streams,
    std::optional<uint16_t>
        anticipated_concurrent_incoming_bidirectional_streams,
    std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>
        additional_headers) {
  net::WebTransportParameters params;
  params.enable_web_transport_http3 = true;
  params.application_protocols = std::move(application_protocols);

  switch (congestion_control) {
    case mojom::WebTransportCongestionControl::kDefault:
      params.congestion_control_hint =
          net::WebTransportParameters::CongestionControlHint::kDefault;
      break;
    case mojom::WebTransportCongestionControl::kThroughput:
      params.congestion_control_hint =
          net::WebTransportParameters::CongestionControlHint::kThroughput;
      break;
    case mojom::WebTransportCongestionControl::kLowLatency:
      params.congestion_control_hint =
          net::WebTransportParameters::CongestionControlHint::kLowLatency;
      break;
    default:
      NOTREACHED();
  }

  params.anticipated_concurrent_incoming_unidirectional_streams =
      anticipated_concurrent_incoming_unidirectional_streams;
  params.anticipated_concurrent_incoming_bidirectional_streams =
      anticipated_concurrent_incoming_bidirectional_streams;

  for (const auto& fingerprint : fingerprints) {
    params.server_certificate_fingerprints.push_back(
        quic::CertificateFingerprint{.algorithm = fingerprint->algorithm,
                                     .fingerprint = fingerprint->fingerprint});
  }
  params.additional_headers = std::move(additional_headers);
  return params;
}

base::TimeDelta ToTimeDelta(absl::Duration duration) {
  return base::Microseconds(absl::ToInt64Microseconds(duration));
}

webtransport::StreamPriority ToStreamPriority(
    const mojom::WebTransportStreamPriority& p) {
  return {p.send_group_id.value_or(0), p.send_order};
}

mojom::WebTransportStatsPtr StatsToMojom(
    const webtransport::SessionStats& stats,
    uint64_t expired_pending_datagram_count) {
  mojom::WebTransportStatsPtr result = mojom::WebTransportStats::New();
  result->timestamp = base::Time::Now();
  result->min_rtt = ToTimeDelta(stats.min_rtt);
  result->smoothed_rtt = ToTimeDelta(stats.smoothed_rtt);
  result->rtt_variation = ToTimeDelta(stats.rtt_variation);
  result->estimated_send_rate_bps = stats.estimated_send_rate_bps;
  result->datagrams_expired_outgoing = base::ClampAdd(
      stats.datagram_stats.expired_outgoing, expired_pending_datagram_count);
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
        const uint64_t bytes_received = stream->FinalizeReceiveStats();
        stream->writable_watcher_.Cancel();
        stream->writable_.reset();
        if (stream->transport_->client_) {
          stream->transport_->client_->OnIncomingStreamClosed(
              stream->id_, /*fin_received=*/false, bytes_received);
        }
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

  void SetPriority(const webtransport::StreamPriority& priority) {
    if (outgoing_) {
      outgoing_->SetPriority(priority);
    }
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
    auto* incoming = incoming_.get();
    FinalizeReceiveStats();
    incoming->SendStopSending(code);
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

  // Spec bytesReceived: contiguous application bytes received on this stream.
  // This is bytes already forwarded into the Mojo pipe plus bytes still
  // buffered in the QUICHE sequencer, for example due to backpressure.
  uint64_t bytes_received() const {
    return bytes_forwarded_to_data_pipe_ +
           (incoming_ ? incoming_->ReadableBytes() : 0u);
  }

 private:
  uint64_t FinalizeReceiveStats() {
    bytes_forwarded_to_data_pipe_ = bytes_received();
    incoming_ = nullptr;
    return bytes_forwarded_to_data_pipe_;
  }

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
          transport_->final_receive_stream_stats_.Put(id_,
                                                      FinalizeReceiveStats());
          writable_watcher_.Cancel();
          writable_.reset();
          MayDisposeLater();
          return;
        }
        DCHECK_EQ(result, MOJO_RESULT_OK);

        base::span<char> chars = base::as_writable_chars(buffer);
        read_result = incoming_->Read(absl::MakeSpan(chars));
        bytes_forwarded_to_data_pipe_ += read_result.bytes_read;
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
        const uint64_t bytes_received = FinalizeReceiveStats();
        if (transport_->client_) {
          transport_->client_->OnIncomingStreamClosed(
              id_, /*fin_received=*/true, bytes_received);
        }
        writable_watcher_.Cancel();
        writable_.reset();
        MayDisposeLater();
        return;
      }
    }
  }

  void OnResetStreamReceived(quic::WebTransportStreamError error) {
    const uint64_t bytes_received = FinalizeReceiveStats();
    if (transport_->client_) {
      transport_->client_->OnReceivedResetStream(id_, error, bytes_received);
    }
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

  // Bytes removed from the QUICHE sequencer and forwarded to the Mojo pipe.
  uint64_t bytes_forwarded_to_data_pipe_ = 0;

  // This must be the last member.
  base::WeakPtrFactory<Stream> weak_factory_{this};
};

// One participant in the session's Datagram scheduler: either a writable
// created by the renderer through CreateDatagramWritable(), or the writable
// backing the legacy SendDatagram() path.
class WebTransport::DatagramWritable final
    : public mojom::WebTransportDatagramWritable {
 public:
  // Creates the writable used by WebTransport::SendDatagram(). It has no Mojo
  // receiver and lives as long as the session's Datagram state.
  explicit DatagramWritable(WebTransport* transport) : transport_(transport) {}

  // Creates a renderer-owned writable. Closing `receiver` removes it.
  DatagramWritable(
      WebTransport* transport,
      mojo::PendingReceiver<mojom::WebTransportDatagramWritable> receiver,
      const mojom::WebTransportStreamPriority& priority)
      : transport_(transport),
        priority_(ToStreamPriority(priority)),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebTransport::OnDatagramWritableDisconnected,
                       base::Unretained(transport_), base::Unretained(this)));
  }

  ~DatagramWritable() override {
    // Members are destroyed in reverse declaration order, but be explicit: the
    // queued Mojo response callbacks may not be dropped while their pipe is
    // open.
    receiver_.reset();
  }

  // mojom::WebTransportDatagramWritable implementation:
  void SendDatagram(base::span<const uint8_t> data,
                    SendDatagramCallback callback) override {
    transport_->QueueDatagram(this, data, std::move(callback));
  }

  void SetPriority(mojom::WebTransportStreamPriorityPtr priority) override {
    if (transport_->torn_down_ || transport_->closing_) {
      return;
    }
    const webtransport::StreamPriority new_priority =
        ToStreamPriority(*priority);
    if (new_priority != priority_) {
      const bool send_group_changed =
          new_priority.send_group_id != priority_.send_group_id;
      priority_ = new_priority;
      // A writable which moved to another send group competes with a different
      // set of writables, so give it a fresh place in the schedule.
      if (send_group_changed && !pending_datagrams_.empty()) {
        transport_->ScheduleDatagramWritable(this);
      }
    }
    transport_->ScheduleDatagramPump();
  }

  // Closes the Mojo pipe and discards the Datagrams which are still queued. The
  // pipe has to be closed first because the queue owns the pending Mojo
  // response callbacks, which may not be dropped while their pipe is open.
  void Close(uint32_t custom_reason, std::string_view description) {
    if (receiver_.is_bound() && custom_reason != 0) {
      receiver_.ResetWithReason(custom_reason, std::string(description));
    } else {
      receiver_.reset();
    }
    while (!pending_datagrams_.empty()) {
      transport_->AccountForRemovedPendingDatagram(
          pending_datagrams_.front().data.size());
      pending_datagrams_.pop();
    }
  }

  // Higher send_order values are selected first within a send group.
  const webtransport::StreamPriority& priority() const { return priority_; }

  base::queue<PendingDatagram>& pending_datagrams() {
    return pending_datagrams_;
  }

  // Nonempty writables in the same send group share this order. Lower values
  // are selected first to provide round-robin fairness across groups.
  uint64_t group_schedule_order() const { return group_schedule_order_; }
  void set_group_schedule_order(uint64_t order) {
    group_schedule_order_ = order;
  }

  // Breaks ties between equal-priority writables in a group. Lower values are
  // selected first and refreshed after each Datagram is processed.
  uint64_t writable_schedule_order() const { return writable_schedule_order_; }
  void set_writable_schedule_order(uint64_t order) {
    writable_schedule_order_ = order;
  }

 private:
  const raw_ptr<WebTransport> transport_;  // outlives |this|.
  webtransport::StreamPriority priority_;
  // Declared before `receiver_` so that the receiver is destroyed first: see
  // Close().
  base::queue<PendingDatagram> pending_datagrams_;
  uint64_t group_schedule_order_ = 0;
  uint64_t writable_schedule_order_ = 0;
  mojo::Receiver<mojom::WebTransportDatagramWritable> receiver_{this};
};

WebTransport::WebTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkAnonymizationKey& key,
    const std::vector<mojom::WebTransportCertificateFingerprintPtr>&
        fingerprints,
    const std::vector<std::string>& application_protocols,
    mojom::WebTransportCongestionControl congestion_control,
    std::optional<uint16_t>
        anticipated_concurrent_incoming_unidirectional_streams,
    std::optional<uint16_t>
        anticipated_concurrent_incoming_bidirectional_streams,
    std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> additional_headers,
    NetworkContext* context,
    mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojom::ClientSecurityStatePtr client_security_state)
    : transport_(net::CreateWebTransportClient(
          url,
          origin,
          this,
          key,
          // TODO(crbug.com/495684670): Consider exposing this at the network
          // service layer once a need arises.
          net::handles::kInvalidNetworkHandle,
          context->url_request_context(),
          CreateParameters(
              fingerprints,
              std::move(application_protocols),
              congestion_control,
              anticipated_concurrent_incoming_unidirectional_streams,
              anticipated_concurrent_incoming_bidirectional_streams,
              std::move(additional_headers)))),
      url_(url),
      origin_(origin),
      context_(context),
      final_receive_stream_stats_(kMaxFinalReceiveStreamStatsEntries),
      receiver_(this),
      handshake_client_(std::move(handshake_client)),
      url_loader_network_observer_(std::move(url_loader_network_observer)),
      client_security_state_(std::move(client_security_state)) {
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
  if (torn_down_ || closing_) {
    std::move(callback).Run(false);
    return;
  }

  if (!legacy_datagram_writable_) {
    // The legacy writable participates in scheduling with the same default
    // priority as a createWritable() writable whose sendGroup is null and
    // sendOrder is zero.
    datagram_writables_.push_back(std::make_unique<DatagramWritable>(this));
    legacy_datagram_writable_ = datagram_writables_.back().get();
  }
  QueueDatagram(legacy_datagram_writable_, data, std::move(callback));
}

void WebTransport::CreateDatagramWritable(
    mojo::PendingReceiver<mojom::WebTransportDatagramWritable> writable,
    mojom::WebTransportStreamPriorityPtr priority) {
  if (torn_down_ || closing_) {
    return;
  }
  // Refusing a writable is not a protocol violation, so the renderer is not
  // terminated. Send an explicit reason because an unexplained disconnect can
  // also mean that session teardown destroyed an undelivered creation request.
  if (datagram_writables_.size() >= kMaxDatagramWritables) {
    auto rejected = std::make_unique<DatagramWritable>(
        this, std::move(writable), *priority);
    rejected->Close(
        mojom::WebTransportDatagramWritable::kCreationRejectedDisconnectReason,
        "The WebTransport session reached its Datagram writable limit.");
    return;
  }
  datagram_writables_.push_back(
      std::make_unique<DatagramWritable>(this, std::move(writable), *priority));
}

void WebTransport::QueueDatagram(DatagramWritable* writable,
                                 base::span<const uint8_t> data,
                                 base::OnceCallback<void(bool)> callback) {
  // Datagrams which cannot be handed to QUICHE any more are discarded, which
  // is a valid outcome for a Datagram write.
  if (torn_down_ || closing_) {
    std::move(callback).Run(false);
    return;
  }
  if (pending_datagram_count_ >= kMaxPendingDatagrams ||
      pending_datagram_bytes_ >= kMaxPendingDatagramBytes ||
      data.size() > kMaxPendingDatagramBytes - pending_datagram_bytes_) {
    std::move(callback).Run(false);
    return;
  }

  const bool was_empty = writable->pending_datagrams().empty();
  writable->pending_datagrams().push(
      PendingDatagram{std::vector<uint8_t>(data.begin(), data.end()),
                      base::TimeTicks::Now(), std::move(callback)});
  ++pending_datagram_count_;
  // Safe because the resource-limit check above bounds the sum.
  pending_datagram_bytes_ += data.size();
  if (was_empty) {
    ScheduleDatagramWritable(writable);
  }

  ScheduleDatagramPump();
  ResetDatagramExpirationTimer();
}

void WebTransport::AccountForRemovedPendingDatagram(size_t size) {
  CHECK_GT(pending_datagram_count_, 0u);
  CHECK_GE(pending_datagram_bytes_, size);
  --pending_datagram_count_;
  pending_datagram_bytes_ -= size;
}

void WebTransport::ScheduleDatagramWritable(DatagramWritable* writable) {
  CHECK(!writable->pending_datagrams().empty());

  uint64_t group_schedule_order = 0;
  for (const auto& other : datagram_writables_) {
    if (other.get() != writable && !other->pending_datagrams().empty() &&
        other->priority().send_group_id == writable->priority().send_group_id) {
      group_schedule_order = other->group_schedule_order();
      break;
    }
  }
  if (group_schedule_order == 0) {
    group_schedule_order = ++next_datagram_schedule_order_;
  }
  writable->set_group_schedule_order(group_schedule_order);
  writable->set_writable_schedule_order(++next_datagram_schedule_order_);
}

WebTransport::DatagramWritable* WebTransport::SelectNextDatagramWritable() {
  DatagramWritable* selected = nullptr;
  for (const auto& entry : datagram_writables_) {
    DatagramWritable* const candidate = entry.get();
    if (candidate->pending_datagrams().empty()) {
      continue;
    }
    if (!selected) {
      selected = candidate;
      continue;
    }

    if (candidate->group_schedule_order() < selected->group_schedule_order() ||
        (candidate->group_schedule_order() ==
             selected->group_schedule_order() &&
         (candidate->priority().send_order > selected->priority().send_order ||
          (candidate->priority().send_order ==
               selected->priority().send_order &&
           candidate->writable_schedule_order() <
               selected->writable_schedule_order())))) {
      selected = candidate;
    }
  }
  CHECK(selected);
  return selected;
}

void WebTransport::RescheduleDatagramGroup(
    webtransport::SendGroupId send_group_id) {
  const uint64_t schedule_order = ++next_datagram_schedule_order_;
  for (const auto& writable : datagram_writables_) {
    if (!writable->pending_datagrams().empty() &&
        writable->priority().send_group_id == send_group_id) {
      writable->set_group_schedule_order(schedule_order);
    }
  }
}

void WebTransport::MaybeSendDatagrams() {
  datagram_pump_scheduled_ = false;
  if (torn_down_ || closing_ || datagram_send_in_progress_ ||
      datagram_blocked_) {
    return;
  }

  webtransport::Session* const session = transport_->session();
  if (!session) {
    return;
  }

  size_t datagrams_processed = 0;
  while (pending_datagram_count_ > 0 &&
         datagrams_processed < kMaxDatagramsPerPump) {
    DatagramWritable* const writable = SelectNextDatagramWritable();
    PendingDatagram datagram = std::move(writable->pending_datagrams().front());
    writable->pending_datagrams().pop();
    ++datagrams_processed;
    AccountForRemovedPendingDatagram(datagram.data.size());
    const webtransport::SendGroupId send_group_id =
        writable->priority().send_group_id;
    if (!writable->pending_datagrams().empty()) {
      writable->set_writable_schedule_order(++next_datagram_schedule_order_);
    }
    RescheduleDatagramGroup(send_group_id);

    datagram_callbacks_.push(std::move(datagram.callback));
    in_flight_datagram_writable_ = writable;
    // QUIC's queue observer runs synchronously for final results. The fallback
    // below also handles implementations that return a final status without
    // notifying the observer. At most one Datagram is committed to QUIC, so a
    // synchronous notification always belongs to this send.
    datagram_send_in_progress_ = true;
    datagram_processed_during_send_ = false;
    base::WeakPtr<WebTransport> weak_this = weak_factory_.GetWeakPtr();
    webtransport::DatagramStatus status =
        session->SendOrQueueDatagram(base::as_string_view(datagram.data));
    // SendOrQueueDatagram() can synchronously re-enter through the queue
    // observer's OnDatagramProcessed(). The WeakPtr protects against re-entrant
    // destruction; the state checks below handle synchronous session teardown.
    if (!weak_this) {
      return;
    }
    datagram_send_in_progress_ = false;
    if (torn_down_ || closing_ || !transport_->session()) {
      return;
    }

    if (status.code == webtransport::DatagramStatusCode::kBlocked &&
        !datagram_processed_during_send_) {
      datagram_blocked_ = true;
      break;
    }

    if (!datagram_processed_during_send_) {
      base::WeakPtr<WebTransport> weak_after_completion =
          weak_factory_.GetWeakPtr();
      CompleteNextDatagram(status.code ==
                           webtransport::DatagramStatusCode::kSuccess);
      if (!weak_after_completion || torn_down_) {
        return;
      }
    }
  }

  ResetDatagramExpirationTimer();
  if (pending_datagram_count_ > 0) {
    ScheduleDatagramPump();
  }
}

void WebTransport::CompleteNextDatagram(bool sent) {
  CHECK(!datagram_callbacks_.empty());
  in_flight_datagram_writable_ = nullptr;
  base::OnceCallback<void(bool)> callback =
      std::move(datagram_callbacks_.front());
  datagram_callbacks_.pop();
  // The writable this Datagram came from may already be gone; the response is
  // then dropped by the bindings.
  std::move(callback).Run(sent);
}

void WebTransport::ScheduleDatagramPump() {
  if (datagram_pump_scheduled_ || datagram_send_in_progress_ ||
      datagram_blocked_) {
    return;
  }
  datagram_pump_scheduled_ = true;
  // Defer pumping to avoid reentering QUIC write processing and to batch
  // priority updates made in the same task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebTransport::MaybeSendDatagrams,
                                weak_factory_.GetWeakPtr()));
}

base::TimeDelta WebTransport::GetOutgoingDatagramExpirationDuration() const {
  if (!outgoing_datagram_expiration_duration_.is_zero()) {
    return outgoing_datagram_expiration_duration_;
  }

  webtransport::Session* const session = transport_->session();
  if (!session) {
    return kMinDefaultDatagramExpirationDuration;
  }
  const base::TimeDelta min_rtt =
      ToTimeDelta(session->GetSessionStats().min_rtt);
  // Match QuicDatagramQueue's default of 1.25 minimum RTTs with a four
  // millisecond floor.
  return std::max(min_rtt * 5 / 4, kMinDefaultDatagramExpirationDuration);
}

void WebTransport::ResetDatagramExpirationTimer() {
  datagram_expiration_timer_.Stop();
  const base::TimeDelta expiration_duration =
      GetOutgoingDatagramExpirationDuration();

  std::optional<base::TimeTicks> earliest_expiration;
  for (const auto& writable : datagram_writables_) {
    // Preserve callback order for a writable while one of its Datagrams is
    // blocked in QUIC. CompleteNextDatagram() resets the timer after it clears
    // the in-flight writable.
    if (in_flight_datagram_writable_ == writable.get() ||
        writable->pending_datagrams().empty()) {
      continue;
    }
    const base::TimeTicks expiration =
        writable->pending_datagrams().front().queued_at + expiration_duration;
    if (!earliest_expiration || expiration < *earliest_expiration) {
      earliest_expiration = expiration;
    }
  }
  if (!earliest_expiration) {
    return;
  }

  datagram_expiration_timer_.Start(
      FROM_HERE,
      std::max(base::TimeDelta(),
               *earliest_expiration - base::TimeTicks::Now()),
      base::BindOnce(&WebTransport::ExpirePendingDatagrams,
                     weak_factory_.GetWeakPtr()));
}

void WebTransport::ExpirePendingDatagrams() {
  if (torn_down_ || closing_) {
    return;
  }
  const base::TimeDelta expiration_duration =
      GetOutgoingDatagramExpirationDuration();
  const base::TimeTicks expiration_cutoff =
      base::TimeTicks::Now() - expiration_duration;
  std::vector<base::OnceCallback<void(bool)>> expired_callbacks;
  for (const auto& writable : datagram_writables_) {
    if (in_flight_datagram_writable_ == writable.get()) {
      continue;
    }
    base::queue<PendingDatagram>& pending_datagrams =
        writable->pending_datagrams();
    while (!pending_datagrams.empty() &&
           pending_datagrams.front().queued_at <= expiration_cutoff) {
      AccountForRemovedPendingDatagram(pending_datagrams.front().data.size());
      expired_callbacks.push_back(
          std::move(pending_datagrams.front().callback));
      pending_datagrams.pop();
      ++expired_pending_datagram_count_;
    }
  }
  ScheduleDatagramPump();
  ResetDatagramExpirationTimer();
  // These callbacks are owned by the local vector, so they remain valid if a
  // callback re-enters and destroys this object.
  base::WeakPtr<WebTransport> weak_this = weak_factory_.GetWeakPtr();
  for (auto& callback : expired_callbacks) {
    std::move(callback).Run(false);
    if (!weak_this) {
      return;
    }
  }
}

void WebTransport::OnDatagramWritableDisconnected(DatagramWritable* writable) {
  writable->Close(/*custom_reason=*/0, std::string_view());
  if (in_flight_datagram_writable_ == writable) {
    in_flight_datagram_writable_ = nullptr;
  }
  std::erase_if(datagram_writables_,
                [writable](const std::unique_ptr<DatagramWritable>& entry) {
                  return entry.get() == writable;
                });
  ScheduleDatagramPump();
  ResetDatagramExpirationTimer();
}

void WebTransport::ClearDatagramState() {
  datagram_expiration_timer_.Stop();
  for (const auto& writable : datagram_writables_) {
    writable->Close(
        mojom::WebTransportDatagramWritable::kSessionClosedDisconnectReason,
        "The WebTransport session closed.");
  }
  // Clear the pointers into `datagram_writables_` before its entries are
  // destroyed.
  legacy_datagram_writable_ = nullptr;
  in_flight_datagram_writable_ = nullptr;
  datagram_writables_.clear();
  // Every pipe those callbacks could respond on is closed now, so they can be
  // dropped without upsetting the bindings.
  datagram_callbacks_ = {};
}

void WebTransport::MovePendingDatagramToInFlightForTesting(size_t index) {
  auto& writable = datagram_writables_[index];
  CHECK(!writable->pending_datagrams().empty());
  PendingDatagram datagram = std::move(writable->pending_datagrams().front());
  writable->pending_datagrams().pop();
  AccountForRemovedPendingDatagram(datagram.data.size());
  datagram_callbacks_.push(std::move(datagram.callback));
  in_flight_datagram_writable_ = writable.get();
}

void WebTransport::ExpireNextDatagramForTesting(size_t index) {
  auto& writable = datagram_writables_[index];
  CHECK(!writable->pending_datagrams().empty());
  writable->pending_datagrams().front().queued_at = base::TimeTicks();
  ExpirePendingDatagrams();
}

void WebTransport::CreateStream(
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable,
    mojom::WebTransportStreamPriorityPtr priority,
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
    if (priority) {
      stream->SetPriority(ToStreamPriority(*priority));
    }
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
  if (priority) {
    stream->SetPriority(ToStreamPriority(*priority));
  }
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
  if (it != streams_.end()) {
    it->second->StopSending(code);
  }
  // The renderer requests final stats before StopSending(). Messages on the
  // WebTransport remote are ordered, so the cancellation snapshot is no longer
  // needed once this request arrives. A renderer disconnect instead destroys
  // the WebTransport through receiver_'s disconnect handler.
  auto final_it = final_receive_stream_stats_.Peek(stream);
  if (final_it != final_receive_stream_stats_.end()) {
    final_receive_stream_stats_.Erase(final_it);
  }
}

void WebTransport::SetStreamPriority(
    uint32_t stream,
    mojom::WebTransportStreamPriorityPtr priority) {
  auto it = streams_.find(stream);
  if (it == streams_.end()) {
    return;
  }
  it->second->SetPriority(ToStreamPriority(*priority));
}

void WebTransport::SetOutgoingDatagramExpirationDuration(
    base::TimeDelta duration) {
  if (torn_down_ || closing_) {
    return;
  }

  CHECK(transport_->session());
  duration = std::clamp(duration, base::TimeDelta(),
                        kMaxOutgoingDatagramExpirationDuration);
  outgoing_datagram_expiration_duration_ = duration;
  transport_->session()->SetDatagramMaxTimeInQueue(
      absl::Microseconds(duration.InMicroseconds()));
  ResetDatagramExpirationTimer();
}

void WebTransport::Close(mojom::WebTransportCloseInfoPtr close_info) {
  if (torn_down_ || closing_) {
    return;
  }
  closing_ = true;

  receiver_.reset();
  handshake_client_.reset();
  client_.reset();
  // Closes the writables' pipes, which discards the Datagrams they still have
  // queued.
  ClearDatagramState();

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

void WebTransport::OnLocalNetworkAccessCheck(
    const net::IPEndPoint& server_address,
    const net::NetLogWithSource& net_log,
    net::CompletionOnceCallback callback) {
  if (!base::FeatureList::IsEnabled(
          features::kLocalNetworkAccessChecksWebTransport)) {
    std::move(callback).Run(net::OK);
    return;
  }

  // required_ip_address_space is always kUnknown as WebTransport is always
  // https, so there is no need for mixed content check bypasses.
  //
  // WebTransport has no `url_load_options` available for overriding in
  // content/public/browser/content_browser_client.h.
  LocalNetworkAccessChecker checker(
      url_, origin_,
      /*required_ip_address_space=*/network::mojom::IPAddressSpace::kUnknown,
      client_security_state_.get(), /*url_load_options=*/0);

  LocalNetworkAccessCheckResult check_result = checker.Check(server_address);
  std::optional<mojom::CorsError> cors_error =
      LocalNetworkAccessCheckResultToCorsError(check_result);
  if (!cors_error.has_value()) {
    std::move(callback).Run(net::OK);
    return;
  }

  if (url_loader_network_observer_ &&
      check_result == LocalNetworkAccessCheckResult::kLNAPermissionRequired) {
    // WebTransport connections are not cached, so just use kDirect.
    mojom::TransportType transport_type = mojom::TransportType::kDirect;

    url_loader_network_observer_->OnLocalNetworkAccessPermissionRequired(
        transport_type, *checker.ResponseAddressSpace(),
        base::BindOnce(
            [](base::WeakPtr<WebTransport> weak_self,
               const net::NetLogWithSource& net_log,
               const mojom::TransportType transport_type,
               const mojom::IPAddressSpace address_space,
               net::CompletionOnceCallback callback,
               mojom::LocalNetworkAccessResult result) {
              if (!weak_self) {
                // Checking the weak ptr not to call the `callback` after
                // `this` is destructed. This is needed because the
                // observer's pipe may outlive `this` and the owner
                // `WebTransport`.
                return;
              }

              net_log.AddEvent(
                  net::NetLogEventType::
                      LOCAL_NETWORK_ACCESS_PERMISSION_REQUESTED,
                  [&] {
                    return base::DictValue()
                        .Set("address_space",
                             IPAddressSpaceToStringPiece(address_space))
                        .Set("transport_type",
                             TransportTypeToStringPiece(transport_type))
                        .Set("result",
                             LocalNetworkAccessResultToStringPiece(result));
                  });

              std::move(callback).Run(
                  result == mojom::LocalNetworkAccessResult::kGranted
                      ? net::OK
                      : net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS);
            },
            weak_factory_.GetWeakPtr(), net_log, transport_type,
            *checker.ResponseAddressSpace(), std::move(callback)));
  } else {
    std::move(callback).Run(net::ERR_BLOCKED_BY_LOCAL_NETWORK_ACCESS_CHECKS);
  }
}

void WebTransport::OnBeforeConnect(const net::IPEndPoint& server_address) {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(handshake_client_);

  // Here we assume that the server_address is not going to handed to the
  // initiator renderer.
  handshake_client_->OnBeforeConnect(server_address);
}

void WebTransport::OnConnected(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  if (torn_down_ || closing_) {
    return;
  }

  DCHECK(handshake_client_);
  CHECK(response_headers);

  // https://fetch.spec.whatwg.org/#forbidden-response-header-name
  auto filtered_response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          response_headers->raw_headers());
  filtered_response_headers->RemoveHeader("Set-Cookie");
  filtered_response_headers->RemoveHeader("Set-Cookie2");

  auto max_datagram_size =
      transport_->GetMaxDatagramSize().transform([](quic::QuicByteCount size) {
        return base::saturated_cast<uint32_t>(size);
      });
  handshake_client_->OnConnectionEstablished(
      receiver_.BindNewPipeAndPassRemote(),
      client_.BindNewPipeAndPassReceiver(),
      std::move(filtered_response_headers),
      transport_->session()->GetNegotiatedSubprotocol(),
      StatsToMojom(transport_->session()->GetSessionStats(),
                   expired_pending_datagram_count_),
      max_datagram_size);

  handshake_client_.reset();
  // We set the disconnect handler for `receiver_`, not `client_`, in order
  // to make the closing sequence consistent: The client calls Close() and
  // then resets the mojo endpoints.
  receiver_.set_disconnect_handler(
      base::BindOnce(&WebTransport::Dispose, base::Unretained(this)));

  // A GOAWAY travels on the connection's control stream, independently of the
  // CONNECT stream carrying this handshake, so drain signal can be received
  // before the CONNECT. Send drain now if that happened.
  if (draining_received_) {
    client_->OnDraining();
  }
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
      final_stats = StatsToMojom(transport_->session()->GetSessionStats(),
                                 expired_pending_datagram_count_);
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

void WebTransport::OnDraining() {
  if (torn_down_ || closing_ || draining_received_) {
    return;
  }

  draining_received_ = true;

  if (client_.is_bound()) {
    client_->OnDraining();
  }
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

  client_->OnDatagramReceived(base::as_byte_span(datagram));
}

void WebTransport::OnCanCreateNewOutgoingBidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void WebTransport::OnCanCreateNewOutgoingUnidirectionalStream() {
  // TODO(yhirano): Implement this.
}

void WebTransport::OnDatagramProcessed(
    std::optional<quic::DatagramStatus> status) {
  if (torn_down_) {
    return;
  }
  if (datagram_callbacks_.empty()) {
    datagram_blocked_ = false;
    ScheduleDatagramPump();
    return;
  }
  base::WeakPtr<WebTransport> weak_this = weak_factory_.GetWeakPtr();
  CompleteNextDatagram(status == quic::DATAGRAM_STATUS_SUCCESS);
  if (!weak_this || torn_down_) {
    return;
  }
  if (datagram_send_in_progress_) {
    datagram_processed_during_send_ = true;
    return;
  }
  ResetDatagramExpirationTimer();
  datagram_blocked_ = false;
  ScheduleDatagramPump();
}

void WebTransport::GetStats(GetStatsCallback callback) {
  webtransport::Session* const session = transport_->session();

  if (torn_down_ || session == nullptr) {
    std::move(callback).Run(nullptr);
    return;
  }

  webtransport::SessionStats stats = session->GetSessionStats();
  std::move(callback).Run(StatsToMojom(stats, expired_pending_datagram_count_));
}

void WebTransport::GetReceiveStreamStats(
    uint32_t stream_id,
    GetReceiveStreamStatsCallback callback) {
  if (torn_down_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // During local cancellation, closing the data pipe and this request arrive
  // independently. If pipe closure is observed first, use the saved snapshot;
  // otherwise, return the stream's value when this request is handled. Both
  // represent the receive count at the cancellation boundary.
  auto final_it = final_receive_stream_stats_.Peek(stream_id);
  if (final_it != final_receive_stream_stats_.end()) {
    auto stats = mojom::WebTransportReceiveStreamStats::New();
    stats->bytes_received = final_it->second;
    std::move(callback).Run(std::move(stats));
    return;
  }

  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  Stream* stream = it->second.get();
  auto stats = mojom::WebTransportReceiveStreamStats::New();
  stats->bytes_received = stream->bytes_received();
  std::move(callback).Run(std::move(stats));
}

void WebTransport::TearDown() {
  torn_down_ = true;
  receiver_.reset();
  handshake_client_.reset();
  client_.reset();
  ClearDatagramState();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebTransport::Dispose, weak_factory_.GetWeakPtr()));
}

void WebTransport::Dispose() {
  // For tab close scenario: Send explicit connection close
  // frame to ensure proper termination before cleanup.
  if (transport_ && !torn_down_ && transport_->session()) {
    transport_->Close(std::nullopt);
  }

  receiver_.reset();

  context_->Remove(this);
  // |this| is deleted.
}

}  // namespace network
