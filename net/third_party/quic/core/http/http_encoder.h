// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_ENCODER_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_ENCODER_H_

#include <cstddef>

#include "net/third_party/quic/core/http/http_frames.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicDataWriter;

// A class for encoding the HTTP frames that are exchanged in an HTTP over QUIC
// session.
class QUIC_EXPORT_PRIVATE HttpEncoder {
 public:
  HttpEncoder();

  ~HttpEncoder();

  // Serializes the header of a DATA frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeDataFrameHeader(const DataFrame& data,
                                         std::unique_ptr<char[]>* output);

  // Serializes the header of a HEADERS frame into a new buffer stored in
  // |output|. Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeHeadersFrameHeader(const HeadersFrame& headers,
                                            std::unique_ptr<char[]>* output);

  // Serializes a PRIORITY frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializePriorityFrame(const PriorityFrame& priority,
                                       std::unique_ptr<char[]>* output);

  // Serializes a CANCEL_PUSH frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeCancelPushFrame(const CancelPushFrame& cancel_push,
                                         std::unique_ptr<char[]>* output);

  // Serializes a SETTINGS frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeSettingsFrame(const SettingsFrame& settings,
                                       std::unique_ptr<char[]>* output);

  // Serializes the header and push_id of a PUSH_PROMISE frame into a new buffer
  // stored in |output|. Returns the length of the buffer on success, or 0
  // otherwise.
  QuicByteCount SerializePushPromiseFrameWithOnlyPushId(
      const PushPromiseFrame& push_promise,
      std::unique_ptr<char[]>* output);

  // Serializes a GOAWAY frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeGoAwayFrame(const GoAwayFrame& goaway,
                                     std::unique_ptr<char[]>* output);

  // Serializes a MAX_PUSH frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeMaxPushIdFrame(const MaxPushIdFrame& max_push_id,
                                        std::unique_ptr<char[]>* output);

 private:
  bool WriteFrameHeader(QuicByteCount length,
                        HttpFrameType type,
                        QuicDataWriter* writer);

  QuicByteCount GetTotalLength(QuicByteCount payload_length);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_ENCODER_H_
