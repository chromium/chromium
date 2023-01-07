// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_TEST_PACKET_PRINTER_H_
#define NET_QUIC_QUIC_TEST_PACKET_PRINTER_H_

#include <string>

#include "net/socket/socket_test_util.h"

namespace net {

class QuicPacketPrinter : public SocketDataPrinter {
 public:
  explicit QuicPacketPrinter(quic::ParsedQuicVersion version)
      : version_(version) {}
  QuicPacketPrinter(const QuicPacketPrinter&) = delete;
  QuicPacketPrinter& operator=(const QuicPacketPrinter&) = delete;

  ~QuicPacketPrinter() = default;

  std::string PrintWrite(const std::string& data) override;

 private:
  quic::ParsedQuicVersion version_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_TEST_PACKET_PRINTER_H_
