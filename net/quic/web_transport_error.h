// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_WEB_TRANSPORT_ERROR_H_
#define NET_QUIC_WEB_TRANSPORT_ERROR_H_

#include <ostream>
#include <string>
#include <string_view>

#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"

namespace net {

struct NET_EXPORT WebTransportError {
  WebTransportError() = default;
  explicit WebTransportError(int net_error) : net_error(net_error) {
    DCHECK_LT(net_error, 0);
  }
  WebTransportError(int net_error,
                    quic::QuicErrorCode quic_error,
                    std::string_view details,
                    bool safe_to_report_details)
      : net_error(net_error),
        quic_error(quic_error),
        details(details),
        safe_to_report_details(safe_to_report_details) {
    DCHECK_LT(net_error, 0);
  }

  // |net_error| is always set to a meaningful value.
  int net_error = ERR_FAILED;

  // |quic_error| is set to a QUIC error, or to quic::QUIC_NO_ERROR if the error
  // originates non-QUIC parts of the stack.
  quic::QuicErrorCode quic_error = quic::QUIC_NO_ERROR;

  // Human-readable error summary.
  std::string details;

  // WebTransport requires that the connection errors have to be
  // undistinguishable until the peer is confirmed to be a WebTransport
  // endpoint.  See https://w3c.github.io/webtransport/#protocol-security
  bool safe_to_report_details = false;
};

NET_EXPORT
std::string WebTransportErrorToString(const WebTransportError& error);

NET_EXPORT
std::ostream& operator<<(std::ostream& os, const WebTransportError& error);

}  // namespace net

#endif  // NET_QUIC_WEB_TRANSPORT_ERROR_H_
