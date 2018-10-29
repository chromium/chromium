// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_FRAMES_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_FRAMES_H_

#include <map>

#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/spdy/core/spdy_framer.h"

namespace quic {

enum class HttpFrameType : uint8_t {
  DATA = 0x0,
  HEADERS = 0x1,
  PRIORITY = 0X2,
  CANCEL_PUSH = 0X3,
  SETTINGS = 0x4,
  PUSH_PROMISE = 0x5,
  GOAWAY = 0x7,
  MAX_PUSH_ID = 0xD
};

// 4.2.2.  DATA
//
//   DATA frames (type=0x0) convey arbitrary, variable-length sequences of
//   octets associated with an HTTP request or response payload.
struct DataFrame {
  QuicStringPiece data;
};

// 4.2.3.  HEADERS
//
//   The HEADERS frame (type=0x1) is used to carry a header block,
//   compressed using QPACK.
struct HeadersFrame {
  QuicStringPiece headers;
};

// 4.2.4.  PRIORITY
//
//   The PRIORITY (type=0x02) frame specifies the sender-advised priority
//   of a stream
enum PriorityElementType {
  REQUEST_STREAM = 0,
  PUSH_STREAM = 1,
  PLACEHOLDER = 2,
  ROOT_OF_TREE = 3
};

struct PriorityFrame {
  PriorityElementType prioritized_type;
  PriorityElementType dependency_type;
  bool exclusive;
  uint64_t prioritized_element_id;
  uint64_t element_dependency_id;
  uint8_t weight;

  bool operator==(const PriorityFrame& rhs) const {
    return prioritized_type == rhs.prioritized_type &&
           dependency_type == rhs.dependency_type &&
           exclusive == rhs.exclusive &&
           prioritized_element_id == rhs.prioritized_element_id &&
           element_dependency_id == rhs.element_dependency_id &&
           weight == rhs.weight;
  }
};

// 4.2.5.  CANCEL_PUSH
//
//   The CANCEL_PUSH frame (type=0x3) is used to request cancellation of
//   server push prior to the push stream being created.
using PushId = uint64_t;

struct CancelPushFrame {
  PushId push_id;

  bool operator==(const CancelPushFrame& rhs) const {
    return push_id == rhs.push_id;
  }
};

// 4.2.6.  SETTINGS
//
//   The SETTINGS frame (type=0x4) conveys configuration parameters that
//   affect how endpoints communicate, such as preferences and constraints
//   on peer behavior

using SettingsId = uint16_t;
using SettingsMap = std::map<SettingsId, uint64_t>;

struct SettingsFrame {
  SettingsMap values;

  bool operator==(const SettingsFrame& rhs) const {
    return values == rhs.values;
  }
};

// 4.2.7.  PUSH_PROMISE
//
//   The PUSH_PROMISE frame (type=0x05) is used to carry a request header
//   set from server to client, as in HTTP/2.
struct PushPromiseFrame {
  PushId push_id;
  QuicStringPiece headers;

  bool operator==(const PushPromiseFrame& rhs) const {
    return push_id == rhs.push_id && headers == rhs.headers;
  }
};

// 4.2.8.  GOAWAY
//
//   The GOAWAY frame (type=0x7) is used to initiate graceful shutdown of
//   a connection by a server.
struct GoAwayFrame {
  QuicStreamId stream_id;

  bool operator==(const GoAwayFrame& rhs) const {
    return stream_id == rhs.stream_id;
  }
};

// 4.2.9.  MAX_PUSH_ID
//
//   The MAX_PUSH_ID frame (type=0xD) is used by clients to control the
//   number of server pushes that the server can initiate.
struct MaxPushIdFrame {
  PushId push_id;

  bool operator==(const MaxPushIdFrame& rhs) const {
    return push_id == rhs.push_id;
  }
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_HTTP_FRAMES_H_
