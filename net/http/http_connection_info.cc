// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_connection_info.h"

#include "base/notreached.h"

namespace net {

std::string_view HttpConnectionInfoToString(
    HttpConnectionInfo connection_info) {
  switch (connection_info) {
    case HttpConnectionInfo::kUNKNOWN:
      return "unknown";
    case HttpConnectionInfo::kHTTP1_1:
      return "http/1.1";
    case HttpConnectionInfo::kDEPRECATED_SPDY2:
      NOTREACHED_IN_MIGRATION();
      return "";
    case HttpConnectionInfo::kDEPRECATED_SPDY3:
      return "spdy/3";
    // Since ConnectionInfo is persisted to disk, deprecated values have to be
    // handled. Note that h2-14 and h2-15 are essentially wire compatible with
    // h2.
    // Intentional fallthrough.
    case HttpConnectionInfo::kDEPRECATED_HTTP2_14:
    case HttpConnectionInfo::kDEPRECATED_HTTP2_15:
    case HttpConnectionInfo::kHTTP2:
      return "h2";
    case HttpConnectionInfo::kQUIC_UNKNOWN_VERSION:
      return "http/2+quic";
    case HttpConnectionInfo::kQUIC_32:
      return "http/2+quic/32";
    case HttpConnectionInfo::kQUIC_33:
      return "http/2+quic/33";
    case HttpConnectionInfo::kQUIC_34:
      return "http/2+quic/34";
    case HttpConnectionInfo::kQUIC_35:
      return "http/2+quic/35";
    case HttpConnectionInfo::kQUIC_36:
      return "http/2+quic/36";
    case HttpConnectionInfo::kQUIC_37:
      return "http/2+quic/37";
    case HttpConnectionInfo::kQUIC_38:
      return "http/2+quic/38";
    case HttpConnectionInfo::kQUIC_39:
      return "http/2+quic/39";
    case HttpConnectionInfo::kQUIC_40:
      return "http/2+quic/40";
    case HttpConnectionInfo::kQUIC_41:
      return "http/2+quic/41";
    case HttpConnectionInfo::kQUIC_42:
      return "http/2+quic/42";
    case HttpConnectionInfo::kQUIC_43:
      return "http/2+quic/43";
    case HttpConnectionInfo::kQUIC_44:
      return "http/2+quic/44";
    case HttpConnectionInfo::kQUIC_45:
      return "http/2+quic/45";
    case HttpConnectionInfo::kQUIC_46:
      return "http/2+quic/46";
    case HttpConnectionInfo::kQUIC_47:
      return "http/2+quic/47";
    case HttpConnectionInfo::kQUIC_Q048:
      return "h3-Q048";
    case HttpConnectionInfo::kQUIC_T048:
      return "h3-T048";
    case HttpConnectionInfo::kQUIC_Q049:
      return "h3-Q049";
    case HttpConnectionInfo::kQUIC_T049:
      return "h3-T049";
    case HttpConnectionInfo::kQUIC_Q050:
      return "h3-Q050";
    case HttpConnectionInfo::kQUIC_T050:
      return "h3-T050";
    case HttpConnectionInfo::kQUIC_Q099:
      return "h3-Q099";
    case HttpConnectionInfo::kQUIC_DRAFT_25:
      return "h3-25";
    case HttpConnectionInfo::kQUIC_DRAFT_27:
      return "h3-27";
    case HttpConnectionInfo::kQUIC_DRAFT_28:
      return "h3-28";
    case HttpConnectionInfo::kQUIC_DRAFT_29:
      return "h3-29";
    case HttpConnectionInfo::kQUIC_T099:
      return "h3-T099";
    case HttpConnectionInfo::kHTTP0_9:
      return "http/0.9";
    case HttpConnectionInfo::kHTTP1_0:
      return "http/1.0";
    case HttpConnectionInfo::kQUIC_999:
      return "http2+quic/999";
    case HttpConnectionInfo::kQUIC_T051:
      return "h3-T051";
    case HttpConnectionInfo::kQUIC_RFC_V1:
      return "h3";
    case HttpConnectionInfo::kDEPRECATED_QUIC_2_DRAFT_1:
      return "h3/quic2draft01";
    case HttpConnectionInfo::kQUIC_2_DRAFT_8:
      return "h3/quic2draft08";
  }
}

std::string_view HttpConnectionInfoCoarseToString(
    HttpConnectionInfoCoarse connection_info_coarse) {
  switch (connection_info_coarse) {
    case HttpConnectionInfoCoarse::kHTTP1:
      return "Http1";
    case HttpConnectionInfoCoarse::kHTTP2:
      return "Http2";
    case HttpConnectionInfoCoarse::kQUIC:
      return "Http3";
    case HttpConnectionInfoCoarse::kOTHER:
      return "Other";
  }
}

// Returns a more coarse-grained description of the protocol used to fetch the
// response.
HttpConnectionInfoCoarse HttpConnectionInfoToCoarse(HttpConnectionInfo info) {
  switch (info) {
    case HttpConnectionInfo::kHTTP0_9:
    case HttpConnectionInfo::kHTTP1_0:
    case HttpConnectionInfo::kHTTP1_1:
      return HttpConnectionInfoCoarse::kHTTP1;

    case HttpConnectionInfo::kHTTP2:
    case HttpConnectionInfo::kDEPRECATED_SPDY2:
    case HttpConnectionInfo::kDEPRECATED_SPDY3:
    case HttpConnectionInfo::kDEPRECATED_HTTP2_14:
    case HttpConnectionInfo::kDEPRECATED_HTTP2_15:
      return HttpConnectionInfoCoarse::kHTTP2;

    case HttpConnectionInfo::kQUIC_UNKNOWN_VERSION:
    case HttpConnectionInfo::kQUIC_32:
    case HttpConnectionInfo::kQUIC_33:
    case HttpConnectionInfo::kQUIC_34:
    case HttpConnectionInfo::kQUIC_35:
    case HttpConnectionInfo::kQUIC_36:
    case HttpConnectionInfo::kQUIC_37:
    case HttpConnectionInfo::kQUIC_38:
    case HttpConnectionInfo::kQUIC_39:
    case HttpConnectionInfo::kQUIC_40:
    case HttpConnectionInfo::kQUIC_41:
    case HttpConnectionInfo::kQUIC_42:
    case HttpConnectionInfo::kQUIC_43:
    case HttpConnectionInfo::kQUIC_44:
    case HttpConnectionInfo::kQUIC_45:
    case HttpConnectionInfo::kQUIC_46:
    case HttpConnectionInfo::kQUIC_47:
    case HttpConnectionInfo::kQUIC_Q048:
    case HttpConnectionInfo::kQUIC_T048:
    case HttpConnectionInfo::kQUIC_Q049:
    case HttpConnectionInfo::kQUIC_T049:
    case HttpConnectionInfo::kQUIC_Q050:
    case HttpConnectionInfo::kQUIC_T050:
    case HttpConnectionInfo::kQUIC_Q099:
    case HttpConnectionInfo::kQUIC_T099:
    case HttpConnectionInfo::kQUIC_999:
    case HttpConnectionInfo::kQUIC_DRAFT_25:
    case HttpConnectionInfo::kQUIC_DRAFT_27:
    case HttpConnectionInfo::kQUIC_DRAFT_28:
    case HttpConnectionInfo::kQUIC_DRAFT_29:
    case HttpConnectionInfo::kQUIC_T051:
    case HttpConnectionInfo::kQUIC_RFC_V1:
    case HttpConnectionInfo::kDEPRECATED_QUIC_2_DRAFT_1:
    case HttpConnectionInfo::kQUIC_2_DRAFT_8:
      return HttpConnectionInfoCoarse::kQUIC;

    case HttpConnectionInfo::kUNKNOWN:
      return HttpConnectionInfoCoarse::kOTHER;
  }
}

}  // namespace net
