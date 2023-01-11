// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_transport_simple_test_server.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "net/quic/crypto_test_utils_chromium.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_backend.h"
#include "net/tools/quic/quic_simple_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace content {

WebTransportSimpleTestServer::WebTransportSimpleTestServer() {
  quic::QuicEnableVersion(quic::ParsedQuicVersion::RFCv1());
}

WebTransportSimpleTestServer::~WebTransportSimpleTestServer() {
  server_thread_->task_runner()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(server_)));

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_for_thread_join;
  server_thread_.reset();
}

void WebTransportSimpleTestServer::SetUpCommandLine(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  command_line->AppendSwitchASCII(
      switches::kOriginToForceQuicOn,
      base::StringPrintf("localhost:%d", server_address().port()));
  command_line->AppendSwitch(switches::kEnableQuic);
  command_line->AppendSwitchASCII(
      switches::kQuicVersion,
      quic::AlpnForVersion(quic::ParsedQuicVersion::RFCv1()));
  // The value is calculated from net/data/ssl/certificates/quic-chain.pem.
  command_line->AppendSwitchASCII(
      network::switches::kIgnoreCertificateErrorsSPKIList,
      "I+ryIVl5ksb8KijTneC3y7z1wBFn5x35O5is9g5n/KM=");
}

void WebTransportSimpleTestServer::Start() {
  CHECK(!server_thread_);

  server_thread_ = std::make_unique<base::Thread>("WebTransport server");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  CHECK(server_thread_->StartWithOptions(std::move(thread_options)));
  CHECK(server_thread_->WaitUntilThreadStarted());

  base::WaitableEvent event;
  net::IPEndPoint server_address;
  server_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        backend_ = std::make_unique<quic::test::QuicTestBackend>();
        backend_->set_enable_webtransport(true);
        server_ = std::make_unique<net::QuicSimpleServer>(
            net::test::ProofSourceForTestingChromium(), quic::QuicConfig(),
            quic::QuicCryptoServerConfig::ConfigOptions(),
            quic::AllSupportedVersions(), backend_.get());
        bool result = server_->CreateUDPSocketAndListen(quic::QuicSocketAddress(
            quic::QuicSocketAddress(quic::QuicIpAddress::Any6(), /*port=*/0)));
        CHECK(result);
        server_address = server_->server_address();
        event.Signal();
      }));
  event.Wait();
  server_address_ = server_address;
}

}  // namespace content
