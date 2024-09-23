// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Dumps the contents of a QUIC crypto handshake message in a human readable
// format.
//
// Usage: crypto_message_printer_bin <hex of message>

#include <iostream>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_framer.h"

using quic::Perspective;
using std::cerr;
using std::cout;
using std::endl;

namespace net {

class CryptoMessagePrinter : public quic::CryptoFramerVisitorInterface {
 public:
  explicit CryptoMessagePrinter() = default;

  void OnHandshakeMessage(
      const quic::CryptoHandshakeMessage& message) override {
    cout << message.DebugString() << endl;
  }

  void OnError(quic::CryptoFramer* framer) override {
    cerr << "Error code: " << framer->error() << endl;
    cerr << "Error details: " << framer->error_detail() << endl;
  }

};

}  // namespace net

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);

  if (argc != 1) {
    cerr << "Usage: " << argv[0] << " <hex of message>\n";
    return 1;
  }

  net::CryptoMessagePrinter printer;
  quic::CryptoFramer framer;
  framer.set_visitor(&printer);
  framer.set_process_truncated_messages(true);
  std::string input;
  if (!base::HexStringToString(argv[1], &input) ||
      !framer.ProcessInput(input)) {
    return 1;
  }
  if (framer.InputBytesRemaining() != 0) {
    cerr << "Input partially consumed. " << framer.InputBytesRemaining()
         << " bytes remaining." << endl;
    return 2;
  }
  return 0;
}
