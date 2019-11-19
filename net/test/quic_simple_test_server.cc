// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/quic_simple_test_server.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quic/core/quic_dispatcher.h"
#include "net/third_party/quiche/src/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
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

base::Thread* g_quic_server_thread = nullptr;
quic::QuicMemoryCacheBackend* g_quic_cache_backend = nullptr;
net::QuicSimpleServer* g_quic_server = nullptr;
int g_quic_server_port = 0;

}  // namespace

namespace net {

const std::string QuicSimpleTestServer::GetDomain() {
  return kTestServerDomain;
}

const std::string QuicSimpleTestServer::GetHost() {
  return kTestServerHost;
}

GURL QuicSimpleTestServer::GetFileURL(const std::string& file_path) {
  return GURL("https://test.example.com:" + base::NumberToString(GetPort()))
      .Resolve(file_path);
}

GURL QuicSimpleTestServer::GetHelloURL() {
  // Don't include |port| into Hello URL as it is mapped differently.
  return GURL("https://test.example.com").Resolve(kHelloPath);
}

const std::string QuicSimpleTestServer::GetStatusHeaderName() {
  return kStatusHeader;
}

// Hello Url returns response with HTTP/2 headers and trailers.
const std::string QuicSimpleTestServer::GetHelloPath() {
  return kHelloPath;
}

const std::string QuicSimpleTestServer::GetHelloBodyValue() {
  return kHelloBodyValue;
}
const std::string QuicSimpleTestServer::GetHelloStatus() {
  return kHelloStatus;
}

const std::string QuicSimpleTestServer::GetHelloHeaderName() {
  return kHelloHeaderName;
}

const std::string QuicSimpleTestServer::GetHelloHeaderValue() {
  return kHelloHeaderValue;
}

const std::string QuicSimpleTestServer::GetHelloTrailerName() {
  return kHelloTrailerName;
}

const std::string QuicSimpleTestServer::GetHelloTrailerValue() {
  return kHelloTrailerValue;
}

// Simple Url returns response without HTTP/2 trailers.
GURL QuicSimpleTestServer::GetSimpleURL() {
  // Don't include |port| into Simple URL as it is mapped differently.
  return GURL("https://test.example.com").Resolve(kSimplePath);
}

const std::string QuicSimpleTestServer::GetSimpleBodyValue() {
  return kSimpleBodyValue;
}

const std::string QuicSimpleTestServer::GetSimpleStatus() {
  return kSimpleStatus;
}

const std::string QuicSimpleTestServer::GetSimpleHeaderName() {
  return kSimpleHeaderName;
}

const std::string QuicSimpleTestServer::GetSimpleHeaderValue() {
  return kSimpleHeaderValue;
}

void SetupQuicMemoryCacheBackend() {
  spdy::SpdyHeaderBlock headers;
  headers[kHelloHeaderName] = kHelloHeaderValue;
  headers[kStatusHeader] = kHelloStatus;
  spdy::SpdyHeaderBlock trailers;
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
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  DCHECK(!g_quic_server);

  quic::QuicConfig config;
  // Set up server certs.
  base::FilePath directory;
  directory = test_files_root;
  std::unique_ptr<ProofSourceChromium> proof_source(new ProofSourceChromium());
  CHECK(proof_source->Initialize(directory.AppendASCII("quic-chain.pem"),
                                 directory.AppendASCII("quic-leaf-cert.key"),
                                 base::FilePath()));
  SetupQuicMemoryCacheBackend();

  g_quic_server =
      new QuicSimpleServer(std::move(proof_source), config,
                           quic::QuicCryptoServerConfig::ConfigOptions(),
                           quic::AllSupportedVersions(), g_quic_cache_backend);

  // Start listening on an unbound port.
  int rv = g_quic_server->Listen(IPEndPoint(IPAddress::IPv4AllZeros(), 0));
  CHECK_GE(rv, 0) << "Quic server fails to start";
  g_quic_server_port = g_quic_server->server_address().port();
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
  DVLOG(3) << g_quic_server_thread;
  DCHECK(!g_quic_server_thread);
  g_quic_server_thread = new base::Thread("quic server thread");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  bool started = g_quic_server_thread->StartWithOptions(thread_options);
  DCHECK(started);
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
