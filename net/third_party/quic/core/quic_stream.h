// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server QUIC streams.

// It does not contain the entire interface needed by an application to interact
// with a QUIC stream.  Some parts of the interface must be obtained by
// accessing the owning session object.  A subclass of QuicStream
// connects the object and the application that generates and consumes the data
// of the stream.

// The QuicStream object has a dependent QuicStreamSequencer object,
// which is given the stream frames as they arrive, and provides stream data in
// order by invoking ProcessRawData().

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <list>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_flow_controller.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_stream_send_buffer.h"
#include "net/third_party/quic/core/quic_stream_sequencer.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/session_notifier_interface.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_mem_slice_span.h"
#include "net/third_party/quic/platform/api/quic_reference_counted.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

namespace quic {

namespace test {
class QuicStreamPeer;
}  // namespace test

class QuicSession;

class QUIC_EXPORT_PRIVATE QuicStream {
 public:
  // This is somewhat arbitrary.  It's possible, but unlikely, we will either
  // fail to set a priority client-side, or cancel a stream before stripping the
  // priority from the wire server-side.  In either case, start out with a
  // priority in the middle.
  static const spdy::SpdyPriority kDefaultPriority = 3;
  static_assert(kDefaultPriority ==
                    (spdy::kV3LowestPriority + spdy::kV3HighestPriority) / 2,
                "Unexpected value of kDefaultPriority");

  // Creates a new stream with stream_id |id| associated with |session|. If
  // |is_static| is true, then the stream will be given precedence
  // over other streams when determing what streams should write next.
  // |type| indicates whether the stream is bidirectional, read unidirectional
  // or write unidirectional.
  QuicStream(QuicStreamId id,
             QuicSession* session,
             bool is_static,
             StreamType type);
  QuicStream(const QuicStream&) = delete;
  QuicStream& operator=(const QuicStream&) = delete;

  virtual ~QuicStream();

  // Not in use currently.
  void SetFromConfig();

  // Called by the session when a (potentially duplicate) stream frame has been
  // received for this stream.
  virtual void OnStreamFrame(const QuicStreamFrame& frame);

  // Called by the session when the connection becomes writeable to allow the
  // stream to write any pending data.
  virtual void OnCanWrite();

  // Called by the session just before the object is destroyed.
  // The object should not be accessed after OnClose is called.
  // Sends a RST_STREAM with code QUIC_RST_ACKNOWLEDGEMENT if neither a FIN nor
  // a RST_STREAM has been sent.
  virtual void OnClose();

  // Called by the session when the endpoint receives a RST_STREAM from the
  // peer.
  virtual void OnStreamReset(const QuicRstStreamFrame& frame);

  // Called by the session when the endpoint receives or sends a connection
  // close, and should immediately close the stream.
  virtual void OnConnectionClosed(QuicErrorCode error,
                                  ConnectionCloseSource source);

  // Called by the stream subclass after it has consumed the final incoming
  // data.
  virtual void OnFinRead();

  // Called when new data is available from the sequencer.  Subclasses must
  // actively retrieve the data using the sequencer's Readv() or
  // GetReadableRegions() method.
  virtual void OnDataAvailable() = 0;

  // Called by the subclass or the sequencer to reset the stream from this
  // end.
  virtual void Reset(QuicRstStreamErrorCode error);

  // Called by the subclass or the sequencer to close the entire connection from
  // this end.
  virtual void CloseConnectionWithDetails(QuicErrorCode error,
                                          const QuicString& details);

  spdy::SpdyPriority priority() const;

  // Sets priority_ to priority.  This should only be called before bytes are
  // written to the server.
  void SetPriority(spdy::SpdyPriority priority);

  // Returns true if this stream is still waiting for acks of sent data.
  // This will return false if all data has been acked, or if the stream
  // is no longer interested in data being acked (which happens when
  // a stream is reset because of an error).
  bool IsWaitingForAcks() const;

  // Number of bytes available to read.
  size_t ReadableBytes() const;

  QuicStreamId id() const { return id_; }

  QuicRstStreamErrorCode stream_error() const { return stream_error_; }
  QuicErrorCode connection_error() const { return connection_error_; }

  bool reading_stopped() const {
    return sequencer_.ignore_read_data() || read_side_closed_;
  }
  bool write_side_closed() const { return write_side_closed_; }

  bool rst_received() const { return rst_received_; }
  bool rst_sent() const { return rst_sent_; }
  bool fin_received() const { return fin_received_; }
  bool fin_sent() const { return fin_sent_; }
  bool fin_outstanding() const { return fin_outstanding_; }
  bool fin_lost() const { return fin_lost_; }

  uint64_t BufferedDataBytes() const;

  uint64_t stream_bytes_read() const { return stream_bytes_read_; }
  uint64_t stream_bytes_written() const;

  size_t busy_counter() const { return busy_counter_; }
  void set_busy_counter(size_t busy_counter) { busy_counter_ = busy_counter; }

  void set_fin_sent(bool fin_sent) { fin_sent_ = fin_sent; }
  void set_fin_received(bool fin_received) { fin_received_ = fin_received; }
  void set_rst_sent(bool rst_sent) { rst_sent_ = rst_sent; }

  void set_rst_received(bool rst_received) { rst_received_ = rst_received; }
  void set_stream_error(QuicRstStreamErrorCode error) { stream_error_ = error; }

  // Adjust the flow control window according to new offset in |frame|.
  virtual void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame);

  int num_frames_received() const;
  int num_duplicate_frames_received() const;

  QuicFlowController* flow_controller() { return &flow_controller_; }

  // Called when endpoint receives a frame which could increase the highest
  // offset.
  // Returns true if the highest offset did increase.
  bool MaybeIncreaseHighestReceivedOffset(QuicStreamOffset new_offset);
  // Called when bytes are sent to the peer.
  void AddBytesSent(QuicByteCount bytes);
  // Called by the stream sequencer as bytes are consumed from the buffer.
  // If the receive window has dropped below the threshold, then send a
  // WINDOW_UPDATE frame.
  void AddBytesConsumed(QuicByteCount bytes);

  // Updates the flow controller's send window offset and calls OnCanWrite if
  // it was blocked before.
  void UpdateSendWindowOffset(QuicStreamOffset new_offset);

  // Returns true if the stream has received either a RST_STREAM or a FIN -
  // either of which gives a definitive number of bytes which the peer has
  // sent. If this is not true on deletion of the stream object, the session
  // must keep track of the stream's byte offset until a definitive final value
  // arrives.
  bool HasFinalReceivedByteOffset() const {
    return fin_received_ || rst_received_;
  }

  // Returns true if the stream has queued data waiting to write.
  bool HasBufferedData() const;

  // Returns the version of QUIC being used for this stream.
  QuicTransportVersion transport_version() const;

  // Returns the crypto handshake protocol that was used on this stream's
  // connection.
  HandshakeProtocol handshake_protocol() const;

  // Sets the sequencer to consume all incoming data itself and not call
  // OnDataAvailable().
  // When the FIN is received, the stream will be notified automatically (via
  // OnFinRead()) (which may happen during the call of StopReading()).
  // TODO(dworley): There should be machinery to send a RST_STREAM/NO_ERROR and
  // stop sending stream-level flow-control updates when this end sends FIN.
  virtual void StopReading();

  // Get peer IP of the lastest packet which connection is dealing/delt with.
  virtual const QuicSocketAddress& PeerAddressOfLatestPacket() const;

  // Sends as much of 'data' to the connection as the connection will consume,
  // and then buffers any remaining data in queued_data_.
  // If fin is true: if it is immediately passed on to the session,
  // write_side_closed() becomes true, otherwise fin_buffered_ becomes true.
  void WriteOrBufferData(
      QuicStringPiece data,
      bool fin,
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener);

  // Adds random padding after the fin is consumed for this stream.
  void AddRandomPaddingAfterFin();

  // Write |data_length| of data starts at |offset| from send buffer.
  bool WriteStreamData(QuicStreamOffset offset,
                       QuicByteCount data_length,
                       QuicDataWriter* writer);

  // Called when data [offset, offset + data_length) is acked. |fin_acked|
  // indicates whether the fin is acked. Returns true if any new stream data
  // (including fin) gets acked.
  virtual bool OnStreamFrameAcked(QuicStreamOffset offset,
                                  QuicByteCount data_length,
                                  bool fin_acked,
                                  QuicTime::Delta ack_delay_time);

  // Called when data [offset, offset + data_length) was retransmitted.
  // |fin_retransmitted| indicates whether fin was retransmitted.
  virtual void OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                          QuicByteCount data_length,
                                          bool fin_retransmitted);

  // Called when data [offset, offset + data_length) is considered as lost.
  // |fin_lost| indicates whether the fin is considered as lost.
  virtual void OnStreamFrameLost(QuicStreamOffset offset,
                                 QuicByteCount data_length,
                                 bool fin_lost);

  // Called to retransmit outstanding portion in data [offset, offset +
  // data_length) and |fin|. Returns true if all data gets retransmitted.
  virtual bool RetransmitStreamData(QuicStreamOffset offset,
                                    QuicByteCount data_length,
                                    bool fin);

  // Sets deadline of this stream to be now + |ttl|, returns true if the setting
  // succeeds.
  bool MaybeSetTtl(QuicTime::Delta ttl);

  // Same as WritevData except data is provided in reference counted memory so
  // that data copy is avoided.
  QuicConsumedData WriteMemSlices(QuicMemSliceSpan span, bool fin);

  // Returns true if any stream data is lost (including fin) and needs to be
  // retransmitted.
  virtual bool HasPendingRetransmission() const;

  // Returns true if any portion of data [offset, offset + data_length) is
  // outstanding or fin is outstanding (if |fin| is true). Returns false
  // otherwise.
  bool IsStreamFrameOutstanding(QuicStreamOffset offset,
                                QuicByteCount data_length,
                                bool fin) const;

  StreamType type() const { return type_; }

 protected:
  // Sends as many bytes in the first |count| buffers of |iov| to the connection
  // as the connection will consume. If FIN is consumed, the write side is
  // immediately closed.
  // Returns the number of bytes consumed by the connection.
  // Please note: Returned consumed data is the amount of data saved in send
  // buffer. The data is not necessarily consumed by the connection. So write
  // side is closed when FIN is sent.
  // TODO(fayang): Let WritevData return boolean.
  QuicConsumedData WritevData(const struct iovec* iov, int iov_count, bool fin);

  // Allows override of the session level writev, for the force HOL
  // blocking experiment.
  virtual QuicConsumedData WritevDataInner(size_t write_length,
                                           QuicStreamOffset offset,
                                           bool fin);

  // Close the write side of the socket.  Further writes will fail.
  // Can be called by the subclass or internally.
  // Does not send a FIN.  May cause the stream to be closed.
  virtual void CloseWriteSide();

  // Close the read side of the socket.  May cause the stream to be closed.
  // Subclasses and consumers should use StopReading to terminate reading early
  // if expecting a FIN. Can be used directly by subclasses if not expecting a
  // FIN.
  void CloseReadSide();

  // Called when data of [offset, offset + data_length] is buffered in send
  // buffer.
  virtual void OnDataBuffered(
      QuicStreamOffset offset,
      QuicByteCount data_length,
      const QuicReferenceCountedPointer<QuicAckListenerInterface>&
          ack_listener) {}

  // True if buffered data in send buffer is below buffered_data_threshold_.
  bool CanWriteNewData() const;

  // Called when upper layer can write new data.
  virtual void OnCanWriteNewData() {}

  // Called when |bytes_consumed| bytes has been consumed.
  virtual void OnStreamDataConsumed(size_t bytes_consumed);

  // Writes pending retransmissions if any.
  virtual void WritePendingRetransmission();

  // This is called when stream tries to retransmit data after deadline_. Make
  // this virtual so that subclasses can implement their own logics.
  virtual void OnDeadlinePassed();

  bool fin_buffered() const { return fin_buffered_; }

  const QuicSession* session() const { return session_; }
  QuicSession* session() { return session_; }

  const QuicStreamSequencer* sequencer() const { return &sequencer_; }
  QuicStreamSequencer* sequencer() { return &sequencer_; }

  void DisableConnectionFlowControlForThisStream() {
    stream_contributes_to_connection_flow_control_ = false;
  }

  void set_ack_listener(
      QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
    ack_listener_ = std::move(ack_listener);
  }

  const QuicIntervalSet<QuicStreamOffset>& bytes_acked() const;

  const QuicStreamSendBuffer& send_buffer() const { return send_buffer_; }

  QuicStreamSendBuffer& send_buffer() { return send_buffer_; }

 private:
  friend class test::QuicStreamPeer;
  friend class QuicStreamUtils;

  // Subclasses and consumers should use reading_stopped.
  bool read_side_closed() const { return read_side_closed_; }

  // Calls MaybeSendBlocked on the stream's flow controller and the connection
  // level flow controller.  If the stream is flow control blocked by the
  // connection-level flow controller but not by the stream-level flow
  // controller, marks this stream as connection-level write blocked.
  void MaybeSendBlocked();

  // Write buffered data in send buffer. TODO(fayang): Consider combine
  // WriteOrBufferData, Writev and WriteBufferedData.
  void WriteBufferedData();

  // Returns true if deadline_ has passed.
  bool HasDeadlinePassed() const;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  // Pointer to the owning QuicSession object.
  QuicSession* session_;
  // The priority of the stream, once parsed.
  spdy::SpdyPriority priority_;
  // Bytes read refers to payload bytes only: they do not include framing,
  // encryption overhead etc.
  uint64_t stream_bytes_read_;

  // Stream error code received from a RstStreamFrame or error code sent by the
  // visitor or sequencer in the RstStreamFrame.
  QuicRstStreamErrorCode stream_error_;
  // Connection error code due to which the stream was closed. |stream_error_|
  // is set to |QUIC_STREAM_CONNECTION_ERROR| when this happens and consumers
  // should check |connection_error_|.
  QuicErrorCode connection_error_;

  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;

  // True if the subclass has written a FIN with WriteOrBufferData, but it was
  // buffered in queued_data_ rather than being sent to the session.
  bool fin_buffered_;
  // True if a FIN has been sent to the session.
  bool fin_sent_;
  // True if a FIN is waiting to be acked.
  bool fin_outstanding_;
  // True if a FIN is lost.
  bool fin_lost_;

  // True if this stream has received (and the sequencer has accepted) a
  // StreamFrame with the FIN set.
  bool fin_received_;

  // True if an RST_STREAM has been sent to the session.
  // In combination with fin_sent_, used to ensure that a FIN and/or a
  // RST_STREAM is always sent to terminate the stream.
  bool rst_sent_;

  // True if this stream has received a RST_STREAM frame.
  bool rst_received_;

  // Tracks if the session this stream is running under was created by a
  // server or a client.
  Perspective perspective_;

  QuicFlowController flow_controller_;

  // The connection level flow controller. Not owned.
  QuicFlowController* connection_flow_controller_;

  // Special streams, such as the crypto and headers streams, do not respect
  // connection level flow control limits (but are stream level flow control
  // limited).
  bool stream_contributes_to_connection_flow_control_;

  // A counter incremented when OnCanWrite() is called and no progress is made.
  // For debugging only.
  size_t busy_counter_;

  // Indicates whether paddings will be added after the fin is consumed for this
  // stream.
  bool add_random_padding_after_fin_;

  // Ack listener of this stream, and it is notified when any of written bytes
  // are acked.
  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener_;

  // Send buffer of this stream. Send buffer is cleaned up when data gets acked
  // or discarded.
  QuicStreamSendBuffer send_buffer_;

  // Latched value of FLAGS_quic_buffered_data_threshold.
  const QuicByteCount buffered_data_threshold_;

  // If true, then this stream has precedence over other streams for write
  // scheduling.
  const bool is_static_;

  // If initialized, reset this stream at this deadline.
  QuicTime deadline_;

  // Indicates whether this stream is bidirectional, read unidirectional or
  // write unidirectional.
  const StreamType type_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_H_
