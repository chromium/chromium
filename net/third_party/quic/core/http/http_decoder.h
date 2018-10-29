// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_DECODER_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_DECODER_H_

#include <cstddef>

#include "net/third_party/quic/core/http/http_frames.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicDataReader;

// A class for decoding the HTTP frames that are exchanged in an HTTP over QUIC
// session.
class QUIC_EXPORT_PRIVATE HttpDecoder {
 public:
  class QUIC_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}

    // Called if an error is detected.
    virtual void OnError(HttpDecoder* decoder) = 0;

    // Called when a PRIORITY frame has been successfully parsed.
    virtual void OnPriorityFrame(const PriorityFrame& frame) = 0;

    // Called when a CANCEL_PUSH frame has been successfully parsed.
    virtual void OnCancelPushFrame(const CancelPushFrame& frame) = 0;

    // Called when a MAX_PUSH_ID frame has been successfully parsed.
    virtual void OnMaxPushIdFrame(const MaxPushIdFrame& frame) = 0;

    // Called when a GOAWAY frame has been successfully parsed.
    virtual void OnGoAwayFrame(const GoAwayFrame& frame) = 0;

    // Called when a SETTINGS frame has been successfully parsed.
    virtual void OnSettingsFrame(const SettingsFrame& frame) = 0;

    // Called when a DATA frame has been recevied.
    virtual void OnDataFrameStart() = 0;
    // Called when the payload of a DATA frame has read. May be called
    // multiple times for a single frame.
    virtual void OnDataFramePayload(QuicStringPiece payload) = 0;
    // Called when a DATA frame has been completely processed.
    virtual void OnDataFrameEnd() = 0;

    // Called when a HEADERS frame has been recevied.
    virtual void OnHeadersFrameStart() = 0;
    // Called when the payload of a HEADERS frame has read. May be called
    // multiple times for a single frame.
    virtual void OnHeadersFramePayload(QuicStringPiece payload) = 0;
    // Called when a HEADERS frame has been completely processed.
    virtual void OnHeadersFrameEnd() = 0;

    // Called when a PUSH_PROMISE frame has been recevied for |push_id|.
    virtual void OnPushPromiseFrameStart(PushId push_id) = 0;
    // Called when the payload of a PUSH_PROMISE frame has read. May be called
    // multiple times for a single frame.
    virtual void OnPushPromiseFramePayload(QuicStringPiece payload) = 0;
    // Called when a PUSH_PROMISE frame has been completely processed.
    virtual void OnPushPromiseFrameEnd() = 0;

    // TODO(rch): Consider adding methods like:
    // OnUnknownFrame{Start,Payload,End}()
    // to allow callers to handle unknown frames.
  };

  HttpDecoder();

  ~HttpDecoder();

  // Set callbacks to be called from the decoder.  A visitor must be set, or
  // else the decoder will crash.  It is acceptable for the visitor to do
  // nothing.  If this is called multiple times, only the last visitor
  // will be used.  |visitor| will be owned by the caller.
  void set_visitor(Visitor* visitor) { visitor_ = visitor; }

  // Processes the input and invokes the visitor for any frames.
  // Returns the number of bytes consumed, or 0 if there was an error, in which
  // case error() should be consulted.
  size_t ProcessInput(const char* data, size_t len);

  QuicErrorCode error() const { return error_; }
  const QuicString& error_detail() const { return error_detail_; }

 private:
  // Represents the current state of the parsing state machine.
  enum HttpDecoderState {
    STATE_READING_FRAME_LENGTH,
    STATE_READING_FRAME_TYPE,
    STATE_READING_FRAME_PAYLOAD,
    STATE_ERROR
  };

  // Reads the length of a frame from |reader|. Sets error_ and error_detail_
  // if there are any errors.
  void ReadFrameLength(QuicDataReader* reader);

  // Reads the type of a frame from |reader|. Sets error_ and error_detail_
  // if there are any errors.
  void ReadFrameType(QuicDataReader* reader);

  // Reads the payload of the current frame from |reader| and processes it,
  // possibly buffering the data or invoking the visitor.
  void ReadFramePayload(QuicDataReader* reader);

  // Discards any remaining frame payload from |reader|.
  void DiscardFramePayload(QuicDataReader* reader);

  // Buffers any remaining frame payload from |reader| into |buffer_|.
  void BufferFramePayload(QuicDataReader* reader);

  // Sets |error_| and |error_detail_| accordingly.
  void RaiseError(QuicErrorCode error, QuicString error_detail);

  // Parses the payload of a PRIORITY frame from |reader| into |frame|.
  bool ParsePriorityFrame(QuicDataReader* reader, PriorityFrame* frame);

  // Parses the payload of a SETTINGS frame from |reader| into |frame|.
  bool ParseSettingsFrame(QuicDataReader* reader, SettingsFrame* frame);

  // Visitor to invoke when messages are parsed.
  Visitor* visitor_;  // Unowned.
  // Current state of the parsing.
  HttpDecoderState state_;
  // Type of the frame currently being parsed.
  uint8_t current_frame_type_;
  // Length of the payload of the frame currently being parsed.
  uint64_t current_frame_length_;
  // Remaining payload bytes to be parsed.
  uint64_t remaining_frame_length_;
  // Last error.
  QuicErrorCode error_;
  // The issue which caused |error_|
  QuicString error_detail_;
  // Remaining unparsed data.
  QuicString buffer_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_DECODER_H_
