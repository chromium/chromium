// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include "base/test/fuzzed_data_provider.h"

#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/fuzzed_host_resolver.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/fuzzed_datagram_client_socket.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/socket/socket_tag.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net {

namespace {

const char kCertData[] = {
#include "net/data/ssl/certificates/wildcard.inc"
};

}  // namespace

namespace test {

const char kServerHostName[] = "www.example.org";
const int kServerPort = 443;
const char kUrl[] = "https://www.example.org/";
// TODO(nedwilliamson): Add POST here after testing
// whether that can lead blocking while waiting for
// the callbacks.
const char kMethod[] = "GET";
const size_t kBufferSize = 4096;
const int kCertVerifyFlags = 0;

// Static initialization for persistent factory data
struct Env {
  Env() : host_port_pair(kServerHostName, kServerPort), random_generator(0) {
    clock.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
    ssl_config_service = std::make_unique<SSLConfigServiceDefaults>();
    crypto_client_stream_factory.set_use_mock_crypter(true);
    cert_verifier = std::make_unique<MockCertVerifier>();
    cert_transparency_verifier = std::make_unique<DoNothingCTVerifier>();
    verify_details.cert_verify_result.verified_cert =
        X509Certificate::CreateFromBytes(kCertData, arraysize(kCertData));
    CHECK(verify_details.cert_verify_result.verified_cert);
    verify_details.cert_verify_result.is_issued_by_known_root = true;
  }

  quic::MockClock clock;
  std::unique_ptr<SSLConfigService> ssl_config_service;
  ProofVerifyDetailsChromium verify_details;
  MockCryptoClientStreamFactory crypto_client_stream_factory;
  HostPortPair host_port_pair;
  quic::test::MockRandom random_generator;
  NetLogWithSource net_log;
  std::unique_ptr<CertVerifier> cert_verifier;
  TransportSecurityState transport_security_state;
  quic::QuicTagVector connection_options;
  quic::QuicTagVector client_connection_options;
  std::unique_ptr<CTVerifier> cert_transparency_verifier;
  DefaultCTPolicyEnforcer ct_policy_enforcer;
};

static struct Env* env = new Env();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  FuzzedHostResolver host_resolver(HostResolver::Options(), nullptr,
                                   &data_provider);
  FuzzedSocketFactory socket_factory(&data_provider);

  // Initialize this on each loop since some options mutate this.
  HttpServerPropertiesImpl http_server_properties;

  bool store_server_configs_in_properties = data_provider.ConsumeBool();
  bool close_sessions_on_ip_change = data_provider.ConsumeBool();
  bool mark_quic_broken_when_network_blackholes = data_provider.ConsumeBool();
  bool allow_server_migration = data_provider.ConsumeBool();
  bool race_cert_verification = data_provider.ConsumeBool();
  bool estimate_initial_rtt = data_provider.ConsumeBool();
  bool headers_include_h2_stream_dependency = data_provider.ConsumeBool();
  bool enable_socket_recv_optimization = data_provider.ConsumeBool();
  bool race_stale_dns_on_connection = data_provider.ConsumeBool();

  env->crypto_client_stream_factory.AddProofVerifyDetails(&env->verify_details);

  bool goaway_sessions_on_ip_change = false;
  bool migrate_sessions_early_v2 = false;
  bool migrate_sessions_on_network_change_v2 = false;
  bool retry_on_alternate_network_before_handshake = false;
  bool go_away_on_path_degrading = false;

  if (!close_sessions_on_ip_change) {
    goaway_sessions_on_ip_change = data_provider.ConsumeBool();
    if (!goaway_sessions_on_ip_change) {
      migrate_sessions_on_network_change_v2 = data_provider.ConsumeBool();
      if (migrate_sessions_on_network_change_v2) {
        migrate_sessions_early_v2 = data_provider.ConsumeBool();
        retry_on_alternate_network_before_handshake =
            data_provider.ConsumeBool();
      }
    }
  }

  if (!migrate_sessions_early_v2)
    go_away_on_path_degrading = data_provider.ConsumeBool();

  std::unique_ptr<QuicStreamFactory> factory =
      std::make_unique<QuicStreamFactory>(
          env->net_log.net_log(), &host_resolver, env->ssl_config_service.get(),
          &socket_factory, &http_server_properties, env->cert_verifier.get(),
          &env->ct_policy_enforcer, &env->transport_security_state,
          env->cert_transparency_verifier.get(), nullptr,
          &env->crypto_client_stream_factory, &env->random_generator,
          &env->clock, quic::kDefaultMaxPacketSize, std::string(),
          store_server_configs_in_properties, close_sessions_on_ip_change,
          goaway_sessions_on_ip_change,
          mark_quic_broken_when_network_blackholes,
          kIdleConnectionTimeoutSeconds, quic::kPingTimeoutSecs,
          quic::kMaxTimeForCryptoHandshakeSecs, quic::kInitialIdleTimeoutSecs,
          migrate_sessions_on_network_change_v2, migrate_sessions_early_v2,
          retry_on_alternate_network_before_handshake,
          race_stale_dns_on_connection, go_away_on_path_degrading,
          base::TimeDelta::FromSeconds(kMaxTimeOnNonDefaultNetworkSecs),
          kMaxMigrationsToNonDefaultNetworkOnWriteError,
          kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
          allow_server_migration, race_cert_verification, estimate_initial_rtt,
          headers_include_h2_stream_dependency, env->connection_options,
          env->client_connection_options, enable_socket_recv_optimization);

  QuicStreamRequest request(factory.get());
  TestCompletionCallback callback;
  NetErrorDetails net_error_details;
  request.Request(
      env->host_port_pair,
      data_provider.PickValueInArray(quic::kSupportedTransportVersions),
      PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY, SocketTag(), kCertVerifyFlags,
      GURL(kUrl), env->net_log, &net_error_details,
      /*failed_on_default_network_callback=*/CompletionOnceCallback(),
      callback.callback());

  callback.WaitForResult();
  std::unique_ptr<QuicChromiumClientSession::Handle> session =
      request.ReleaseSessionHandle();
  if (!session)
    return 0;
  std::unique_ptr<HttpStream> stream(new QuicHttpStream(std::move(session)));

  HttpRequestInfo request_info;
  request_info.method = kMethod;
  request_info.url = GURL(kUrl);
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY, env->net_log,
                           CompletionOnceCallback());

  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  if (OK !=
      stream->SendRequest(request_headers, &response, callback.callback()))
    return 0;

  // TODO(nedwilliamson): attempt connection migration here
  int rv = stream->ReadResponseHeaders(callback.callback());
  if (rv != OK && rv != ERR_IO_PENDING) {
    return 0;
  }
  callback.WaitForResult();

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);
  rv = stream->ReadResponseBody(buffer.get(), kBufferSize, callback.callback());
  if (rv == ERR_IO_PENDING)
    callback.WaitForResult();

  return 0;
}

}  // namespace test
}  // namespace net
