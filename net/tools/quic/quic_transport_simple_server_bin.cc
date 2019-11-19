// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_system_event_loop.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "url/gurl.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(int, port, 20557, "The port to listen on.");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    std::string,
    mode,
    "discard",
    "The mode used by the SimpleServer.  Can be \"echo\" or \"discard\".");

DEFINE_QUIC_COMMAND_LINE_FLAG(std::string,
                              accepted_origins,
                              "",
                              "Comma-separated list of accepted origins");

int main(int argc, char** argv) {
  const char* usage = "quic_transport_simple_server";
  QuicSystemEventLoop event_loop("quic_transport_simple_server");
  std::vector<std::string> non_option_args =
      quic::QuicParseCommandLineFlags(usage, argc, argv);
  if (!non_option_args.empty()) {
    quic::QuicPrintCommandLineFlagHelp(usage);
    return 0;
  }

  std::string mode_text = GetQuicFlag(FLAGS_mode);
  quic::QuicTransportSimpleServerSession::Mode mode;
  if (mode_text == "discard") {
    mode = quic::QuicTransportSimpleServerSession::DISCARD;
  } else if (mode_text == "echo") {
    mode = quic::QuicTransportSimpleServerSession::ECHO;
  } else {
    LOG(ERROR) << "Invalid mode specified: " << mode_text;
    return 1;
  }

  std::string accepted_origins_text = GetQuicFlag(FLAGS_accepted_origins);
  std::vector<url::Origin> accepted_origins;
  for (const base::StringPiece& origin :
       base::SplitStringPiece(accepted_origins_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    GURL url{origin};
    if (!url.is_valid()) {
      LOG(ERROR) << "Failed to parse origin specified: " << origin;
      return 1;
    }
    accepted_origins.push_back(url::Origin::Create(url));
  }

  net::QuicTransportSimpleServer server(GetQuicFlag(FLAGS_port), mode,
                                        accepted_origins);
  return server.Run();
}
