// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: This code is not shared between Google and Chrome.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_

#include <stddef.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/idempotency.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace quic {
class QuicSpdyClientSessionBase;
}  // namespace quic
namespace net {

// A client-initiated ReliableQuicStream.  Instances of this class
// are owned by the QuicClientSession which created them.
class NET_EXPORT_PRIVATE QuicChromiumClientStream
    : public quic::QuicSpdyStream {
 public:
  // Wrapper for interacting with the session in a restricted fashion.
  class NET_EXPORT_PRIVATE Handle {
   public:
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    ~Handle();

    // Returns true if the stream is still connected.
    bool IsOpen() { return stream_ != nullptr; }

    // Reads initial or 103 Early Hints headers into |header_block| and returns
    // the length of the HEADERS frame which contained them. If headers are not
    // available, returns ERR_IO_PENDING and will invoke |callback|
    // asynchronously when the headers arrive.
    // TODO(rch): Invoke |callback| when there is a stream or connection error
    // instead of calling OnClose() or OnError().
    int ReadInitialHeaders(quiche::HttpHeaderBlock* header_block,
                           CompletionOnceCallback callback);

    // Reads at most |buffer_len| bytes of body into |buffer| and returns the
    // number of bytes read. If body is not available, returns ERR_IO_PENDING
    // and will invoke |callback| asynchronously when data arrive.
    // TODO(rch): Invoke |callback| when there is a stream or connection error
    // instead of calling OnClose() or OnError().
    int ReadBody(IOBuffer* buffer,
                 int buffer_len,
                 CompletionOnceCallback callback);

    // Reads trailing headers into |header_block| and returns the length of
    // the HEADERS frame which contained them. If headers are not available,
    // returns ERR_IO_PENDING and will invoke |callback| asynchronously when
    // the headers arrive.
    // TODO(rch): Invoke |callback| when there is a stream or connection error
    // instead of calling OnClose() or OnError().
    int ReadTrailingHeaders(quiche::HttpHeaderBlock* header_block,
                            CompletionOnceCallback callback);

    // Writes |header_block| to the peer. Closes the write side if |fin| is
    // true. If non-null, |ack_notifier_delegate| will be notified when the
    // headers are ACK'd by the peer. Returns a net error code if there is
    // an error writing the headers, or the number of bytes written on
    // success. Will not return ERR_IO_PENDING.
    int WriteHeaders(
        quiche::HttpHeaderBlock header_block,
        bool fin,
        quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
            ack_notifier_delegate);

    // Writes |data| to the peer. Closes the write side if |fin| is true.
    // If the data could not be written immediately, returns ERR_IO_PENDING
    // and invokes |callback| asynchronously when the write completes.
    int WriteStreamData(std::string_view data,
                        bool fin,
                        CompletionOnceCallback callback);

    // Same as WriteStreamData except it writes data from a vector of IOBuffers,
    // with the length of each buffer at the corresponding index in |lengths|.
    int WritevStreamData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                         const std::vector<int>& lengths,
                         bool fin,
                         CompletionOnceCallback callback);

    // Writes |packet| to server by constructing a UDP payload from
    // packet and sending the datagram on the stream.
    int WriteConnectUdpPayload(std::string_view packet);

    // Reads at most |buf_len| bytes into |buf|. Returns the number of bytes
    // read.
    int Read(IOBuffer* buf, int buf_len);

    // Called to notify the stream when the final incoming data is read.
    void OnFinRead();

    // Prevents the connection from migrating to a cellular network while this
    // stream is open.
    void DisableConnectionMigrationToCellularNetwork();

    // Sets the precedence of the stream to |priority|.
    void SetPriority(const quic::QuicStreamPriority& priority);

    // Sends a RST_STREAM frame to the peer and closes the streams.
    void Reset(quic::QuicRstStreamErrorCode error_code);

    // Registers |visitor| to receive HTTP/3 datagrams on the stream.
    void RegisterHttp3DatagramVisitor(Http3DatagramVisitor* visitor);

    // Unregisters an HTTP/3 datagram visitor.
    void UnregisterHttp3DatagramVisitor();

    quic::QuicStreamId id() const;
    quic::QuicErrorCode connection_error() const;
    quic::QuicRstStreamErrorCode stream_error() const;
    uint64_t connection_wire_error() const;
    uint64_t ietf_application_error() const;
    bool fin_sent() const;
    bool fin_received() const;
    uint64_t stream_bytes_read() const;
    uint64_t stream_bytes_written() const;
    size_t NumBytesConsumed() const;
    bool HasBytesToRead() const;
    bool IsDoneReading() const;
    bool IsFirstStream() const;

    base::TimeTicks first_early_hints_time() const {
      return first_early_hints_time_;
    }

    base::TimeTicks headers_received_start_time() const {
      return headers_received_start_time_;
    }

    // TODO(rch): Move this test-only method to a peer, or else remove.
    bool can_migrate_to_cellular_network();

    const NetLogWithSource& net_log() const;

    // Sets the idempotency of the request.
    void SetRequestIdempotency(Idempotency idempotency);
    // Returns the idempotency of the request.
    Idempotency GetRequestIdempotency() const;

    // Returns the largest payload that will fit into a single MESSAGE frame at
    // any point during the connection.  This assumes the version and
    // connection ID lengths do not change. Returns zero if the stream or
    // session are closed.
    quic::QuicPacketLength GetGuaranteedLargestMessagePayload() const;

   private:
    friend class QuicChromiumClientStream;

    // Constucts a new Handle for |stream|.
    explicit Handle(QuicChromiumClientStream* stream);

    // Methods invoked by the stream.
    void OnEarlyHintsAvailable();
    void OnInitialHeadersAvailable();
    void OnTrailingHeadersAvailable();
    void OnDataAvailable();
    void OnCanWrite();
    void OnClose();
    void OnError(int error);

    // Invokes async IO callbacks because of |error|.
    void InvokeCallbacksOnClose(int error);

    // Saves various fields from the stream before the stream goes away.
    void SaveState();

    void SetCallback(CompletionOnceCallback new_callback,
                     CompletionOnceCallback* callback);

    void ResetAndRun(CompletionOnceCallback callback, int rv);

    int HandleIOComplete(int rv);

    raw_ptr<QuicChromiumClientStream> stream_;  // Unowned.

    bool may_invoke_callbacks_ = true;  // True when callbacks may be invoked.

    // Callback to be invoked when ReadInitialHeaders completes asynchronously.
    CompletionOnceCallback read_headers_callback_;
    // Provided by the owner of this handle when ReadInitialHeaders is called.
    raw_ptr<quiche::HttpHeaderBlock> read_headers_buffer_ = nullptr;

    // Callback to be invoked when ReadBody completes asynchronously.
    CompletionOnceCallback read_body_callback_;
    scoped_refptr<IOBuffer> read_body_buffer_;
    int read_body_buffer_len_ = 0;

    // Callback to be invoked when WriteStreamData or WritevStreamData completes
    // asynchronously.
    CompletionOnceCallback write_callback_;

    quic::QuicStreamId id_;
    quic::QuicErrorCode connection_error_;
    quic::QuicRstStreamErrorCode stream_error_;
    uint64_t connection_wire_error_ = 0;
    uint64_t ietf_application_error_ = 0;
    bool fin_sent_;
    bool fin_received_;
    uint64_t stream_bytes_read_;
    uint64_t stream_bytes_written_;
    bool is_done_reading_;
    bool is_first_stream_;
    size_t num_bytes_consumed_;
    Idempotency idempotency_ = DEFAULT_IDEMPOTENCY;

    int net_error_ = ERR_UNEXPECTED;

    NetLogWithSource net_log_;

    // The time at which the first 103 Early Hints response is received.
    base::TimeTicks first_early_hints_time_;

    base::TimeTicks headers_received_start_time_;

    base::WeakPtrFactory<Handle> weak_factory_{this};
  };

  QuicChromiumClientStream(
      quic::QuicStreamId id,
      quic::QuicSpdyClientSessionBase* session,
      quic::QuicServerId server_id,
      quic::StreamType type,
      const NetLogWithSource& net_log,
      const NetworkTrafficAnnotationTag& traffic_annotation);
  QuicChromiumClientStream(
      quic::PendingStream* pending,
      quic::QuicSpdyClientSessionBase* session,
      quic::QuicServerId server_id,
      const NetLogWithSource& net_log,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  QuicChromiumClientStream(const QuicChromiumClientStream&) = delete;
  QuicChromiumClientStream& operator=(const QuicChromiumClientStream&) = delete;

  ~QuicChromiumClientStream() override;

  // quic::QuicSpdyStream
  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override;
  void OnTrailingHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override;
  void OnBodyAvailable() override;
  void OnClose() override;
  void OnCanWrite() override;
  size_t WriteHeaders(
      quiche::HttpHeaderBlock header_block,
      bool fin,
      quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
          ack_listener) override;

  // While the server's set_priority shouldn't be called externally, the creator
  // of client-side streams should be able to set the priority.
  using quic::QuicSpdyStream::SetPriority;

  // Writes |data| to the peer and closes the write side if |fin| is true.
  // Returns true if the data have been fully written. If the data was not fully
  // written, returns false and OnCanWrite() will be invoked later.
  bool WriteStreamData(std::string_view data, bool fin);
  // Same as WriteStreamData except it writes data from a vector of IOBuffers,
  // with the length of each buffer at the corresponding index in |lengths|.
  bool WritevStreamData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                        const std::vector<int>& lengths,
                        bool fin);

  // Creates a new Handle for this stream. Must only be called once.
  std::unique_ptr<QuicChromiumClientStream::Handle> CreateHandle();

  // Clears |handle_| from this stream.
  void ClearHandle();

  // Notifies the stream handle of error, but doesn't close the stream.
  void OnError(int error);

  // Reads at most |buf_len| bytes into |buf|. Returns the number of bytes read.
  int Read(IOBuffer* buf, int buf_len);

  const NetLogWithSource& net_log() const { return net_log_; }

  // Prevents this stream from migrating to a cellular network. May be reset
  // when connection migrates to a cellular network.
  void DisableConnectionMigrationToCellularNetwork();

  bool can_migrate_to_cellular_network() {
    return can_migrate_to_cellular_network_;
  }

  // True if the underlying QUIC session supports HTTP/3 Datagrams.
  bool SupportsH3Datagram() const;

  // Returns the largest payload that will fit into a single MESSAGE frame at
  // any point during the connection.  This assumes the version and
  // connection ID lengths do not change. Returns zero if the stream or
  // session are closed.
  quic::QuicPacketLength GetGuaranteedLargestMessagePayload() const;

  // True if this stream is the first data stream created on this session.
  bool IsFirstStream();

  int DeliverEarlyHints(quiche::HttpHeaderBlock* header_block);

  int DeliverInitialHeaders(quiche::HttpHeaderBlock* header_block);

  bool DeliverTrailingHeaders(quiche::HttpHeaderBlock* header_block,
                              int* frame_len);

  static constexpr char kHttp3DatagramDroppedHistogram[] =
      "Net.QuicChromiumClientStream."
      "Http3DatagramDroppedOnWriteConnectUdpPayload";

  using quic::QuicSpdyStream::HasBufferedData;
  using quic::QuicStream::sequencer;

 private:
  void NotifyHandleOfInitialHeadersAvailableLater();
  void NotifyHandleOfInitialHeadersAvailable();
  void NotifyHandleOfTrailingHeadersAvailableLater();
  void NotifyHandleOfTrailingHeadersAvailable();
  void NotifyHandleOfDataAvailableLater();
  void NotifyHandleOfDataAvailable();

  NetLogWithSource net_log_;
  raw_ptr<Handle> handle_ = nullptr;

  // True when initial headers have been sent.
  bool initial_headers_sent_ = false;

  raw_ptr<quic::QuicSpdyClientSessionBase> session_;
  const quic::QuicServerId server_id_;
  quic::QuicTransportVersion quic_version_;

  // Set to false if this stream should not be migrated to a cellular network
  // during connection migration.
  bool can_migrate_to_cellular_network_ = true;

  // True if non-informational (non-1xx) initial headers have arrived.
  bool initial_headers_arrived_ = false;
  // True if non-informational (non-1xx) initial headers have been delivered to
  // the handle.
  bool headers_delivered_ = false;
  // Stores the initial header until they are delivered to the handle.
  quiche::HttpHeaderBlock initial_headers_;
  // Length of the HEADERS frame containing initial headers.
  size_t initial_headers_frame_len_ = 0;

  // Length of the HEADERS frame containing trailing headers.
  size_t trailing_headers_frame_len_ = 0;

  struct EarlyHints {
    EarlyHints(quiche::HttpHeaderBlock headers, size_t frame_len)
        : headers(std::move(headers)), frame_len(frame_len) {}
    EarlyHints(EarlyHints&& other) = default;
    EarlyHints& operator=(EarlyHints&& other) = default;
    EarlyHints(const EarlyHints& other) = delete;
    EarlyHints& operator=(const EarlyHints& other) = delete;

    quiche::HttpHeaderBlock headers;
    size_t frame_len = 0;
  };
  base::circular_deque<EarlyHints> early_hints_;

  base::WeakPtrFactory<QuicChromiumClientStream> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_
