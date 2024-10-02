// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#define NET_SPDY_BUFFERED_SPDY_FRAMER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/log/net_log_source.h"
#include "net/spdy/header_coalescer.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/http2_frame_decoder_adapter.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_alt_svc_wire_format.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE BufferedSpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramerVisitorInterface() = default;

  BufferedSpdyFramerVisitorInterface(
      const BufferedSpdyFramerVisitorInterface&) = delete;
  BufferedSpdyFramerVisitorInterface& operator=(
      const BufferedSpdyFramerVisitorInterface&) = delete;

  // Called if an error is detected in the spdy::SpdySerializedFrame protocol.
  virtual void OnError(
      http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error) = 0;

  // Called if an error is detected in a HTTP2 stream.
  virtual void OnStreamError(spdy::SpdyStreamId stream_id,
                             const std::string& description) = 0;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(spdy::SpdyStreamId stream_id,
                         bool has_priority,
                         int weight,
                         spdy::SpdyStreamId parent_stream_id,
                         bool exclusive,
                         bool fin,
                         quiche::HttpHeaderBlock headers,
                         base::TimeTicks recv_first_byte_time) = 0;

  // Called when a data frame header is received.
  virtual void OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer (at most 2^16 - 1 - 8).
  virtual void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) = 0;

  // Called when the other side has finished sending data on this stream.
  // |stream_id| The stream that was receivin data.
  virtual void OnStreamEnd(spdy::SpdyStreamId stream_id) = 0;

  // Called when padding is received (padding length field or padding octets).
  // |stream_id| The stream receiving data.
  // |len| The number of padding octets.
  virtual void OnStreamPadding(spdy::SpdyStreamId stream_id, size_t len) = 0;

  // Called when a SETTINGS frame is received.
  virtual void OnSettings() = 0;

  // Called when an individual setting within a SETTINGS frame has been parsed.
  // Note that |id| may or may not be a SETTINGS ID defined in the HTTP/2 spec.
  virtual void OnSetting(spdy::SpdySettingsId id, uint32_t value) = 0;

  // Called when a SETTINGS frame is received with the ACK flag set.
  virtual void OnSettingsAck() = 0;

  // Called at the completion of parsing SETTINGS id and value tuples.
  virtual void OnSettingsEnd() = 0;

  // Called when a PING frame has been parsed.
  virtual void OnPing(spdy::SpdyPingId unique_id, bool is_ack) = 0;

  // Called when a RST_STREAM frame has been parsed.
  virtual void OnRstStream(spdy::SpdyStreamId stream_id,
                           spdy::SpdyErrorCode error_code) = 0;

  // Called when a GOAWAY frame has been parsed.
  virtual void OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                        spdy::SpdyErrorCode error_code,
                        std::string_view debug_data) = 0;

  // Called when a WINDOW_UPDATE frame has been parsed.
  virtual void OnWindowUpdate(spdy::SpdyStreamId stream_id,
                              int delta_window_size) = 0;

  // Called when a PUSH_PROMISE frame has been parsed.
  virtual void OnPushPromise(spdy::SpdyStreamId stream_id,
                             spdy::SpdyStreamId promised_stream_id,
                             quiche::HttpHeaderBlock headers) = 0;

  // Called when an ALTSVC frame has been parsed.
  virtual void OnAltSvc(
      spdy::SpdyStreamId stream_id,
      std::string_view origin,
      const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
          altsvc_vector) = 0;

  // Called when a frame type we don't recognize is received.
  // Return true if this appears to be a valid extension frame, false otherwise.
  // We distinguish between extension frames and nonsense by checking
  // whether the stream id is valid.
  virtual bool OnUnknownFrame(spdy::SpdyStreamId stream_id,
                              uint8_t frame_type) = 0;

 protected:
  virtual ~BufferedSpdyFramerVisitorInterface() = default;
};

class NET_EXPORT_PRIVATE BufferedSpdyFramer
    : public spdy::SpdyFramerVisitorInterface {
 public:
  using TimeFunc = base::TimeTicks (*)();

  BufferedSpdyFramer(uint32_t max_header_list_size,
                     const NetLogWithSource& net_log,
                     TimeFunc time_func = base::TimeTicks::Now);
  BufferedSpdyFramer() = delete;

  BufferedSpdyFramer(const BufferedSpdyFramer&) = delete;
  BufferedSpdyFramer& operator=(const BufferedSpdyFramer&) = delete;

  ~BufferedSpdyFramer() override;

  // Sets callbacks to be called from the buffered spdy framer.  A visitor must
  // be set, or else the framer will likely crash.  It is acceptable for the
  // visitor to do nothing.  If this is called multiple times, only the last
  // visitor will be used.
  void set_visitor(BufferedSpdyFramerVisitorInterface* visitor);

  // Set debug callbacks to be called from the framer. The debug visitor is
  // completely optional and need not be set in order for normal operation.
  // If this is called multiple times, only the last visitor will be used.
  void set_debug_visitor(spdy::SpdyFramerDebugVisitorInterface* debug_visitor);

  // spdy::SpdyFramerVisitorInterface
  void OnError(http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error,
               std::string detailed_error) override;
  void OnHeaders(spdy::SpdyStreamId stream_id,
                 size_t payload_length,
                 bool has_priority,
                 int weight,
                 spdy::SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 bool end) override;
  void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override;
  void OnStreamEnd(spdy::SpdyStreamId stream_id) override;
  void OnStreamPadLength(spdy::SpdyStreamId stream_id, size_t value) override;
  void OnStreamPadding(spdy::SpdyStreamId stream_id, size_t len) override;
  spdy::SpdyHeadersHandlerInterface* OnHeaderFrameStart(
      spdy::SpdyStreamId stream_id) override;
  void OnHeaderFrameEnd(spdy::SpdyStreamId stream_id) override;
  void OnSettings() override;
  void OnSetting(spdy::SpdySettingsId id, uint32_t value) override;
  void OnSettingsAck() override;
  void OnSettingsEnd() override;
  void OnPing(spdy::SpdyPingId unique_id, bool is_ack) override;
  void OnRstStream(spdy::SpdyStreamId stream_id,
                   spdy::SpdyErrorCode error_code) override;
  void OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                spdy::SpdyErrorCode error_code) override;
  bool OnGoAwayFrameData(const char* goaway_data, size_t len) override;
  void OnWindowUpdate(spdy::SpdyStreamId stream_id,
                      int delta_window_size) override;
  void OnPushPromise(spdy::SpdyStreamId stream_id,
                     spdy::SpdyStreamId promised_stream_id,
                     bool end) override;
  void OnAltSvc(spdy::SpdyStreamId stream_id,
                std::string_view origin,
                const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override;
  void OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override;
  void OnContinuation(spdy::SpdyStreamId stream_id,
                      size_t payload_length,
                      bool end) override;
  void OnPriority(spdy::SpdyStreamId stream_id,
                  spdy::SpdyStreamId parent_stream_id,
                  int weight,
                  bool exclusive) override {}
  void OnPriorityUpdate(spdy::SpdyStreamId prioritized_stream_id,
                        std::string_view priority_field_value) override {}
  bool OnUnknownFrame(spdy::SpdyStreamId stream_id,
                      uint8_t frame_type) override;
  void OnUnknownFrameStart(spdy::SpdyStreamId stream_id,
                           size_t length,
                           uint8_t type,
                           uint8_t flags) override {}
  void OnUnknownFramePayload(spdy::SpdyStreamId stream_id,
                             std::string_view payload) override {}

  // spdy::SpdyFramer methods.
  size_t ProcessInput(const char* data, size_t len);
  void UpdateHeaderDecoderTableSize(uint32_t value);
  http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error() const;
  http2::Http2DecoderAdapter::SpdyState state() const;
  bool MessageFullyRead();
  bool HasError();
  std::unique_ptr<spdy::SpdySerializedFrame> CreateRstStream(
      spdy::SpdyStreamId stream_id,
      spdy::SpdyErrorCode error_code) const;
  std::unique_ptr<spdy::SpdySerializedFrame> CreateSettings(
      const spdy::SettingsMap& values) const;
  std::unique_ptr<spdy::SpdySerializedFrame> CreatePingFrame(
      spdy::SpdyPingId unique_id,
      bool is_ack) const;
  std::unique_ptr<spdy::SpdySerializedFrame> CreateWindowUpdate(
      spdy::SpdyStreamId stream_id,
      uint32_t delta_window_size) const;
  std::unique_ptr<spdy::SpdySerializedFrame> CreateDataFrame(
      spdy::SpdyStreamId stream_id,
      const char* data,
      uint32_t len,
      spdy::SpdyDataFlags flags);
  std::unique_ptr<spdy::SpdySerializedFrame> CreatePriority(
      spdy::SpdyStreamId stream_id,
      spdy::SpdyStreamId dependency_id,
      int weight,
      bool exclusive) const;

  // Serialize a frame of unknown type.
  spdy::SpdySerializedFrame SerializeFrame(const spdy::SpdyFrameIR& frame) {
    return spdy_framer_.SerializeFrame(frame);
  }

  int frames_received() const { return frames_received_; }

  // Updates the maximum size of the header encoder compression table.
  void UpdateHeaderEncoderTableSize(uint32_t value);
  // Returns the maximum size of the header encoder compression table.
  uint32_t header_encoder_table_size() const;

 private:
  spdy::SpdyFramer spdy_framer_;
  http2::Http2DecoderAdapter deframer_;
  raw_ptr<BufferedSpdyFramerVisitorInterface> visitor_ = nullptr;

  int frames_received_ = 0;

  // Collection of fields from control frames that we need to
  // buffer up from the spdy framer.
  struct ControlFrameFields {
    ControlFrameFields();

    spdy::SpdyFrameType type;
    spdy::SpdyStreamId stream_id = 0U;
    spdy::SpdyStreamId associated_stream_id = 0U;
    spdy::SpdyStreamId promised_stream_id = 0U;
    bool has_priority = false;
    spdy::SpdyPriority priority = 0U;
    int weight = 0;
    spdy::SpdyStreamId parent_stream_id = 0U;
    bool exclusive = false;
    bool fin = false;
    bool unidirectional = false;
    base::TimeTicks recv_first_byte_time;
  };
  std::unique_ptr<ControlFrameFields> control_frame_fields_;

  // Collection of fields of a GOAWAY frame that this class needs to buffer.
  struct GoAwayFields {
    spdy::SpdyStreamId last_accepted_stream_id;
    spdy::SpdyErrorCode error_code;
    std::string debug_data;
  };
  std::unique_ptr<GoAwayFields> goaway_fields_;

  std::unique_ptr<HeaderCoalescer> coalescer_;

  const uint32_t max_header_list_size_;
  NetLogWithSource net_log_;
  TimeFunc time_func_;
};

}  // namespace net

#endif  // NET_SPDY_BUFFERED_SPDY_FRAMER_H_
