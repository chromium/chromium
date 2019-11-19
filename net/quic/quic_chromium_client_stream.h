// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: This code is not shared between Google and Chrome.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_

#include <stddef.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_stream.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
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
    ~Handle();

    // Returns true if the stream is still connected.
    bool IsOpen() { return stream_ != nullptr; }

    // Reads initial headers into |header_block| and returns the length of
    // the HEADERS frame which contained them. If headers are not available,
    // returns ERR_IO_PENDING and will invoke |callback| asynchronously when
    // the headers arrive.
    // TODO(rch): Invoke |callback| when there is a stream or connection error
    // instead of calling OnClose() or OnError().
    int ReadInitialHeaders(spdy::SpdyHeaderBlock* header_block,
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
    int ReadTrailingHeaders(spdy::SpdyHeaderBlock* header_block,
                            CompletionOnceCallback callback);

    // Writes |header_block| to the peer. Closes the write side if |fin| is
    // true. If non-null, |ack_notifier_delegate| will be notified when the
    // headers are ACK'd by the peer. Returns a net error code if there is
    // an error writing the headers, or the number of bytes written on
    // success. Will not return ERR_IO_PENDING.
    int WriteHeaders(
        spdy::SpdyHeaderBlock header_block,
        bool fin,
        quic::QuicReferenceCountedPointer<quic::QuicAckListenerInterface>
            ack_notifier_delegate);

    // Writes |data| to the peer. Closes the write side if |fin| is true.
    // If the data could not be written immediately, returns ERR_IO_PENDING
    // and invokes |callback| asynchronously when the write completes.
    int WriteStreamData(base::StringPiece data,
                        bool fin,
                        CompletionOnceCallback callback);

    // Same as WriteStreamData except it writes data from a vector of IOBuffers,
    // with the length of each buffer at the corresponding index in |lengths|.
    int WritevStreamData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                         const std::vector<int>& lengths,
                         bool fin,
                         CompletionOnceCallback callback);

    // Reads at most |buf_len| bytes into |buf|. Returns the number of bytes
    // read.
    int Read(IOBuffer* buf, int buf_len);

    // Called to notify the stream when the final incoming data is read.
    void OnFinRead();

    // Prevents the connection from migrating to a cellular network while this
    // stream is open.
    void DisableConnectionMigrationToCellularNetwork();

    // Sets the precedence of the stream to |precedence|.
    void SetPriority(const spdy::SpdyStreamPrecedence& precedence);

    // Sends a RST_STREAM frame to the peer and closes the streams.
    void Reset(quic::QuicRstStreamErrorCode error_code);

    quic::QuicStreamId id() const;
    quic::QuicErrorCode connection_error() const;
    quic::QuicRstStreamErrorCode stream_error() const;
    bool fin_sent() const;
    bool fin_received() const;
    uint64_t stream_bytes_read() const;
    uint64_t stream_bytes_written() const;
    size_t NumBytesConsumed() const;
    bool HasBytesToRead() const;
    bool IsDoneReading() const;
    bool IsFirstStream() const;

    // TODO(rch): Move these test-only methods to a peer, or else remove.
    void OnPromiseHeaderList(quic::QuicStreamId promised_id,
                             size_t frame_len,
                             const quic::QuicHeaderList& header_list);
    bool can_migrate_to_cellular_network();

    const NetLogWithSource& net_log() const;

   private:
    friend class QuicChromiumClientStream;

    // Constucts a new Handle for |stream|.
    explicit Handle(QuicChromiumClientStream* stream);

    // Methods invoked by the stream.
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

    QuicChromiumClientStream* stream_;  // Unowned.

    bool may_invoke_callbacks_;  // True when callbacks may be invoked.

    // Callback to be invoked when ReadHeaders completes asynchronously.
    CompletionOnceCallback read_headers_callback_;
    spdy::SpdyHeaderBlock* read_headers_buffer_;

    // Callback to be invoked when ReadBody completes asynchronously.
    CompletionOnceCallback read_body_callback_;
    IOBuffer* read_body_buffer_;
    int read_body_buffer_len_;

    // Callback to be invoked when WriteStreamData or WritevStreamData completes
    // asynchronously.
    CompletionOnceCallback write_callback_;

    quic::QuicStreamId id_;
    quic::QuicErrorCode connection_error_;
    quic::QuicRstStreamErrorCode stream_error_;
    bool fin_sent_;
    bool fin_received_;
    uint64_t stream_bytes_read_;
    uint64_t stream_bytes_written_;
    bool is_done_reading_;
    bool is_first_stream_;
    size_t num_bytes_consumed_;

    int net_error_;

    NetLogWithSource net_log_;

    base::WeakPtrFactory<Handle> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

  QuicChromiumClientStream(
      quic::QuicStreamId id,
      quic::QuicSpdyClientSessionBase* session,
      quic::StreamType type,
      const NetLogWithSource& net_log,
      const NetworkTrafficAnnotationTag& traffic_annotation);
  QuicChromiumClientStream(
      quic::PendingStream* pending,
      quic::QuicSpdyClientSessionBase* session,
      quic::StreamType type,
      const NetLogWithSource& net_log,
      const NetworkTrafficAnnotationTag& traffic_annotation);

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
  void OnPromiseHeaderList(quic::QuicStreamId promised_id,
                           size_t frame_len,
                           const quic::QuicHeaderList& header_list) override;
  void OnBodyAvailable() override;
  void OnClose() override;
  void OnCanWrite() override;
  size_t WriteHeaders(
      spdy::SpdyHeaderBlock header_block,
      bool fin,
      quic::QuicReferenceCountedPointer<quic::QuicAckListenerInterface>
          ack_listener) override;

  // While the server's set_priority shouldn't be called externally, the creator
  // of client-side streams should be able to set the priority.
  using quic::QuicSpdyStream::SetPriority;

  // Writes |data| to the peer and closes the write side if |fin| is true.
  // Returns true if the data have been fully written. If the data was not fully
  // written, returns false and OnCanWrite() will be invoked later.
  bool WriteStreamData(quic::QuicStringPiece data, bool fin);
  // Same as WriteStreamData except it writes data from a vector of IOBuffers,
  // with the length of each buffer at the corresponding index in |lengths|.
  bool WritevStreamData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                        const std::vector<int>& lengths,
                        bool fin);

  // Creates a new Handle for this stream. Must only be called once.
  std::unique_ptr<QuicChromiumClientStream::Handle> CreateHandle();

  // Clears |handle_| from this stream.
  void ClearHandle();

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

  // True if this stream is the first data stream created on this session.
  bool IsFirstStream();

  bool DeliverInitialHeaders(spdy::SpdyHeaderBlock* header_block,
                             int* frame_len);

  bool DeliverTrailingHeaders(spdy::SpdyHeaderBlock* header_block,
                              int* frame_len);

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
  Handle* handle_;

  bool headers_delivered_;

  // True when initial headers have been sent.
  bool initial_headers_sent_;

  quic::QuicSpdyClientSessionBase* session_;
  quic::QuicTransportVersion quic_version_;

  // Set to false if this stream should not be migrated to a cellular network
  // during connection migration.
  bool can_migrate_to_cellular_network_;

  // Stores the initial header if they arrive before the handle.
  spdy::SpdyHeaderBlock initial_headers_;
  // Length of the HEADERS frame containing initial headers.
  size_t initial_headers_frame_len_;

  // Length of the HEADERS frame containing trailing headers.
  size_t trailing_headers_frame_len_;

  base::WeakPtrFactory<QuicChromiumClientStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicChromiumClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_H_
