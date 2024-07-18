// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/quic_simple_test_server.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/port_util.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_dispatcher.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/tools/quic/quic_simple_server.h"

namespace {

const char kTestServerDomain[] = "example.com";
// This must match the certificate used (quic-chain.pem and quic-leaf-cert.key).
const char kTestServerHost[] = "test.example.com";

const char kStatusHeader[] = ":status";

const char kHelloPath[] = "/hello.txt";
const char kHelloBodyValue[] = "Hello from QUIC Server";
const char kHelloStatus[] = "200";

const char kHelloHeaderName[] = "hello_header";
const char kHelloHeaderValue[] = "hello header value";

const char kHelloTrailerName[] = "hello_trailer";
const char kHelloTrailerValue[] = "hello trailer value";

const char kSimplePath[] = "/simple.txt";
const char kSimpleBodyValue[] = "Simple Hello from QUIC Server";
const char kSimpleStatus[] = "200";

const char kSimpleHeaderName[] = "hello_header";
const char kSimpleHeaderValue[] = "hello header value";
const std::string kCombinedHelloHeaderValue = std::string("foo\0bar", 7);
const char kCombinedHeaderName[] = "combined";

base::Thread* g_quic_server_thread = nullptr;
quic::QuicMemoryCacheBackend* g_quic_cache_backend = nullptr;
net::QuicSimpleServer* g_quic_server = nullptr;
int g_quic_server_port = 0;

}  // namespace

namespace net {

std::string const QuicSimpleTestServer::GetDomain() {
  return kTestServerDomain;
}

std::string const QuicSimpleTestServer::GetHost() {
  return kTestServerHost;
}

HostPortPair const QuicSimpleTestServer::GetHostPort() {
  return HostPortPair(kTestServerHost, GetPort());
}

GURL QuicSimpleTestServer::GetFileURL(const std::string& file_path) {
  return GURL("https://test.example.com:" + base::NumberToString(GetPort()))
      .Resolve(file_path);
}

GURL QuicSimpleTestServer::GetHelloURL() {
  // Don't include |port| into Hello URL as it is mapped differently.
  return GURL("https://test.example.com").Resolve(kHelloPath);
}

std::string const QuicSimpleTestServer::GetStatusHeaderName() {
  return kStatusHeader;
}

// Hello Url returns response with HTTP/2 headers and trailers.
std::string const QuicSimpleTestServer::GetHelloPath() {
  return kHelloPath;
}

std::string const QuicSimpleTestServer::GetHelloBodyValue() {
  return kHelloBodyValue;
}
std::string const QuicSimpleTestServer::GetHelloStatus() {
  return kHelloStatus;
}

std::string const QuicSimpleTestServer::GetHelloHeaderName() {
  return kHelloHeaderName;
}

std::string const QuicSimpleTestServer::GetHelloHeaderValue() {
  return kHelloHeaderValue;
}

std::string const QuicSimpleTestServer::GetCombinedHeaderName() {
  return kCombinedHeaderName;
}

std::string const QuicSimpleTestServer::GetHelloTrailerName() {
  return kHelloTrailerName;
}

std::string const QuicSimpleTestServer::GetHelloTrailerValue() {
  return kHelloTrailerValue;
}

// Simple Url returns response without HTTP/2 trailers.
GURL QuicSimpleTestServer::GetSimpleURL() {
  // Don't include |port| into Simple URL as it is mapped differently.
  return GURL("https://test.example.com").Resolve(kSimplePath);
}

std::string const QuicSimpleTestServer::GetSimpleBodyValue() {
  return kSimpleBodyValue;
}

std::string const QuicSimpleTestServer::GetSimpleStatus() {
  return kSimpleStatus;
}

std::string const QuicSimpleTestServer::GetSimpleHeaderName() {
  return kSimpleHeaderName;
}

std::string const QuicSimpleTestServer::GetSimpleHeaderValue() {
  return kSimpleHeaderValue;
}

void SetupQuicMemoryCacheBackend() {
  quiche::HttpHeaderBlock headers;
  headers[kHelloHeaderName] = kHelloHeaderValue;
  headers[kStatusHeader] = kHelloStatus;
  headers[kCombinedHeaderName] = kCombinedHelloHeaderValue;
  quiche::HttpHeaderBlock trailers;
  trailers[kHelloTrailerName] = kHelloTrailerValue;
  g_quic_cache_backend = new quic::QuicMemoryCacheBackend();
  g_quic_cache_backend->AddResponse(base::StringPrintf("%s", kTestServerHost),
                                    kHelloPath, std::move(headers),
                                    kHelloBodyValue, std::move(trailers));
  headers[kSimpleHeaderName] = kSimpleHeaderValue;
  headers[kStatusHeader] = kSimpleStatus;
  g_quic_cache_backend->AddResponse(base::StringPrintf("%s", kTestServerHost),
                                    kSimplePath, std::move(headers),
                                    kSimpleBodyValue);
}

void StartQuicServerOnServerThread(const base::FilePath& test_files_root,
                                   base::WaitableEvent* server_started_event) {
  CHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  CHECK(!g_quic_server);

  quic::QuicConfig config;
  // Set up server certs.
  base::FilePath directory;
  directory = test_files_root;
  auto proof_source = std::make_unique<ProofSourceChromium>();
  CHECK(proof_source->Initialize(directory.AppendASCII("quic-chain.pem"),
                                 directory.AppendASCII("quic-leaf-cert.key"),
                                 base::FilePath()));
  SetupQuicMemoryCacheBackend();

  // If we happen to list on a disallowed port, connections will fail. Try in a
  // loop until we get an allowed port.
  std::unique_ptr<QuicSimpleServer> server;
  bool got_allowed_port = false;
  constexpr int kMaxTries = 100;
  int rv = 0;

  for (int tries = 0; !got_allowed_port && tries < kMaxTries; ++tries) {
    server = std::make_unique<QuicSimpleServer>(
        std::move(proof_source), config,
        quic::QuicCryptoServerConfig::ConfigOptions(),
        quic::AllSupportedVersions(), g_quic_cache_backend);

    // Start listening on an unbound port.
    rv = server->Listen(IPEndPoint(IPAddress::IPv4AllZeros(), 0));
    if (rv >= 0) {
      got_allowed_port |= IsPortAllowedForScheme(
          server->server_address().port(), url::kHttpsScheme);
    }
  }

  CHECK_GE(rv, 0) << "QuicSimpleTestServer: Listen failed";
  CHECK(got_allowed_port);
  g_quic_server_port = server->server_address().port();
  g_quic_server = server.release();
  server_started_event->Signal();
}

void ShutdownOnServerThread(base::WaitableEvent* server_stopped_event) {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  g_quic_server->Shutdown();
  delete g_quic_server;
  g_quic_server = nullptr;
  delete g_quic_cache_backend;
  g_quic_cache_backend = nullptr;
  server_stopped_event->Signal();
}

void ShutdownDispatcherOnServerThread(
    base::WaitableEvent* dispatcher_stopped_event) {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  g_quic_server->dispatcher()->Shutdown();
  dispatcher_stopped_event->Signal();
}

bool QuicSimpleTestServer::Start() {
  CHECK(!g_quic_server_thread);
  g_quic_server_thread = new base::Thread("quic server thread");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  bool started =
      g_quic_server_thread->StartWithOptions(std::move(thread_options));
  CHECK(started);
  base::FilePath test_files_root = GetTestCertsDirectory();

  base::WaitableEvent server_started_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&StartQuicServerOnServerThread, test_files_root,
                                &server_started_event));
  server_started_event.Wait();
  return true;
}

void QuicSimpleTestServer::AddResponse(const std::string& path,
                                       quiche::HttpHeaderBlock response_headers,
                                       const std::string& response_body) {
  g_quic_cache_backend->AddResponse(
      base::StringPrintf("%s:%d", kTestServerHost, GetPort()), path,
      std::move(response_headers), response_body);
}

void QuicSimpleTestServer::AddResponseWithEarlyHints(
    const std::string& path,
    const quiche::HttpHeaderBlock& response_headers,
    const std::string& response_body,
    const std::vector<quiche::HttpHeaderBlock>& early_hints) {
  g_quic_cache_backend->AddResponseWithEarlyHints(kTestServerHost, path,
                                                  response_headers.Clone(),
                                                  response_body, early_hints);
}

void QuicSimpleTestServer::SetResponseDelay(const std::string& path,
                                            base::TimeDelta delay) {
  g_quic_cache_backend->SetResponseDelay(
      base::StringPrintf("%s:%d", kTestServerHost, GetPort()), path,
      quic::QuicTime::Delta::FromMicroseconds(delay.InMicroseconds()));
}

// Shut down the server dispatcher, and the stream should error out.
void QuicSimpleTestServer::ShutdownDispatcherForTesting() {
  if (!g_quic_server)
    return;
  DCHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  base::WaitableEvent dispatcher_stopped_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ShutdownDispatcherOnServerThread,
                                &dispatcher_stopped_event));
  dispatcher_stopped_event.Wait();
}

void QuicSimpleTestServer::Shutdown() {
  if (!g_quic_server)
    return;
  DCHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  base::WaitableEvent server_stopped_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ShutdownOnServerThread, &server_stopped_event));
  server_stopped_event.Wait();
  delete g_quic_server_thread;
  g_quic_server_thread = nullptr;
}

int QuicSimpleTestServer::GetPort() {
  return g_quic_server_port;
}

}  // namespace net
