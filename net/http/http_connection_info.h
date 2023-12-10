// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CONNECTION_INFO_H_
#define NET_HTTP_HTTP_CONNECTION_INFO_H_

#include <string_view>

#include "net/base/net_export.h"

namespace net {

// Describes the kind of connection used to fetch this response.
//
// NOTE: Please keep in sync with ConnectionInfo enum in
// tools/metrics/histograms/metadata/net/enums.xml.
// Because of that, and also because these values are persisted to
// the cache, please make sure not to delete or reorder values.
enum class HttpConnectionInfo {
  kUNKNOWN = 0,
  kHTTP1_1 = 1,
  kDEPRECATED_SPDY2 = 2,
  kDEPRECATED_SPDY3 = 3,
  kHTTP2 = 4,  // HTTP/2.
  kQUIC_UNKNOWN_VERSION = 5,
  kDEPRECATED_HTTP2_14 = 6,  // HTTP/2 draft-14.
  kDEPRECATED_HTTP2_15 = 7,  // HTTP/2 draft-15.
  kHTTP0_9 = 8,
  kHTTP1_0 = 9,
  kQUIC_32 = 10,
  kQUIC_33 = 11,
  kQUIC_34 = 12,
  kQUIC_35 = 13,
  kQUIC_36 = 14,
  kQUIC_37 = 15,
  kQUIC_38 = 16,
  kQUIC_39 = 17,
  kQUIC_40 = 18,
  kQUIC_41 = 19,
  kQUIC_42 = 20,
  kQUIC_43 = 21,
  kQUIC_Q099 = 22,
  kQUIC_44 = 23,
  kQUIC_45 = 24,
  kQUIC_46 = 25,
  kQUIC_47 = 26,
  kQUIC_999 = 27,
  kQUIC_Q048 = 28,
  kQUIC_Q049 = 29,
  kQUIC_Q050 = 30,
  kQUIC_T048 = 31,
  kQUIC_T049 = 32,
  kQUIC_T050 = 33,
  kQUIC_T099 = 34,
  kQUIC_DRAFT_25 = 35,
  kQUIC_DRAFT_27 = 36,
  kQUIC_DRAFT_28 = 37,
  kQUIC_DRAFT_29 = 38,
  kQUIC_T051 = 39,
  kQUIC_RFC_V1 = 40,
  kDEPRECATED_QUIC_2_DRAFT_1 = 41,
  kQUIC_2_DRAFT_8 = 42,

  kMaxValue = kQUIC_2_DRAFT_8,
};

enum class HttpConnectionInfoCoarse {
  kHTTP1,  // HTTP/0.9, 1.0 and 1.1
  kHTTP2,
  kQUIC,
  kOTHER,
};

NET_EXPORT std::string_view HttpConnectionInfoToString(
    HttpConnectionInfo connection_info);

NET_EXPORT std::string_view HttpConnectionInfoCoarseToString(
    HttpConnectionInfoCoarse http_connection_info_coarse);

// Returns a more coarse-grained description of the protocol used to fetch the
// response.
NET_EXPORT HttpConnectionInfoCoarse
HttpConnectionInfoToCoarse(HttpConnectionInfo info);

}  // namespace net

#endif  // NET_HTTP_HTTP_CONNECTION_INFO_H_
