// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/session_usage.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_test_base.h"
#include "net/quic/quic_socket_data_provider.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

class QuicSessionPoolProxyJobTest
    : public QuicSessionPoolTestBase,
      public ::testing::TestWithParam<quic::ParsedQuicVersion> {
 protected:
  QuicSessionPoolProxyJobTest() : QuicSessionPoolTestBase(GetParam()) {}

  test::QuicTestPacketMaker MakePacketMaker(
      const std::string& host,
      quic::Perspective perspective,
      bool client_priority_uses_incremental = false,
      bool use_priority_header = false) {
    return test::QuicTestPacketMaker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), host, perspective, client_priority_uses_incremental,
        use_priority_header);
  }

  base::HistogramTester histogram_tester;
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolProxyJobTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()));

TEST_P(QuicSessionPoolProxyJobTest, CreateProxiedQuicSession) {
  Initialize();

  GURL url("https://www.example.org/");
  GURL proxy(kProxy1Url);
  auto origin = url::SchemeHostPort(url);
  auto proxy_origin = url::SchemeHostPort(proxy);
  auto nak = NetworkAnonymizationKey();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(proxy_origin.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // QUIC proxies do not use priority header.
  client_maker_.set_use_priority_header(false);

  // Use a separate packet maker for the connection to the endpoint.
  QuicTestPacketMaker endpoint_maker =
      MakePacketMaker(kDefaultServerHostName, quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/true);

  const uint64_t stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  QuicSocketDataProvider socket_data(version_);
  socket_data.AddWrite("initial-settings", ConstructInitialSettingsPacket(1))
      .Sync();
  socket_data
      .AddWrite("connect-udp",
                ConstructConnectUdpRequestPacket(
                    2, stream_id, proxy.host(),
                    "/.well-known/masque/udp/www.example.org/443/", false))
      .Sync();
  socket_data.AddRead("server-settings", ConstructServerSettingsPacket(3));
  socket_data.AddRead("ok-response",
                      ConstructOkResponsePacket(4, stream_id, true));
  socket_data.AddWrite("ack",
                       client_maker_.Packet(3).AddAckFrame(3, 4, 3).Build());
  socket_data.AddWrite("endpoint-initial-settings",
                       ConstructClientH3DatagramPacket(
                           4, stream_id, kConnectUdpContextId,
                           endpoint_maker.MakeInitialSettingsPacket(1)));
  socket_factory_->AddSocketDataProvider(&socket_data);

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy_origin.host(), 443),
  });
  EXPECT_TRUE(proxy_chain.IsValid());

  RequestBuilder builder(this);
  builder.destination = origin;
  builder.proxy_chain = proxy_chain;
  builder.http_user_agent_settings = &http_user_agent_settings_;
  builder.url = url;

  // Note: `builder` defaults to using the parameterized `version_` member,
  // which we will assert here as a pre-condition for checking that the proxy
  // session ignores this and uses RFCv1 instead.
  ASSERT_EQ(builder.quic_version, version_);

  EXPECT_EQ(ERR_IO_PENDING, builder.CallRequest());
  ASSERT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session =
      GetActiveSession(origin, PRIVACY_MODE_DISABLED, nak, proxy_chain);
  ASSERT_TRUE(session);

  // The direct connection to the proxy has a max packet size 1350. The
  // connection to the endpoint could use up to 1350 - (packet header = 38) -
  // (quarter-stream-id = 1) - (context-id = 1), but this value is greater than
  // the default maximum of 1250. We can only observe the largest datagram that
  // could be sent to the endpoint, which would be 1250 - (packet header = 38) =
  // 1212 bytes.
  EXPECT_EQ(session->GetGuaranteedLargestMessagePayload(), 1212);

  // Check that the session through the proxy uses the version from the request.
  EXPECT_EQ(session->GetQuicVersion(), version_);

  // Check that the session to the proxy is keyed by an empty NAK and always
  // uses RFCv1.
  QuicChromiumClientSession* proxy_session =
      GetActiveSession(proxy_origin, PRIVACY_MODE_DISABLED, nak,
                       ProxyChain::ForIpProtection({}), SessionUsage::kProxy);
  ASSERT_TRUE(proxy_session);
  EXPECT_EQ(proxy_session->GetQuicVersion(), quic::ParsedQuicVersion::RFCv1());

  stream.reset();

  // Ensure the session finishes creating before proceeding.
  RunUntilIdle();

  EXPECT_TRUE(socket_data.AllDataConsumed());
  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Http3.Quic.Success", 1);
}

TEST_P(QuicSessionPoolProxyJobTest, DoubleProxiedQuicSession) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {net::features::kPartitionConnectionsByNetworkIsolationKey},
      {net::features::kPartitionProxyChains});
  Initialize();

  // Set up a connection via proxy1, to proxy2, to example.org, all using QUIC.
  GURL url("https://www.example.org/");
  GURL proxy1(kProxy1Url);
  GURL proxy2(kProxy2Url);
  auto origin = url::SchemeHostPort(url);
  auto proxy1_origin = url::SchemeHostPort(proxy1);
  auto proxy2_origin = url::SchemeHostPort(proxy2);
  auto endpoint_nak =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(url));

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(proxy1_origin.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicSocketDataProvider socket_data(version_);
  quic::QuicStreamId stream_id_0 =
      GetNthClientInitiatedBidirectionalStreamId(0);
  int to_proxy1_packet_num = 1;
  QuicTestPacketMaker to_proxy1 =
      MakePacketMaker(proxy1_origin.host(), quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/false);
  int from_proxy1_packet_num = 1;
  QuicTestPacketMaker from_proxy1 =
      MakePacketMaker(proxy1_origin.host(), quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false);
  int to_proxy2_packet_num = 1;
  QuicTestPacketMaker to_proxy2 =
      MakePacketMaker(proxy2_origin.host(), quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/false);
  int from_proxy2_packet_num = 1;
  QuicTestPacketMaker from_proxy2 =
      MakePacketMaker(proxy2_origin.host(), quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false);
  int to_endpoint_packet_num = 1;
  QuicTestPacketMaker to_endpoint =
      MakePacketMaker("www.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/true);

  // The browser sends initial settings to proxy1.
  socket_data.AddWrite(
      "proxy1 initial settings",
      to_proxy1.MakeInitialSettingsPacket(to_proxy1_packet_num++));

  // The browser sends CONNECT-UDP request to proxy1.
  socket_data
      .AddWrite("proxy1 connect-udp",
                ConstructConnectUdpRequestPacket(
                    to_proxy1, to_proxy1_packet_num++, stream_id_0,
                    proxy1_origin.host(),
                    base::StrCat({"/.well-known/masque/udp/",
                                  proxy2_origin.host(), "/443/"}),
                    false))
      .Sync();

  // Proxy1 sends initial settings.
  socket_data.AddRead(
      "proxy1 server settings",
      from_proxy1.MakeInitialSettingsPacket(from_proxy1_packet_num++));

  // Proxy1 responds to the CONNECT.
  socket_data.AddRead(
      "proxy1 ok response",
      ConstructOkResponsePacket(from_proxy1, from_proxy1_packet_num++,
                                stream_id_0, true));

  // The browser ACKs the OK response packet.
  socket_data.AddWrite(
      "proxy1 ack ok",
      ConstructAckPacket(to_proxy1, to_proxy1_packet_num++, 1, 2, 1));

  // The browser sends initial settings and a CONNECT-UDP request to proxy2 via
  // proxy1.
  socket_data.AddWrite("proxy2 settings-and-request",
                       to_proxy1.Packet(to_proxy1_packet_num++)
                           .AddMessageFrame(ConstructH3Datagram(
                               stream_id_0, kConnectUdpContextId,
                               ConstructInitialSettingsPacket(
                                   to_proxy2, to_proxy2_packet_num++)))
                           .AddMessageFrame(ConstructH3Datagram(
                               stream_id_0, kConnectUdpContextId,
                               ConstructConnectUdpRequestPacket(
                                   to_proxy2, to_proxy2_packet_num++,
                                   stream_id_0, proxy2_origin.host(),
                                   base::StrCat({"/.well-known/masque/udp/",
                                                 origin.host(), "/443/"}),
                                   false)))
                           .Build());

  // Proxy2 sends initial settings and an OK response to the CONNECT request,
  // via proxy1.
  socket_data.AddRead(
      "proxy2 server settings and ok response",
      from_proxy1.Packet(from_proxy1_packet_num++)
          .AddMessageFrame(
              ConstructH3Datagram(stream_id_0, kConnectUdpContextId,
                                  ConstructInitialSettingsPacket(
                                      from_proxy2, from_proxy2_packet_num++)))
          .AddMessageFrame(ConstructH3Datagram(
              stream_id_0, kConnectUdpContextId,
              ConstructOkResponsePacket(from_proxy2, from_proxy2_packet_num++,
                                        stream_id_0, true)))
          .Build());

  // The browser ACK's the datagram from proxy1, and acks proxy2's OK response
  // packet via proxy1.
  socket_data.AddWrite("proxy2 acks",
                       to_proxy1.Packet(to_proxy1_packet_num++)
                           .AddAckFrame(1, 3, 1)
                           .AddMessageFrame(ConstructH3Datagram(
                               stream_id_0, kConnectUdpContextId,
                               to_proxy2.Packet(to_proxy2_packet_num++)
                                   .AddAckFrame(1, 2, 1)
                                   .Build()))
                           .Build());

  // The browser sends initial settings to the endpoint, via proxy2, via proxy1.
  socket_data.AddWrite(
      "endpoint initial settings",
      to_proxy1.Packet(to_proxy1_packet_num++)
          .AddMessageFrame(ConstructH3Datagram(
              stream_id_0, kConnectUdpContextId,
              to_proxy2.Packet(to_proxy2_packet_num++)
                  .AddMessageFrame(ConstructH3Datagram(
                      stream_id_0, kConnectUdpContextId,
                      ConstructInitialSettingsPacket(to_endpoint,
                                                     to_endpoint_packet_num++)))
                  .Build()))
          .Build());

  socket_factory_->AddSocketDataProvider(&socket_data);

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy1_origin.host(), 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy2_origin.host(), 443),
  });
  EXPECT_TRUE(proxy_chain.IsValid());

  RequestBuilder builder(this);
  builder.destination = origin;
  builder.proxy_chain = proxy_chain;
  builder.http_user_agent_settings = &http_user_agent_settings_;
  builder.network_anonymization_key = endpoint_nak;
  builder.url = url;

  // Note: `builder` defaults to using the parameterized `version_` member,
  // which we will assert here as a pre-condition for checking that the proxy
  // session ignores this and uses RFCv1 instead.
  ASSERT_EQ(builder.quic_version, version_);

  EXPECT_EQ(ERR_IO_PENDING, builder.CallRequest());
  ASSERT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session = GetActiveSession(
      origin, PRIVACY_MODE_DISABLED, endpoint_nak, proxy_chain);
  ASSERT_TRUE(session);

  // The direct connection to the proxy has a max packet size 1350. The
  // connection to the endpoint could use up to 1350 - (packet header = 38) -
  // (quarter-stream-id = 1) - (context-id = 1), but this value is greater than
  // the default maximum of 1250. We can only observe the largest datagram that
  // could be sent to the endpoint, which would be 1250 - (packet header = 38) =
  // 1212 bytes.
  EXPECT_EQ(session->GetGuaranteedLargestMessagePayload(), 1212);

  // Check that the session through the proxy uses the version from the request.
  EXPECT_EQ(session->GetQuicVersion(), version_);

  // Check that the session to proxy1 uses an empty NAK (due to
  // !kPartitionProxyChains) and RFCv1.
  auto proxy_nak = NetworkAnonymizationKey();
  QuicChromiumClientSession* proxy1_session =
      GetActiveSession(proxy1_origin, PRIVACY_MODE_DISABLED, proxy_nak,
                       ProxyChain::ForIpProtection({}), SessionUsage::kProxy);
  ASSERT_TRUE(proxy1_session);
  EXPECT_EQ(proxy1_session->quic_session_key().network_anonymization_key(),
            proxy_nak);
  EXPECT_EQ(proxy1_session->GetQuicVersion(), quic::ParsedQuicVersion::RFCv1());

  // Check that the session to proxy2 uses the endpoint NAK and RFCv1.
  QuicChromiumClientSession* proxy2_session = GetActiveSession(
      proxy2_origin, PRIVACY_MODE_DISABLED, endpoint_nak,
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, proxy1_origin.host(), 443)}),
      SessionUsage::kProxy);
  ASSERT_TRUE(proxy2_session);
  EXPECT_EQ(proxy2_session->quic_session_key().network_anonymization_key(),
            endpoint_nak);
  EXPECT_EQ(proxy2_session->GetQuicVersion(), quic::ParsedQuicVersion::RFCv1());

  stream.reset();

  // Ensure the session finishes creating before proceeding.
  RunUntilIdle();

  ASSERT_TRUE(socket_data.AllDataConsumed());

  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Http3.Quic.Success", 1);
}

TEST_P(QuicSessionPoolProxyJobTest, CreateProxySessionFails) {
  Initialize();

  GURL url("https://www.example.org/");
  GURL proxy(kProxy1Url);
  auto origin = url::SchemeHostPort(url);
  auto proxy_origin = url::SchemeHostPort(proxy);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(proxy_origin.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicSocketDataProvider socket_data(version_);
  // Creation of underlying session fails immediately.
  socket_data.AddWriteError("creation-fails", ERR_SOCKET_NOT_CONNECTED).Sync();
  socket_factory_->AddSocketDataProvider(&socket_data);

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy_origin.host(), 443),
  });
  EXPECT_TRUE(proxy_chain.IsValid());

  RequestBuilder builder(this);
  builder.destination = origin;
  builder.proxy_chain = proxy_chain;
  builder.http_user_agent_settings = &http_user_agent_settings_;
  builder.url = url;
  EXPECT_EQ(ERR_IO_PENDING, builder.CallRequest());
  ASSERT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllDataConsumed());
}

TEST_P(QuicSessionPoolProxyJobTest, CreateSessionFails) {
  Initialize();

  GURL url("https://www.example.org/");
  GURL proxy(kProxy1Url);
  auto origin = url::SchemeHostPort(url);
  auto proxy_origin = url::SchemeHostPort(proxy);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(proxy_origin.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // QUIC proxies do not use priority header.
  client_maker_.set_use_priority_header(false);

  // Set up to accept socket creation, but not actually carry any packets.
  QuicSocketDataProvider socket_data(version_);
  socket_data.AddPause("nothing-happens");
  socket_factory_->AddSocketDataProvider(&socket_data);

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy_origin.host(), 443),
  });
  EXPECT_TRUE(proxy_chain.IsValid());

  RequestBuilder builder(this);
  builder.destination = origin;
  builder.proxy_chain = proxy_chain;
  builder.http_user_agent_settings = &http_user_agent_settings_;
  builder.url = url;
  EXPECT_EQ(ERR_IO_PENDING, builder.CallRequest());

  // Set up the socket, but don't even finish writing initial settings.
  RunUntilIdle();

  // Oops, the session went away. This generates an error
  // from `QuicSessionPool::CreateSessionOnProxyStream`.
  factory_->CloseAllSessions(ERR_QUIC_HANDSHAKE_FAILED,
                             quic::QuicErrorCode::QUIC_INTERNAL_ERROR);

  ASSERT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());

  // The direct connection was successful; the tunneled connection failed, but
  // that is not measured by this metric.
  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Http3.Quic.Success", 1);
  histogram_tester.ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Http3.Quic.Error", 0);
}

// If the server in a proxied session provides an SPA, the client does not
// follow it.
TEST_P(QuicSessionPoolProxyJobTest,
       ProxiedQuicSessionWithServerPreferredAddressShouldNotMigrate) {
  IPEndPoint server_preferred_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  FLAGS_quic_enable_chaos_protection = false;
  if (!quic_params_->allow_server_migration) {
    quic_params_->connection_options.push_back(quic::kSPAD);
  }
  Initialize();

  GURL url("https://www.example.org/");
  GURL proxy(kProxy1Url);
  auto origin = url::SchemeHostPort(url);
  auto proxy_origin = url::SchemeHostPort(proxy);
  auto nak = NetworkAnonymizationKey();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(proxy_origin.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set the config for the _endpoint_ to send a preferred address.
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(
      ToQuicSocketAddress(server_preferred_address));
  quic::test::QuicConfigPeer::SetPreferredAddressConnectionIdAndToken(
      &config, kNewCID, quic::QuicUtils::GenerateStatelessResetToken(kNewCID));
  crypto_client_stream_factory_.SetConfigForServerId(
      quic::QuicServerId("www.example.org", 443), config);

  // QUIC proxies do not use priority header.
  client_maker_.set_use_priority_header(false);

  // Use a separate packet maker for the connection to the endpoint.
  QuicTestPacketMaker endpoint_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
      /*client_priority_uses_incremental=*/true,
      /*use_priority_header=*/true);

  const uint64_t stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  QuicSocketDataProvider socket_data(version_);
  socket_data.AddWrite("initial-settings", ConstructInitialSettingsPacket(1))
      .Sync();
  socket_data
      .AddWrite("connect-udp",
                ConstructConnectUdpRequestPacket(
                    2, stream_id, proxy.host(),
                    "/.well-known/masque/udp/www.example.org/443/", false))
      .Sync();
  socket_data.AddRead("server-settings", ConstructServerSettingsPacket(3));
  socket_data.AddRead("ok-response",
                      ConstructOkResponsePacket(4, stream_id, true));
  socket_data.AddWrite("ack",
                       client_maker_.Packet(3).AddAckFrame(3, 4, 3).Build());
  socket_data.AddWrite("datagram",
                       ConstructClientH3DatagramPacket(
                           4, stream_id, kConnectUdpContextId,
                           endpoint_maker.MakeInitialSettingsPacket(1)));
  socket_factory_->AddSocketDataProvider(&socket_data);

  // Create socket data which should never be consumed. A packet with a
  // PathChallengeFrame written to this socket indicates that the client
  // incorrectly tried to connect directly to the server at its alternate
  // address.
  QuicSocketDataProvider socket_data_alt_addr(version_);
  socket_factory_->AddSocketDataProvider(&socket_data_alt_addr);

  auto proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                         proxy_origin.host(), 443),
  });
  EXPECT_TRUE(proxy_chain.IsValid());

  RequestBuilder builder(this);
  builder.destination = origin;
  builder.proxy_chain = proxy_chain;
  builder.http_user_agent_settings = &http_user_agent_settings_;
  builder.url = url;
  EXPECT_EQ(ERR_IO_PENDING, builder.CallRequest());
  ASSERT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session =
      GetActiveSession(origin, PRIVACY_MODE_DISABLED, nak, proxy_chain);
  ASSERT_TRUE(session);

  // Ensure the session finishes creating before proceeding.
  RunUntilIdle();

  // Double-check that no migration occurred, so the peer address is not the
  // server's preferred address.
  IPEndPoint peer_address = ToIPEndPoint(session->peer_address());
  EXPECT_NE(peer_address, server_preferred_address);

  EXPECT_TRUE(socket_data.AllDataConsumed());
}

}  // namespace net::test
