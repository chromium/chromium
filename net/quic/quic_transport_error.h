// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_TRANSPORT_ERROR_H_
#define NET_QUIC_QUIC_TRANSPORT_ERROR_H_

#include <ostream>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"

namespace net {

// TODO(crbug.com/1193409): rename this class to WebTransportError.
struct NET_EXPORT QuicTransportError {
  QuicTransportError() = default;
  QuicTransportError(int net_error,
                     quic::QuicErrorCode quic_error,
                     base::StringPiece details,
                     bool safe_to_report_details)
      : net_error(net_error),
        quic_error(quic_error),
        details(details),
        safe_to_report_details(safe_to_report_details) {}

  // |net_error| is always set to a meaningful value.
  int net_error = OK;

  // |quic_error| is set to a QUIC error, or to quic::QUIC_NO_ERROR if the error
  // originates non-QUIC parts of the stack.
  quic::QuicErrorCode quic_error = quic::QUIC_NO_ERROR;

  // Human-readable error summary.
  std::string details;

  // QuicTransport requires that the connection errors have to be
  // undistinguishable until the peer is confirmed to be a QuicTransport
  // endpoint.  See https://wicg.github.io/web-transport/#protocol-security
  bool safe_to_report_details = false;
};

NET_EXPORT
std::string QuicTransportErrorToString(const QuicTransportError& error);

NET_EXPORT
std::ostream& operator<<(std::ostream& os, const QuicTransportError& error);

}  // namespace net

#endif  // NET_QUIC_QUIC_TRANSPORT_ERROR_H_
