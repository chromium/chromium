// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/fuzzed_host_resolver_util.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/fuzzed_datagram_client_socket.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/socket/socket_tag.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

const uint8_t kCertData[] = {
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

// Persistent factory data, statically initialized on the first time
// LLVMFuzzerTestOneInput is called.
struct FuzzerEnvironment {
  FuzzerEnvironment()
      : scheme_host_port(url::kHttpsScheme, kServerHostName, kServerPort) {
    net::SetSystemDnsResolutionTaskRunnerForTesting(  // IN-TEST
        base::SequencedTaskRunner::GetCurrentDefault());

    quic_context.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
    ssl_config_service = std::make_unique<SSLConfigServiceDefaults>();
    crypto_client_stream_factory.set_use_mock_crypter(true);
    cert_verifier = std::make_unique<MockCertVerifier>();
    verify_details.cert_verify_result.verified_cert =
        X509Certificate::CreateFromBytes(kCertData);
    CHECK(verify_details.cert_verify_result.verified_cert);
    verify_details.cert_verify_result.is_issued_by_known_root = true;
  }
  ~FuzzerEnvironment() = default;

  std::unique_ptr<SSLConfigService> ssl_config_service;
  ProofVerifyDetailsChromium verify_details;
  MockCryptoClientStreamFactory crypto_client_stream_factory;
  url::SchemeHostPort scheme_host_port;
  NetLogWithSource net_log;
  std::unique_ptr<CertVerifier> cert_verifier;
  TransportSecurityState transport_security_state;
  quic::QuicTagVector connection_options;
  quic::QuicTagVector client_connection_options;
  MockQuicContext quic_context;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  FuzzerEnvironment env;

  std::unique_ptr<ContextHostResolver> host_resolver =
      CreateFuzzedContextHostResolver(HostResolver::ManagerOptions(), nullptr,
                                      &data_provider,
                                      true /* enable_caching */);
  FuzzedSocketFactory socket_factory(&data_provider);

  // Initialize this on each loop since some options mutate this.
  HttpServerProperties http_server_properties;

  QuicParams& params = *env.quic_context.params();
  params.max_server_configs_stored_in_properties =
      data_provider.ConsumeBool() ? 1 : 0;
  params.close_sessions_on_ip_change = data_provider.ConsumeBool();
  params.allow_server_migration = data_provider.ConsumeBool();
  params.estimate_initial_rtt = data_provider.ConsumeBool();
  params.enable_socket_recv_optimization = data_provider.ConsumeBool();

  env.crypto_client_stream_factory.AddProofVerifyDetails(&env.verify_details);

  params.goaway_sessions_on_ip_change = false;
  params.migrate_sessions_early_v2 = false;
  params.migrate_sessions_on_network_change_v2 = false;
  params.retry_on_alternate_network_before_handshake = false;
  params.migrate_idle_sessions = false;

  if (!params.close_sessions_on_ip_change) {
    params.goaway_sessions_on_ip_change = data_provider.ConsumeBool();
    if (!params.goaway_sessions_on_ip_change) {
      params.migrate_sessions_on_network_change_v2 =
          data_provider.ConsumeBool();
      if (params.migrate_sessions_on_network_change_v2) {
        params.migrate_sessions_early_v2 = data_provider.ConsumeBool();
        params.retry_on_alternate_network_before_handshake =
            data_provider.ConsumeBool();
        params.migrate_idle_sessions = data_provider.ConsumeBool();
      }
    }
  }

  std::unique_ptr<QuicSessionPool> factory = std::make_unique<QuicSessionPool>(
      env.net_log.net_log(), host_resolver.get(), env.ssl_config_service.get(),
      &socket_factory, &http_server_properties, env.cert_verifier.get(),
      &env.transport_security_state, nullptr, nullptr, nullptr,
      &env.crypto_client_stream_factory, &env.quic_context);

  QuicSessionRequest request(factory.get());
  TestCompletionCallback callback;
  NetErrorDetails net_error_details;
  quic::ParsedQuicVersionVector versions = AllSupportedQuicVersions();
  quic::ParsedQuicVersion version =
      versions[data_provider.ConsumeIntegralInRange<size_t>(
          0, versions.size() - 1)];

  quic::QuicEnableVersion(version);

  request.Request(
      env.scheme_host_port, version, ProxyChain::Direct(),
      TRAFFIC_ANNOTATION_FOR_TESTS, /*http_user_agent_settings=*/nullptr,
      SessionUsage::kDestination, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false, kCertVerifyFlags, GURL(kUrl),
      env.net_log, &net_error_details,
      /*failed_on_default_network_callback=*/CompletionOnceCallback(),
      callback.callback());

  callback.WaitForResult();
  std::unique_ptr<QuicChromiumClientSession::Handle> session =
      request.ReleaseSessionHandle();
  if (!session) {
    return 0;
  }
  auto dns_aliases = session->GetDnsAliasesForSessionKey(request.session_key());
  auto stream = std::make_unique<QuicHttpStream>(std::move(session),
                                                 std::move(dns_aliases));

  HttpRequestInfo request_info;
  request_info.method = kMethod;
  request_info.url = GURL(kUrl);
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  stream->InitializeStream(true, DEFAULT_PRIORITY, env.net_log,
                           CompletionOnceCallback());

  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  if (OK !=
      stream->SendRequest(request_headers, &response, callback.callback())) {
    return 0;
  }

  // TODO(nedwilliamson): attempt connection migration here
  int rv = stream->ReadResponseHeaders(callback.callback());
  if (rv != OK && rv != ERR_IO_PENDING) {
    return 0;
  }
  callback.WaitForResult();

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  rv = stream->ReadResponseBody(buffer.get(), kBufferSize, callback.callback());
  if (rv == ERR_IO_PENDING) {
    callback.WaitForResult();
  }

  return 0;
}

}  // namespace test
}  // namespace net
