// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_default_proof_providers.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_system_event_loop.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "url/gurl.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(uint16_t, port, 20557, "The port to listen on.");

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

  net::QuicTransportSimpleServer server(GetQuicFlag(FLAGS_port),
                                        accepted_origins,
                                        quic::CreateDefaultProofSource());
  server.set_read_error_callback(
      base::BindOnce([](int /*result*/) { exit(EXIT_FAILURE); }));
  if (server.Start() != EXIT_SUCCESS)
    return EXIT_FAILURE;

  base::RunLoop run_loop;
  run_loop.Run();
  return EXIT_SUCCESS;
}
