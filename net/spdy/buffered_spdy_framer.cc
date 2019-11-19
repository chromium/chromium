// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace net {

namespace {

// GOAWAY frame debug data is only buffered up to this many bytes.
size_t kGoAwayDebugDataMaxSize = 1024;

}  // namespace

BufferedSpdyFramer::BufferedSpdyFramer(uint32_t max_header_list_size,
                                       const NetLogWithSource& net_log,
                                       TimeFunc time_func)
    : spdy_framer_(spdy::SpdyFramer::ENABLE_COMPRESSION),
      visitor_(nullptr),
      frames_received_(0),
      max_header_list_size_(max_header_list_size),
      net_log_(net_log),
      time_func_(time_func) {
  // Do not bother decoding response header payload above the limit.
  deframer_.GetHpackDecoder()->set_max_decode_buffer_size_bytes(
      max_header_list_size_);
}

BufferedSpdyFramer::~BufferedSpdyFramer() = default;

void BufferedSpdyFramer::set_visitor(
    BufferedSpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
  deframer_.set_visitor(this);
}

void BufferedSpdyFramer::set_debug_visitor(
    spdy::SpdyFramerDebugVisitorInterface* debug_visitor) {
  spdy_framer_.set_debug_visitor(debug_visitor);
  deframer_.set_debug_visitor(debug_visitor);
}

void BufferedSpdyFramer::OnError(
    http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error) {
  visitor_->OnError(spdy_framer_error);
}

void BufferedSpdyFramer::OnHeaders(spdy::SpdyStreamId stream_id,
                                   bool has_priority,
                                   int weight,
                                   spdy::SpdyStreamId parent_stream_id,
                                   bool exclusive,
                                   bool fin,
                                   bool end) {
  frames_received_++;
  DCHECK(!control_frame_fields_.get());
  control_frame_fields_ = std::make_unique<ControlFrameFields>();
  control_frame_fields_->type = spdy::SpdyFrameType::HEADERS;
  control_frame_fields_->stream_id = stream_id;
  control_frame_fields_->has_priority = has_priority;
  if (control_frame_fields_->has_priority) {
    control_frame_fields_->weight = weight;
    control_frame_fields_->parent_stream_id = parent_stream_id;
    control_frame_fields_->exclusive = exclusive;
  }
  control_frame_fields_->fin = fin;
  control_frame_fields_->recv_first_byte_time = time_func_();
}

void BufferedSpdyFramer::OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                                           size_t length,
                                           bool fin) {
  frames_received_++;
  visitor_->OnDataFrameHeader(stream_id, length, fin);
}

void BufferedSpdyFramer::OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                           const char* data,
                                           size_t len) {
  visitor_->OnStreamFrameData(stream_id, data, len);
}

void BufferedSpdyFramer::OnStreamEnd(spdy::SpdyStreamId stream_id) {
  visitor_->OnStreamEnd(stream_id);
}

void BufferedSpdyFramer::OnStreamPadLength(spdy::SpdyStreamId stream_id,
                                           size_t value) {
  // Deliver the stream pad length byte for flow control handling.
  visitor_->OnStreamPadding(stream_id, 1);
}

void BufferedSpdyFramer::OnStreamPadding(spdy::SpdyStreamId stream_id,
                                         size_t len) {
  visitor_->OnStreamPadding(stream_id, len);
}

spdy::SpdyHeadersHandlerInterface* BufferedSpdyFramer::OnHeaderFrameStart(
    spdy::SpdyStreamId stream_id) {
  coalescer_ =
      std::make_unique<HeaderCoalescer>(max_header_list_size_, net_log_);
  return coalescer_.get();
}

void BufferedSpdyFramer::OnHeaderFrameEnd(spdy::SpdyStreamId stream_id) {
  if (coalescer_->error_seen()) {
    visitor_->OnStreamError(stream_id,
                            "Could not parse Spdy Control Frame Header.");
    control_frame_fields_.reset();
    return;
  }
  DCHECK(control_frame_fields_.get());
  switch (control_frame_fields_->type) {
    case spdy::SpdyFrameType::HEADERS:
      visitor_->OnHeaders(
          control_frame_fields_->stream_id, control_frame_fields_->has_priority,
          control_frame_fields_->weight,
          control_frame_fields_->parent_stream_id,
          control_frame_fields_->exclusive, control_frame_fields_->fin,
          coalescer_->release_headers(),
          control_frame_fields_->recv_first_byte_time);
      break;
    case spdy::SpdyFrameType::PUSH_PROMISE:
      visitor_->OnPushPromise(control_frame_fields_->stream_id,
                              control_frame_fields_->promised_stream_id,
                              coalescer_->release_headers());
      break;
    default:
      DCHECK(false) << "Unexpect control frame type: "
                    << control_frame_fields_->type;
      break;
  }
  control_frame_fields_.reset(nullptr);
}

void BufferedSpdyFramer::OnSettings() {
  visitor_->OnSettings();
}

void BufferedSpdyFramer::OnSetting(spdy::SpdySettingsId id, uint32_t value) {
  visitor_->OnSetting(id, value);
}

void BufferedSpdyFramer::OnSettingsAck() {
  visitor_->OnSettingsAck();
}

void BufferedSpdyFramer::OnSettingsEnd() {
  visitor_->OnSettingsEnd();
}

void BufferedSpdyFramer::OnPing(spdy::SpdyPingId unique_id, bool is_ack) {
  visitor_->OnPing(unique_id, is_ack);
}

void BufferedSpdyFramer::OnRstStream(spdy::SpdyStreamId stream_id,
                                     spdy::SpdyErrorCode error_code) {
  visitor_->OnRstStream(stream_id, error_code);
}
void BufferedSpdyFramer::OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                                  spdy::SpdyErrorCode error_code) {
  DCHECK(!goaway_fields_);
  goaway_fields_ = std::make_unique<GoAwayFields>();
  goaway_fields_->last_accepted_stream_id = last_accepted_stream_id;
  goaway_fields_->error_code = error_code;
}

bool BufferedSpdyFramer::OnGoAwayFrameData(const char* goaway_data,
                                           size_t len) {
  if (len > 0) {
    if (goaway_fields_->debug_data.size() < kGoAwayDebugDataMaxSize) {
      goaway_fields_->debug_data.append(
          goaway_data, std::min(len, kGoAwayDebugDataMaxSize -
                                         goaway_fields_->debug_data.size()));
    }
    return true;
  }
  visitor_->OnGoAway(goaway_fields_->last_accepted_stream_id,
                     goaway_fields_->error_code, goaway_fields_->debug_data);
  goaway_fields_.reset();
  return true;
}

void BufferedSpdyFramer::OnWindowUpdate(spdy::SpdyStreamId stream_id,
                                        int delta_window_size) {
  visitor_->OnWindowUpdate(stream_id, delta_window_size);
}

void BufferedSpdyFramer::OnPushPromise(spdy::SpdyStreamId stream_id,
                                       spdy::SpdyStreamId promised_stream_id,
                                       bool end) {
  frames_received_++;
  DCHECK(!control_frame_fields_.get());
  control_frame_fields_ = std::make_unique<ControlFrameFields>();
  control_frame_fields_->type = spdy::SpdyFrameType::PUSH_PROMISE;
  control_frame_fields_->stream_id = stream_id;
  control_frame_fields_->promised_stream_id = promised_stream_id;
  control_frame_fields_->recv_first_byte_time = time_func_();
}

void BufferedSpdyFramer::OnAltSvc(
    spdy::SpdyStreamId stream_id,
    base::StringPiece origin,
    const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector& altsvc_vector) {
  visitor_->OnAltSvc(stream_id, origin, altsvc_vector);
}

void BufferedSpdyFramer::OnContinuation(spdy::SpdyStreamId stream_id,
                                        bool end) {}

bool BufferedSpdyFramer::OnUnknownFrame(spdy::SpdyStreamId stream_id,
                                        uint8_t frame_type) {
  return visitor_->OnUnknownFrame(stream_id, frame_type);
}

size_t BufferedSpdyFramer::ProcessInput(const char* data, size_t len) {
  return deframer_.ProcessInput(data, len);
}

void BufferedSpdyFramer::UpdateHeaderDecoderTableSize(uint32_t value) {
  deframer_.GetHpackDecoder()->ApplyHeaderTableSizeSetting(value);
}

void BufferedSpdyFramer::Reset() {
  deframer_.Reset();
}

http2::Http2DecoderAdapter::SpdyFramerError
BufferedSpdyFramer::spdy_framer_error() const {
  return deframer_.spdy_framer_error();
}

http2::Http2DecoderAdapter::SpdyState BufferedSpdyFramer::state() const {
  return deframer_.state();
}

bool BufferedSpdyFramer::MessageFullyRead() {
  return state() == http2::Http2DecoderAdapter::SPDY_FRAME_COMPLETE;
}

bool BufferedSpdyFramer::HasError() {
  return deframer_.HasError();
}

// TODO(jgraettinger): Eliminate uses of this method (prefer
// spdy::SpdyRstStreamIR).
std::unique_ptr<spdy::SpdySerializedFrame> BufferedSpdyFramer::CreateRstStream(
    spdy::SpdyStreamId stream_id,
    spdy::SpdyErrorCode error_code) const {
  spdy::SpdyRstStreamIR rst_ir(stream_id, error_code);
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializeRstStream(rst_ir));
}

// TODO(jgraettinger): Eliminate uses of this method (prefer
// spdy::SpdySettingsIR).
std::unique_ptr<spdy::SpdySerializedFrame> BufferedSpdyFramer::CreateSettings(
    const spdy::SettingsMap& values) const {
  spdy::SpdySettingsIR settings_ir;
  for (auto it = values.begin(); it != values.end(); ++it) {
    settings_ir.AddSetting(it->first, it->second);
  }
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializeSettings(settings_ir));
}

// TODO(jgraettinger): Eliminate uses of this method (prefer spdy::SpdyPingIR).
std::unique_ptr<spdy::SpdySerializedFrame> BufferedSpdyFramer::CreatePingFrame(
    spdy::SpdyPingId unique_id,
    bool is_ack) const {
  spdy::SpdyPingIR ping_ir(unique_id);
  ping_ir.set_is_ack(is_ack);
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializePing(ping_ir));
}

// TODO(jgraettinger): Eliminate uses of this method (prefer
// spdy::SpdyWindowUpdateIR).
std::unique_ptr<spdy::SpdySerializedFrame>
BufferedSpdyFramer::CreateWindowUpdate(spdy::SpdyStreamId stream_id,
                                       uint32_t delta_window_size) const {
  spdy::SpdyWindowUpdateIR update_ir(stream_id, delta_window_size);
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializeWindowUpdate(update_ir));
}

// TODO(jgraettinger): Eliminate uses of this method (prefer spdy::SpdyDataIR).
std::unique_ptr<spdy::SpdySerializedFrame> BufferedSpdyFramer::CreateDataFrame(
    spdy::SpdyStreamId stream_id,
    const char* data,
    uint32_t len,
    spdy::SpdyDataFlags flags) {
  spdy::SpdyDataIR data_ir(stream_id, base::StringPiece(data, len));
  data_ir.set_fin((flags & spdy::DATA_FLAG_FIN) != 0);
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializeData(data_ir));
}

// TODO(jgraettinger): Eliminate uses of this method (prefer
// spdy::SpdyPriorityIR).
std::unique_ptr<spdy::SpdySerializedFrame> BufferedSpdyFramer::CreatePriority(
    spdy::SpdyStreamId stream_id,
    spdy::SpdyStreamId dependency_id,
    int weight,
    bool exclusive) const {
  spdy::SpdyPriorityIR priority_ir(stream_id, dependency_id, weight, exclusive);
  return std::make_unique<spdy::SpdySerializedFrame>(
      spdy_framer_.SerializePriority(priority_ir));
}

size_t BufferedSpdyFramer::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(spdy_framer_) +
         base::trace_event::EstimateMemoryUsage(deframer_) +
         base::trace_event::EstimateMemoryUsage(coalescer_) +
         base::trace_event::EstimateMemoryUsage(control_frame_fields_) +
         base::trace_event::EstimateMemoryUsage(goaway_fields_);
}

BufferedSpdyFramer::ControlFrameFields::ControlFrameFields() = default;

size_t BufferedSpdyFramer::GoAwayFields::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(debug_data);
}

}  // namespace net
