// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/crypto_test_utils.h"

#include <memory>
#include <string>

#include "net/third_party/quic/core/crypto/channel_id.h"
#include "net/third_party/quic/core/crypto/common_cert_set.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/crypto/crypto_server_config_protobuf.h"
#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_crypto_stream.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_framer_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simple_quic_framer.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/sha.h"


namespace quic {
namespace test {

TestChannelIDKey::TestChannelIDKey(EVP_PKEY* ecdsa_key)
    : ecdsa_key_(ecdsa_key) {}
TestChannelIDKey::~TestChannelIDKey() {}

bool TestChannelIDKey::Sign(QuicStringPiece signed_data,
                            QuicString* out_signature) const {
  bssl::ScopedEVP_MD_CTX md_ctx;
  if (EVP_DigestSignInit(md_ctx.get(), nullptr, EVP_sha256(), nullptr,
                         ecdsa_key_.get()) != 1) {
    return false;
  }

  EVP_DigestUpdate(md_ctx.get(), ChannelIDVerifier::kContextStr,
                   strlen(ChannelIDVerifier::kContextStr) + 1);
  EVP_DigestUpdate(md_ctx.get(), ChannelIDVerifier::kClientToServerStr,
                   strlen(ChannelIDVerifier::kClientToServerStr) + 1);
  EVP_DigestUpdate(md_ctx.get(), signed_data.data(), signed_data.size());

  size_t sig_len;
  if (!EVP_DigestSignFinal(md_ctx.get(), nullptr, &sig_len)) {
    return false;
  }

  std::unique_ptr<uint8_t[]> der_sig(new uint8_t[sig_len]);
  if (!EVP_DigestSignFinal(md_ctx.get(), der_sig.get(), &sig_len)) {
    return false;
  }

  uint8_t* derp = der_sig.get();
  bssl::UniquePtr<ECDSA_SIG> sig(
      d2i_ECDSA_SIG(nullptr, const_cast<const uint8_t**>(&derp), sig_len));
  if (sig.get() == nullptr) {
    return false;
  }

  // The signature consists of a pair of 32-byte numbers.
  static const size_t kSignatureLength = 32 * 2;
  std::unique_ptr<uint8_t[]> signature(new uint8_t[kSignatureLength]);
  if (!BN_bn2bin_padded(&signature[0], 32, sig->r) ||
      !BN_bn2bin_padded(&signature[32], 32, sig->s)) {
    return false;
  }

  *out_signature =
      QuicString(reinterpret_cast<char*>(signature.get()), kSignatureLength);

  return true;
}

QuicString TestChannelIDKey::SerializeKey() const {
  // i2d_PublicKey will produce an ANSI X9.62 public key which, for a P-256
  // key, is 0x04 (meaning uncompressed) followed by the x and y field
  // elements as 32-byte, big-endian numbers.
  static const int kExpectedKeyLength = 65;

  int len = i2d_PublicKey(ecdsa_key_.get(), nullptr);
  if (len != kExpectedKeyLength) {
    return "";
  }

  uint8_t buf[kExpectedKeyLength];
  uint8_t* derp = buf;
  i2d_PublicKey(ecdsa_key_.get(), &derp);

  return QuicString(reinterpret_cast<char*>(buf + 1), kExpectedKeyLength - 1);
}

TestChannelIDSource::~TestChannelIDSource() {}

QuicAsyncStatus TestChannelIDSource::GetChannelIDKey(
    const QuicString& hostname,
    std::unique_ptr<ChannelIDKey>* channel_id_key,
    ChannelIDSourceCallback* /*callback*/) {
  *channel_id_key = QuicMakeUnique<TestChannelIDKey>(HostnameToKey(hostname));
  return QUIC_SUCCESS;
}

// static
EVP_PKEY* TestChannelIDSource::HostnameToKey(const QuicString& hostname) {
  // In order to generate a deterministic key for a given hostname the
  // hostname is hashed with SHA-256 and the resulting digest is treated as a
  // big-endian number. The most-significant bit is cleared to ensure that
  // the resulting value is less than the order of the group and then it's
  // taken as a private key. Given the private key, the public key is
  // calculated with a group multiplication.
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, hostname.data(), hostname.size());

  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &sha256);

  // Ensure that the digest is less than the order of the P-256 group by
  // clearing the most-significant bit.
  digest[0] &= 0x7f;

  bssl::UniquePtr<BIGNUM> k(BN_new());
  CHECK(BN_bin2bn(digest, sizeof(digest), k.get()) != nullptr);

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(p256);

  bssl::UniquePtr<EC_KEY> ecdsa_key(EC_KEY_new());
  CHECK(ecdsa_key && EC_KEY_set_group(ecdsa_key.get(), p256.get()));

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  CHECK(EC_POINT_mul(p256.get(), point.get(), k.get(), nullptr, nullptr,
                     nullptr));

  EC_KEY_set_private_key(ecdsa_key.get(), k.get());
  EC_KEY_set_public_key(ecdsa_key.get(), point.get());

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  // EVP_PKEY_set1_EC_KEY takes a reference so no |release| here.
  EVP_PKEY_set1_EC_KEY(pkey.get(), ecdsa_key.get());

  return pkey.release();
}

namespace crypto_test_utils {

namespace {

// CryptoFramerVisitor is a framer visitor that records handshake messages.
class CryptoFramerVisitor : public CryptoFramerVisitorInterface {
 public:
  CryptoFramerVisitor() : error_(false) {}

  void OnError(CryptoFramer* framer) override { error_ = true; }

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    messages_.push_back(message);
  }

  bool error() const { return error_; }

  const std::vector<CryptoHandshakeMessage>& messages() const {
    return messages_;
  }

 private:
  bool error_;
  std::vector<CryptoHandshakeMessage> messages_;
};

// HexChar parses |c| as a hex character. If valid, it sets |*value| to the
// value of the hex character and returns true. Otherwise it returns false.
bool HexChar(char c, uint8_t* value) {
  if (c >= '0' && c <= '9') {
    *value = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *value = c - 'a' + 10;
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *value = c - 'A' + 10;
    return true;
  }
  return false;
}

// A ChannelIDSource that works in asynchronous mode unless the |callback|
// argument to GetChannelIDKey is nullptr.
class AsyncTestChannelIDSource : public ChannelIDSource, public CallbackSource {
 public:
  // Takes ownership of |sync_source|, a synchronous ChannelIDSource.
  explicit AsyncTestChannelIDSource(ChannelIDSource* sync_source)
      : sync_source_(sync_source) {}
  ~AsyncTestChannelIDSource() override {}

  // ChannelIDSource implementation.
  QuicAsyncStatus GetChannelIDKey(const QuicString& hostname,
                                  std::unique_ptr<ChannelIDKey>* channel_id_key,
                                  ChannelIDSourceCallback* callback) override {
    // Synchronous mode.
    if (!callback) {
      return sync_source_->GetChannelIDKey(hostname, channel_id_key, nullptr);
    }

    // Asynchronous mode.
    QuicAsyncStatus status =
        sync_source_->GetChannelIDKey(hostname, &channel_id_key_, nullptr);
    if (status != QUIC_SUCCESS) {
      return QUIC_FAILURE;
    }
    callback_.reset(callback);
    return QUIC_PENDING;
  }

  // CallbackSource implementation.
  void RunPendingCallbacks() override {
    if (callback_) {
      callback_->Run(&channel_id_key_);
      callback_.reset();
    }
  }

 private:
  std::unique_ptr<ChannelIDSource> sync_source_;
  std::unique_ptr<ChannelIDSourceCallback> callback_;
  std::unique_ptr<ChannelIDKey> channel_id_key_;
};

}  // anonymous namespace

FakeServerOptions::FakeServerOptions() {}

FakeServerOptions::~FakeServerOptions() {}

FakeClientOptions::FakeClientOptions()
    : channel_id_enabled(false), channel_id_source_async(false) {}

FakeClientOptions::~FakeClientOptions() {}

namespace {
// This class is used by GenerateFullCHLO() to extract SCID and STK from
// REJ/SREJ and to construct a full CHLO with these fields and given inchoate
// CHLO.
class FullChloGenerator {
 public:
  FullChloGenerator(
      QuicCryptoServerConfig* crypto_config,
      QuicSocketAddress server_addr,
      QuicSocketAddress client_addr,
      const QuicClock* clock,
      QuicReferenceCountedPointer<QuicSignedServerConfig> signed_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      CryptoHandshakeMessage* out)
      : crypto_config_(crypto_config),
        server_addr_(server_addr),
        client_addr_(client_addr),
        clock_(clock),
        signed_config_(signed_config),
        compressed_certs_cache_(compressed_certs_cache),
        out_(out),
        params_(new QuicCryptoNegotiatedParameters) {}

  class ValidateClientHelloCallback : public ValidateClientHelloResultCallback {
   public:
    explicit ValidateClientHelloCallback(FullChloGenerator* generator)
        : generator_(generator) {}
    void Run(QuicReferenceCountedPointer<
                 ValidateClientHelloResultCallback::Result> result,
             std::unique_ptr<ProofSource::Details> /* details */) override {
      generator_->ValidateClientHelloDone(std::move(result));
    }

   private:
    FullChloGenerator* generator_;
  };

  std::unique_ptr<ValidateClientHelloCallback>
  GetValidateClientHelloCallback() {
    return QuicMakeUnique<ValidateClientHelloCallback>(this);
  }

 private:
  void ValidateClientHelloDone(
      QuicReferenceCountedPointer<ValidateClientHelloResultCallback::Result>
          result) {
    result_ = result;
    crypto_config_->ProcessClientHello(
        result_, /*reject_only=*/false, /*connection_id=*/1, server_addr_,
        client_addr_, AllSupportedVersions().front(), AllSupportedVersions(),
        /*use_stateless_rejects=*/true, /*server_designated_connection_id=*/0,
        clock_, QuicRandom::GetInstance(), compressed_certs_cache_, params_,
        signed_config_, /*total_framing_overhead=*/50, kDefaultMaxPacketSize,
        GetProcessClientHelloCallback());
  }

  class ProcessClientHelloCallback : public ProcessClientHelloResultCallback {
   public:
    explicit ProcessClientHelloCallback(FullChloGenerator* generator)
        : generator_(generator) {}
    void Run(
        QuicErrorCode error,
        const QuicString& error_details,
        std::unique_ptr<CryptoHandshakeMessage> message,
        std::unique_ptr<DiversificationNonce> diversification_nonce,
        std::unique_ptr<ProofSource::Details> proof_source_details) override {
      generator_->ProcessClientHelloDone(std::move(message));
    }

   private:
    FullChloGenerator* generator_;
  };

  std::unique_ptr<ProcessClientHelloCallback> GetProcessClientHelloCallback() {
    return QuicMakeUnique<ProcessClientHelloCallback>(this);
  }

  void ProcessClientHelloDone(std::unique_ptr<CryptoHandshakeMessage> rej) {
    // Verify output is a REJ or SREJ.
    EXPECT_THAT(rej->tag(),
                testing::AnyOf(testing::Eq(kSREJ), testing::Eq(kREJ)));

    VLOG(1) << "Extract valid STK and SCID from\n" << rej->DebugString();
    QuicStringPiece srct;
    ASSERT_TRUE(rej->GetStringPiece(kSourceAddressTokenTag, &srct));

    QuicStringPiece scfg;
    ASSERT_TRUE(rej->GetStringPiece(kSCFG, &scfg));
    std::unique_ptr<CryptoHandshakeMessage> server_config(
        CryptoFramer::ParseMessage(scfg));

    QuicStringPiece scid;
    ASSERT_TRUE(server_config->GetStringPiece(kSCID, &scid));

    *out_ = result_->client_hello;
    out_->SetStringPiece(kSCID, scid);
    out_->SetStringPiece(kSourceAddressTokenTag, srct);
    uint64_t xlct = LeafCertHashForTesting();
    out_->SetValue(kXLCT, xlct);
  }

 protected:
  QuicCryptoServerConfig* crypto_config_;
  QuicSocketAddress server_addr_;
  QuicSocketAddress client_addr_;
  const QuicClock* clock_;
  QuicReferenceCountedPointer<QuicSignedServerConfig> signed_config_;
  QuicCompressedCertsCache* compressed_certs_cache_;
  CryptoHandshakeMessage* out_;

  QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
  QuicReferenceCountedPointer<ValidateClientHelloResultCallback::Result>
      result_;
};

}  // namespace

int HandshakeWithFakeServer(QuicConfig* server_quic_config,
                            MockQuicConnectionHelper* helper,
                            MockAlarmFactory* alarm_factory,
                            PacketSavingConnection* client_conn,
                            QuicCryptoClientStream* client,
                            const FakeServerOptions& options) {
  PacketSavingConnection* server_conn = new PacketSavingConnection(
      helper, alarm_factory, Perspective::IS_SERVER,
      ParsedVersionOfIndex(client_conn->supported_versions(), 0));

  QuicCryptoServerConfig crypto_config(
      QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
      ProofSourceForTesting(), KeyExchangeSource::Default(),
      TlsServerHandshaker::CreateSslCtx());
  QuicCompressedCertsCache compressed_certs_cache(
      QuicCompressedCertsCache::kQuicCompressedCertsCacheSize);
  SetupCryptoServerConfigForTest(server_conn->clock(),
                                 server_conn->random_generator(),
                                 &crypto_config, options);

  TestQuicSpdyServerSession server_session(
      server_conn, *server_quic_config, client_conn->supported_versions(),
      &crypto_config, &compressed_certs_cache);
  server_session.OnSuccessfulVersionNegotiation(
      client_conn->supported_versions().front());
  EXPECT_CALL(*server_session.helper(),
              CanAcceptClientHello(testing::_, testing::_, testing::_,
                                   testing::_, testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*server_session.helper(),
              GenerateConnectionIdForReject(testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*server_conn, OnCanWrite()).Times(testing::AnyNumber());
  EXPECT_CALL(*client_conn, OnCanWrite()).Times(testing::AnyNumber());

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->encrypted_packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn,
                               server_session.GetMutableCryptoStream());
  CompareClientAndServerKeys(client, server_session.GetMutableCryptoStream());

  return client->num_sent_client_hellos();
}

int HandshakeWithFakeClient(MockQuicConnectionHelper* helper,
                            MockAlarmFactory* alarm_factory,
                            PacketSavingConnection* server_conn,
                            QuicCryptoServerStream* server,
                            const QuicServerId& server_id,
                            const FakeClientOptions& options) {
  ParsedQuicVersionVector supported_versions = AllSupportedVersions();
  if (options.only_tls_versions) {
    supported_versions.clear();
    for (QuicTransportVersion transport_version :
         AllSupportedTransportVersions()) {
      supported_versions.push_back(
          ParsedQuicVersion(PROTOCOL_TLS1_3, transport_version));
    }
  }
  PacketSavingConnection* client_conn = new PacketSavingConnection(
      helper, alarm_factory, Perspective::IS_CLIENT, supported_versions);
  // Advance the time, because timers do not like uninitialized times.
  client_conn->AdvanceTime(QuicTime::Delta::FromSeconds(1));

  QuicCryptoClientConfig crypto_config(ProofVerifierForTesting(),
                                       TlsClientHandshaker::CreateSslCtx());
  AsyncTestChannelIDSource* async_channel_id_source = nullptr;
  if (options.channel_id_enabled) {
    ChannelIDSource* source = ChannelIDSourceForTesting();
    if (options.channel_id_source_async) {
      async_channel_id_source = new AsyncTestChannelIDSource(source);
      source = async_channel_id_source;
    }
    crypto_config.SetChannelIDSource(source);
  }
  if (!options.token_binding_params.empty()) {
    crypto_config.tb_key_params = options.token_binding_params;
  }
  TestQuicSpdyClientSession client_session(client_conn, DefaultQuicConfig(),
                                           server_id, &crypto_config);

  EXPECT_CALL(client_session, OnProofValid(testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(client_session, OnProofVerifyDetailsAvailable(testing::_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*client_conn, OnCanWrite()).Times(testing::AnyNumber());
  client_session.GetMutableCryptoStream()->CryptoConnect();
  CHECK_EQ(1u, client_conn->encrypted_packets_.size());

  CommunicateHandshakeMessagesAndRunCallbacks(
      client_conn, client_session.GetMutableCryptoStream(), server_conn, server,
      async_channel_id_source);

  if (server->handshake_confirmed() && server->encryption_established()) {
    CompareClientAndServerKeys(client_session.GetMutableCryptoStream(), server);

    if (options.channel_id_enabled) {
      std::unique_ptr<ChannelIDKey> channel_id_key;
      QuicAsyncStatus status =
          crypto_config.channel_id_source()->GetChannelIDKey(
              server_id.host(), &channel_id_key, nullptr);
      EXPECT_EQ(QUIC_SUCCESS, status);
      EXPECT_EQ(channel_id_key->SerializeKey(),
                server->crypto_negotiated_params().channel_id);
      EXPECT_EQ(
          options.channel_id_source_async,
          client_session.GetCryptoStream()->WasChannelIDSourceCallbackRun());
    }
  }

  return client_session.GetCryptoStream()->num_sent_client_hellos();
}

void SetupCryptoServerConfigForTest(const QuicClock* clock,
                                    QuicRandom* rand,
                                    QuicCryptoServerConfig* crypto_config,
                                    const FakeServerOptions& fake_options) {
  QuicCryptoServerConfig::ConfigOptions options;
  options.channel_id_enabled = true;
  options.token_binding_params = fake_options.token_binding_params;
  std::unique_ptr<CryptoHandshakeMessage> scfg(
      crypto_config->AddDefaultConfig(rand, clock, options));
}

void SendHandshakeMessageToStream(QuicCryptoStream* stream,
                                  const CryptoHandshakeMessage& message,
                                  Perspective perspective) {
  const QuicData& data = message.GetSerialized();
  QuicStreamFrame frame(
      QuicUtils::GetCryptoStreamId(
          QuicStreamPeer::session(stream)->connection()->transport_version()),
      false, stream->stream_bytes_read(), data.AsStringPiece());
  stream->OnStreamFrame(frame);
}

void CommunicateHandshakeMessages(PacketSavingConnection* client_conn,
                                  QuicCryptoStream* client,
                                  PacketSavingConnection* server_conn,
                                  QuicCryptoStream* server) {
  CommunicateHandshakeMessagesAndRunCallbacks(client_conn, client, server_conn,
                                              server, nullptr);
}

void CommunicateHandshakeMessagesAndRunCallbacks(
    PacketSavingConnection* client_conn,
    QuicCryptoStream* client,
    PacketSavingConnection* server_conn,
    QuicCryptoStream* server,
    CallbackSource* callback_source) {
  size_t client_i = 0, server_i = 0;
  while (!client->handshake_confirmed() || !server->handshake_confirmed()) {
    ASSERT_GT(client_conn->encrypted_packets_.size(), client_i);
    QUIC_LOG(INFO) << "Processing "
                   << client_conn->encrypted_packets_.size() - client_i
                   << " packets client->server";
    MovePackets(client_conn, &client_i, server, server_conn,
                Perspective::IS_SERVER);
    if (callback_source) {
      callback_source->RunPendingCallbacks();
    }

    if (client->handshake_confirmed() && server->handshake_confirmed()) {
      break;
    }
    ASSERT_GT(server_conn->encrypted_packets_.size(), server_i);
    QUIC_LOG(INFO) << "Processing "
                   << server_conn->encrypted_packets_.size() - server_i
                   << " packets server->client";
    MovePackets(server_conn, &server_i, client, client_conn,
                Perspective::IS_CLIENT);
    if (callback_source) {
      callback_source->RunPendingCallbacks();
    }
  }
}

std::pair<size_t, size_t> AdvanceHandshake(PacketSavingConnection* client_conn,
                                           QuicCryptoStream* client,
                                           size_t client_i,
                                           PacketSavingConnection* server_conn,
                                           QuicCryptoStream* server,
                                           size_t server_i) {
  QUIC_LOG(INFO) << "Processing "
                 << client_conn->encrypted_packets_.size() - client_i
                 << " packets client->server";
  MovePackets(client_conn, &client_i, server, server_conn,
              Perspective::IS_SERVER);

  QUIC_LOG(INFO) << "Processing "
                 << server_conn->encrypted_packets_.size() - server_i
                 << " packets server->client";
  if (server_conn->encrypted_packets_.size() - server_i == 2) {
    QUIC_LOG(INFO) << "here";
  }
  MovePackets(server_conn, &server_i, client, client_conn,
              Perspective::IS_CLIENT);

  return std::make_pair(client_i, server_i);
}

QuicString GetValueForTag(const CryptoHandshakeMessage& message, QuicTag tag) {
  auto it = message.tag_value_map().find(tag);
  if (it == message.tag_value_map().end()) {
    return QuicString();
  }
  return it->second;
}

uint64_t LeafCertHashForTesting() {
  QuicReferenceCountedPointer<ProofSource::Chain> chain;
  QuicSocketAddress server_address;
  QuicCryptoProof proof;
  std::unique_ptr<ProofSource> proof_source(ProofSourceForTesting());

  class Callback : public ProofSource::Callback {
   public:
    Callback(bool* ok, QuicReferenceCountedPointer<ProofSource::Chain>* chain)
        : ok_(ok), chain_(chain) {}

    void Run(bool ok,
             const QuicReferenceCountedPointer<ProofSource::Chain>& chain,
             const QuicCryptoProof& /* proof */,
             std::unique_ptr<ProofSource::Details> /* details */) override {
      *ok_ = ok;
      *chain_ = chain;
    }

   private:
    bool* ok_;
    QuicReferenceCountedPointer<ProofSource::Chain>* chain_;
  };

  // Note: relies on the callback being invoked synchronously
  bool ok = false;
  proof_source->GetProof(
      server_address, "", "", AllSupportedTransportVersions().front(), "",
      std::unique_ptr<ProofSource::Callback>(new Callback(&ok, &chain)));
  if (!ok || chain->certs.empty()) {
    DCHECK(false) << "Proof generation failed";
    return 0;
  }

  return QuicUtils::FNV1a_64_Hash(chain->certs.at(0));
}

class MockCommonCertSets : public CommonCertSets {
 public:
  MockCommonCertSets(QuicStringPiece cert, uint64_t hash, uint32_t index)
      : cert_(cert), hash_(hash), index_(index) {}

  QuicStringPiece GetCommonHashes() const override {
    QUIC_BUG << "not implemented";
    return QuicStringPiece();
  }

  QuicStringPiece GetCert(uint64_t hash, uint32_t index) const override {
    if (hash == hash_ && index == index_) {
      return cert_;
    }
    return QuicStringPiece();
  }

  bool MatchCert(QuicStringPiece cert,
                 QuicStringPiece common_set_hashes,
                 uint64_t* out_hash,
                 uint32_t* out_index) const override {
    if (cert != cert_) {
      return false;
    }

    if (common_set_hashes.size() % sizeof(uint64_t) != 0) {
      return false;
    }
    bool client_has_set = false;
    for (size_t i = 0; i < common_set_hashes.size(); i += sizeof(uint64_t)) {
      uint64_t hash;
      memcpy(&hash, common_set_hashes.data() + i, sizeof(hash));
      if (hash == hash_) {
        client_has_set = true;
        break;
      }
    }

    if (!client_has_set) {
      return false;
    }

    *out_hash = hash_;
    *out_index = index_;
    return true;
  }

 private:
  const QuicString cert_;
  const uint64_t hash_;
  const uint32_t index_;
};

CommonCertSets* MockCommonCertSets(QuicStringPiece cert,
                                   uint64_t hash,
                                   uint32_t index) {
  return new class MockCommonCertSets(cert, hash, index);
}

void FillInDummyReject(CryptoHandshakeMessage* rej, bool reject_is_stateless) {
  if (reject_is_stateless) {
    rej->set_tag(kSREJ);
  } else {
    rej->set_tag(kREJ);
  }

  // Minimum SCFG that passes config validation checks.
  // clang-format off
  unsigned char scfg[] = {
    // SCFG
    0x53, 0x43, 0x46, 0x47,
    // num entries
    0x01, 0x00,
    // padding
    0x00, 0x00,
    // EXPY
    0x45, 0x58, 0x50, 0x59,
    // EXPY end offset
    0x08, 0x00, 0x00, 0x00,
    // Value
    '1',  '2',  '3',  '4',
    '5',  '6',  '7',  '8'
  };
  // clang-format on
  rej->SetValue(kSCFG, scfg);
  rej->SetStringPiece(kServerNonceTag, "SERVER_NONCE");
  int64_t ttl = 2 * 24 * 60 * 60;
  rej->SetValue(kSTTL, ttl);
  std::vector<QuicTag> reject_reasons;
  reject_reasons.push_back(CLIENT_NONCE_INVALID_FAILURE);
  rej->SetVector(kRREJ, reject_reasons);
}

void CompareClientAndServerKeys(QuicCryptoClientStream* client,
                                QuicCryptoServerStream* server) {
  QuicFramer* client_framer = QuicConnectionPeer::GetFramer(
      QuicStreamPeer::session(client)->connection());
  QuicFramer* server_framer = QuicConnectionPeer::GetFramer(
      QuicStreamPeer::session(server)->connection());
  const QuicEncrypter* client_encrypter(
      QuicFramerPeer::GetEncrypter(client_framer, ENCRYPTION_INITIAL));
  const QuicDecrypter* client_decrypter(
      QuicStreamPeer::session(client)->connection()->decrypter());
  const QuicEncrypter* client_forward_secure_encrypter(
      QuicFramerPeer::GetEncrypter(client_framer, ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* client_forward_secure_decrypter(
      QuicStreamPeer::session(client)->connection()->alternative_decrypter());
  const QuicEncrypter* server_encrypter(
      QuicFramerPeer::GetEncrypter(server_framer, ENCRYPTION_INITIAL));
  const QuicDecrypter* server_decrypter(
      QuicStreamPeer::session(server)->connection()->decrypter());
  const QuicEncrypter* server_forward_secure_encrypter(
      QuicFramerPeer::GetEncrypter(server_framer, ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* server_forward_secure_decrypter(
      QuicStreamPeer::session(server)->connection()->alternative_decrypter());

  QuicStringPiece client_encrypter_key = client_encrypter->GetKey();
  QuicStringPiece client_encrypter_iv = client_encrypter->GetNoncePrefix();
  QuicStringPiece client_decrypter_key = client_decrypter->GetKey();
  QuicStringPiece client_decrypter_iv = client_decrypter->GetNoncePrefix();
  QuicStringPiece client_forward_secure_encrypter_key =
      client_forward_secure_encrypter->GetKey();
  QuicStringPiece client_forward_secure_encrypter_iv =
      client_forward_secure_encrypter->GetNoncePrefix();
  QuicStringPiece client_forward_secure_decrypter_key =
      client_forward_secure_decrypter->GetKey();
  QuicStringPiece client_forward_secure_decrypter_iv =
      client_forward_secure_decrypter->GetNoncePrefix();
  QuicStringPiece server_encrypter_key = server_encrypter->GetKey();
  QuicStringPiece server_encrypter_iv = server_encrypter->GetNoncePrefix();
  QuicStringPiece server_decrypter_key = server_decrypter->GetKey();
  QuicStringPiece server_decrypter_iv = server_decrypter->GetNoncePrefix();
  QuicStringPiece server_forward_secure_encrypter_key =
      server_forward_secure_encrypter->GetKey();
  QuicStringPiece server_forward_secure_encrypter_iv =
      server_forward_secure_encrypter->GetNoncePrefix();
  QuicStringPiece server_forward_secure_decrypter_key =
      server_forward_secure_decrypter->GetKey();
  QuicStringPiece server_forward_secure_decrypter_iv =
      server_forward_secure_decrypter->GetNoncePrefix();

  QuicStringPiece client_subkey_secret =
      client->crypto_negotiated_params().subkey_secret;
  QuicStringPiece server_subkey_secret =
      server->crypto_negotiated_params().subkey_secret;

  const char kSampleLabel[] = "label";
  const char kSampleContext[] = "context";
  const size_t kSampleOutputLength = 32;
  QuicString client_key_extraction;
  QuicString server_key_extraction;
  QuicString client_tb_ekm;
  QuicString server_tb_ekm;
  EXPECT_TRUE(client->ExportKeyingMaterial(kSampleLabel, kSampleContext,
                                           kSampleOutputLength,
                                           &client_key_extraction));
  EXPECT_TRUE(server->ExportKeyingMaterial(kSampleLabel, kSampleContext,
                                           kSampleOutputLength,
                                           &server_key_extraction));
  EXPECT_TRUE(client->ExportTokenBindingKeyingMaterial(&client_tb_ekm));
  EXPECT_TRUE(server->ExportTokenBindingKeyingMaterial(&server_tb_ekm));

  CompareCharArraysWithHexError("client write key", client_encrypter_key.data(),
                                client_encrypter_key.length(),
                                server_decrypter_key.data(),
                                server_decrypter_key.length());
  CompareCharArraysWithHexError("client write IV", client_encrypter_iv.data(),
                                client_encrypter_iv.length(),
                                server_decrypter_iv.data(),
                                server_decrypter_iv.length());
  CompareCharArraysWithHexError("server write key", server_encrypter_key.data(),
                                server_encrypter_key.length(),
                                client_decrypter_key.data(),
                                client_decrypter_key.length());
  CompareCharArraysWithHexError("server write IV", server_encrypter_iv.data(),
                                server_encrypter_iv.length(),
                                client_decrypter_iv.data(),
                                client_decrypter_iv.length());
  CompareCharArraysWithHexError("client forward secure write key",
                                client_forward_secure_encrypter_key.data(),
                                client_forward_secure_encrypter_key.length(),
                                server_forward_secure_decrypter_key.data(),
                                server_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("client forward secure write IV",
                                client_forward_secure_encrypter_iv.data(),
                                client_forward_secure_encrypter_iv.length(),
                                server_forward_secure_decrypter_iv.data(),
                                server_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("server forward secure write key",
                                server_forward_secure_encrypter_key.data(),
                                server_forward_secure_encrypter_key.length(),
                                client_forward_secure_decrypter_key.data(),
                                client_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("server forward secure write IV",
                                server_forward_secure_encrypter_iv.data(),
                                server_forward_secure_encrypter_iv.length(),
                                client_forward_secure_decrypter_iv.data(),
                                client_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("subkey secret", client_subkey_secret.data(),
                                client_subkey_secret.length(),
                                server_subkey_secret.data(),
                                server_subkey_secret.length());
  CompareCharArraysWithHexError(
      "sample key extraction", client_key_extraction.data(),
      client_key_extraction.length(), server_key_extraction.data(),
      server_key_extraction.length());

  CompareCharArraysWithHexError("token binding key extraction",
                                client_tb_ekm.data(), client_tb_ekm.length(),
                                server_tb_ekm.data(), server_tb_ekm.length());
}

QuicTag ParseTag(const char* tagstr) {
  const size_t len = strlen(tagstr);
  CHECK_NE(0u, len);

  QuicTag tag = 0;

  if (tagstr[0] == '#') {
    CHECK_EQ(static_cast<size_t>(1 + 2 * 4), len);
    tagstr++;

    for (size_t i = 0; i < 8; i++) {
      tag <<= 4;

      uint8_t v = 0;
      CHECK(HexChar(tagstr[i], &v));
      tag |= v;
    }

    return tag;
  }

  CHECK_LE(len, 4u);
  for (size_t i = 0; i < 4; i++) {
    tag >>= 8;
    if (i < len) {
      tag |= static_cast<uint32_t>(tagstr[i]) << 24;
    }
  }

  return tag;
}

CryptoHandshakeMessage CreateCHLO(
    std::vector<std::pair<QuicString, QuicString>> tags_and_values) {
  return CreateCHLO(tags_and_values, -1);
}

CryptoHandshakeMessage CreateCHLO(
    std::vector<std::pair<QuicString, QuicString>> tags_and_values,
    int minimum_size_bytes) {
  CryptoHandshakeMessage msg;
  msg.set_tag(MakeQuicTag('C', 'H', 'L', 'O'));

  if (minimum_size_bytes > 0) {
    msg.set_minimum_size(minimum_size_bytes);
  }

  for (const auto& tag_and_value : tags_and_values) {
    const QuicString& tag = tag_and_value.first;
    const QuicString& value = tag_and_value.second;

    const QuicTag quic_tag = ParseTag(tag.c_str());

    size_t value_len = value.length();
    if (value_len > 0 && value[0] == '#') {
      // This is ascii encoded hex.
      QuicString hex_value =
          QuicTextUtils::HexDecode(QuicStringPiece(&value[1]));
      msg.SetStringPiece(quic_tag, hex_value);
      continue;
    }
    msg.SetStringPiece(quic_tag, value);
  }

  // The CryptoHandshakeMessage needs to be serialized and parsed to ensure
  // that any padding is included.
  std::unique_ptr<QuicData> bytes(CryptoFramer::ConstructHandshakeMessage(msg));
  std::unique_ptr<CryptoHandshakeMessage> parsed(
      CryptoFramer::ParseMessage(bytes->AsStringPiece()));
  CHECK(parsed);

  return *parsed;
}

ChannelIDSource* ChannelIDSourceForTesting() {
  return new TestChannelIDSource();
}

void MovePackets(PacketSavingConnection* source_conn,
                 size_t* inout_packet_index,
                 QuicCryptoStream* dest_stream,
                 PacketSavingConnection* dest_conn,
                 Perspective dest_perspective) {
  SimpleQuicFramer framer(source_conn->supported_versions(), dest_perspective);

  SimpleQuicFramer null_encryption_framer(source_conn->supported_versions(),
                                          dest_perspective);

  size_t index = *inout_packet_index;
  for (; index < source_conn->encrypted_packets_.size(); index++) {
    // In order to properly test the code we need to perform encryption and
    // decryption so that the crypters latch when expected. The crypters are in
    // |dest_conn|, but we don't want to try and use them there. Instead we swap
    // them into |framer|, perform the decryption with them, and then swap ther
    // back.
    QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());
    if (!framer.ProcessPacket(*source_conn->encrypted_packets_[index])) {
      // The framer will be unable to decrypt forward-secure packets sent after
      // the handshake is complete. Don't treat them as handshake packets.
      break;
    }
    QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

    if (dest_stream->handshake_protocol() == PROTOCOL_TLS1_3) {
      // Try to process the packet with a framer that only has the NullDecrypter
      // for decryption. If ProcessPacket succeeds, that means the packet was
      // encrypted with the NullEncrypter. With the TLS handshaker in use, no
      // packets should ever be encrypted with the NullEncrypter, instead
      // they're encrypted with an obfuscation cipher based on QUIC version and
      // connection ID.
      ASSERT_FALSE(null_encryption_framer.ProcessPacket(
          *source_conn->encrypted_packets_[index]))
          << "No TLS packets should be encrypted with the NullEncrypter";
    }

    QuicConnectionPeer::SetCurrentPacket(
        dest_conn, source_conn->encrypted_packets_[index]->AsStringPiece());
    for (const auto& stream_frame : framer.stream_frames()) {
      dest_stream->OnStreamFrame(*stream_frame);
    }
  }
  *inout_packet_index = index;

  QuicConnectionPeer::SetCurrentPacket(dest_conn, QuicStringPiece(nullptr, 0));
}

CryptoHandshakeMessage GenerateDefaultInchoateCHLO(
    const QuicClock* clock,
    QuicTransportVersion version,
    QuicCryptoServerConfig* crypto_config) {
  // clang-format off
  return CreateCHLO(
      {{"PDMD", "X509"},
       {"AEAD", "AESG"},
       {"KEXS", "C255"},
       {"PUBS", GenerateClientPublicValuesHex().c_str()},
       {"NONC", GenerateClientNonceHex(clock, crypto_config).c_str()},
       {"VER\0", QuicVersionLabelToString(
           QuicVersionToQuicVersionLabel(version)).c_str()}},
      kClientHelloMinimumSize);
  // clang-format on
}

QuicString GenerateClientNonceHex(const QuicClock* clock,
                                  QuicCryptoServerConfig* crypto_config) {
  QuicCryptoServerConfig::ConfigOptions old_config_options;
  QuicCryptoServerConfig::ConfigOptions new_config_options;
  old_config_options.id = "old-config-id";
  delete crypto_config->AddDefaultConfig(QuicRandom::GetInstance(), clock,
                                         old_config_options);
  std::unique_ptr<QuicServerConfigProtobuf> primary_config(
      crypto_config->GenerateConfig(QuicRandom::GetInstance(), clock,
                                    new_config_options));
  primary_config->set_primary_time(clock->WallNow().ToUNIXSeconds());
  std::unique_ptr<CryptoHandshakeMessage> msg(
      crypto_config->AddConfig(std::move(primary_config), clock->WallNow()));
  QuicStringPiece orbit;
  CHECK(msg->GetStringPiece(kORBT, &orbit));
  QuicString nonce;
  CryptoUtils::GenerateNonce(
      clock->WallNow(), QuicRandom::GetInstance(),
      QuicStringPiece(reinterpret_cast<const char*>(orbit.data()),
                      sizeof(orbit.size())),
      &nonce);
  return ("#" + QuicTextUtils::HexEncode(nonce));
}

QuicString GenerateClientPublicValuesHex() {
  char public_value[32];
  memset(public_value, 42, sizeof(public_value));
  return ("#" + QuicTextUtils::HexEncode(public_value, sizeof(public_value)));
}

void GenerateFullCHLO(const CryptoHandshakeMessage& inchoate_chlo,
                      QuicCryptoServerConfig* crypto_config,
                      QuicSocketAddress server_addr,
                      QuicSocketAddress client_addr,
                      QuicTransportVersion version,
                      const QuicClock* clock,
                      QuicReferenceCountedPointer<QuicSignedServerConfig> proof,
                      QuicCompressedCertsCache* compressed_certs_cache,
                      CryptoHandshakeMessage* out) {
  // Pass a inchoate CHLO.
  FullChloGenerator generator(crypto_config, server_addr, client_addr, clock,
                              proof, compressed_certs_cache, out);
  crypto_config->ValidateClientHello(
      inchoate_chlo, client_addr.host(), server_addr, version, clock, proof,
      generator.GetValidateClientHelloCallback());
}

}  // namespace crypto_test_utils
}  // namespace test
}  // namespace quic
