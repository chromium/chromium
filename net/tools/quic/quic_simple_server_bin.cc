// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicServer.  It listens forever on --port
// (default 6121) until it's killed or ctrl-cd to death.

#include <vector>

#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_command_line_flags.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_system_event_loop.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_simple_server_backend.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_toy_server.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/tools/quic/quic_simple_server_backend_factory.h"

class QuicSimpleServerFactory : public quic::QuicToyServer::ServerFactory {
  std::unique_ptr<quic::QuicSpdyServerBase> CreateServer(
      quic::QuicSimpleServerBackend* backend,
      std::unique_ptr<quic::ProofSource> proof_source,
      const quic::ParsedQuicVersionVector& supported_versions) override {
    return std::make_unique<net::QuicSimpleServer>(
        std::move(proof_source), config_,
        quic::QuicCryptoServerConfig::ConfigOptions(), supported_versions,
        backend);
  }

 private:
  quic::QuicConfig config_;
};

int main(int argc, char* argv[]) {
  quiche::QuicheSystemEventLoop event_loop("quic_server");
  const char* usage = "Usage: quic_server [options]";
  std::vector<std::string> non_option_args =
      quiche::QuicheParseCommandLineFlags(usage, argc, argv);
  if (!non_option_args.empty()) {
    quiche::QuichePrintCommandLineFlagHelp(usage);
    exit(0);
  }

  net::QuicSimpleServerBackendFactory backend_factory;
  QuicSimpleServerFactory server_factory;
  quic::QuicToyServer server(&backend_factory, &server_factory);
  return server.Start();
}
