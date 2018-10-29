// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/quic_test_server.h"

#include "net/third_party/quic/core/quic_epoll_alarm_factory.h"
#include "net/third_party/quic/core/quic_epoll_connection_helper.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "net/third_party/quic/tools/quic_simple_dispatcher.h"
#include "net/third_party/quic/tools/quic_simple_server_session.h"

namespace quic {

namespace test {

class CustomStreamSession : public QuicSimpleServerSession {
 public:
  CustomStreamSession(
      const QuicConfig& config,
      const ParsedQuicVersionVector& supported_versions,
      QuicConnection* connection,
      QuicSession::Visitor* visitor,
      QuicCryptoServerStream::Helper* helper,
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QuicTestServer::StreamFactory* stream_factory,
      QuicTestServer::CryptoStreamFactory* crypto_stream_factory,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerSession(config,
                                supported_versions,
                                connection,
                                visitor,
                                helper,
                                crypto_config,
                                compressed_certs_cache,
                                quic_simple_server_backend),
        stream_factory_(stream_factory),
        crypto_stream_factory_(crypto_stream_factory) {}

  QuicSpdyStream* CreateIncomingStream(QuicStreamId id) override {
    if (!ShouldCreateIncomingStream(id)) {
      return nullptr;
    }
    if (stream_factory_) {
      QuicSpdyStream* stream =
          stream_factory_->CreateStream(id, this, server_backend());
      ActivateStream(QuicWrapUnique(stream));
      return stream;
    }
    return QuicSimpleServerSession::CreateIncomingStream(id);
  }

  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override {
    if (crypto_stream_factory_) {
      return crypto_stream_factory_->CreateCryptoStream(crypto_config, this);
    }
    return QuicSimpleServerSession::CreateQuicCryptoServerStream(
        crypto_config, compressed_certs_cache);
  }

 private:
  QuicTestServer::StreamFactory* stream_factory_;               // Not owned.
  QuicTestServer::CryptoStreamFactory* crypto_stream_factory_;  // Not owned.
};

class QuicTestDispatcher : public QuicSimpleDispatcher {
 public:
  QuicTestDispatcher(
      const QuicConfig& config,
      const QuicCryptoServerConfig* crypto_config,
      QuicVersionManager* version_manager,
      std::unique_ptr<QuicConnectionHelperInterface> helper,
      std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
      std::unique_ptr<QuicAlarmFactory> alarm_factory,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleDispatcher(config,
                             crypto_config,
                             version_manager,
                             std::move(helper),
                             std::move(session_helper),
                             std::move(alarm_factory),
                             quic_simple_server_backend),
        session_factory_(nullptr),
        stream_factory_(nullptr),
        crypto_stream_factory_(nullptr) {}

  QuicServerSessionBase* CreateQuicSession(QuicConnectionId id,
                                           const QuicSocketAddress& client,
                                           QuicStringPiece alpn) override {
    QuicReaderMutexLock lock(&factory_lock_);
    if (session_factory_ == nullptr && stream_factory_ == nullptr &&
        crypto_stream_factory_ == nullptr) {
      return QuicSimpleDispatcher::CreateQuicSession(id, client, alpn);
    }
    QuicConnection* connection =
        new QuicConnection(id, client, helper(), alarm_factory(), writer(),
                           /* owns_writer= */ false, Perspective::IS_SERVER,
                           ParsedQuicVersionVector{framer()->version()});

    QuicServerSessionBase* session = nullptr;
    if (stream_factory_ != nullptr || crypto_stream_factory_ != nullptr) {
      session = new CustomStreamSession(
          config(), GetSupportedVersions(), connection, this, session_helper(),
          crypto_config(), compressed_certs_cache(), stream_factory_,
          crypto_stream_factory_, server_backend());
    } else {
      session = session_factory_->CreateSession(
          config(), connection, this, session_helper(), crypto_config(),
          compressed_certs_cache(), server_backend());
    }
    session->Initialize();
    return session;
  }

  void SetSessionFactory(QuicTestServer::SessionFactory* factory) {
    QuicWriterMutexLock lock(&factory_lock_);
    DCHECK(session_factory_ == nullptr);
    DCHECK(stream_factory_ == nullptr);
    DCHECK(crypto_stream_factory_ == nullptr);
    session_factory_ = factory;
  }

  void SetStreamFactory(QuicTestServer::StreamFactory* factory) {
    QuicWriterMutexLock lock(&factory_lock_);
    DCHECK(session_factory_ == nullptr);
    DCHECK(stream_factory_ == nullptr);
    stream_factory_ = factory;
  }

  void SetCryptoStreamFactory(QuicTestServer::CryptoStreamFactory* factory) {
    QuicWriterMutexLock lock(&factory_lock_);
    DCHECK(session_factory_ == nullptr);
    DCHECK(crypto_stream_factory_ == nullptr);
    crypto_stream_factory_ = factory;
  }

 private:
  QuicMutex factory_lock_;
  QuicTestServer::SessionFactory* session_factory_;             // Not owned.
  QuicTestServer::StreamFactory* stream_factory_;               // Not owned.
  QuicTestServer::CryptoStreamFactory* crypto_stream_factory_;  // Not owned.
};

QuicTestServer::QuicTestServer(
    std::unique_ptr<ProofSource> proof_source,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicServer(std::move(proof_source), quic_simple_server_backend) {}

QuicTestServer::QuicTestServer(
    std::unique_ptr<ProofSource> proof_source,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicServer(std::move(proof_source),
                 config,
                 QuicCryptoServerConfig::ConfigOptions(),
                 supported_versions,
                 quic_simple_server_backend) {}

QuicDispatcher* QuicTestServer::CreateQuicDispatcher() {
  return new QuicTestDispatcher(
      config(), &crypto_config(), version_manager(),
      QuicMakeUnique<QuicEpollConnectionHelper>(epoll_server(),
                                                QuicAllocator::BUFFER_POOL),
      std::unique_ptr<QuicCryptoServerStream::Helper>(
          new QuicSimpleCryptoServerStreamHelper(QuicRandom::GetInstance())),
      QuicMakeUnique<QuicEpollAlarmFactory>(epoll_server()), server_backend());
}

void QuicTestServer::SetSessionFactory(SessionFactory* factory) {
  DCHECK(dispatcher());
  static_cast<QuicTestDispatcher*>(dispatcher())->SetSessionFactory(factory);
}

void QuicTestServer::SetSpdyStreamFactory(StreamFactory* factory) {
  static_cast<QuicTestDispatcher*>(dispatcher())->SetStreamFactory(factory);
}

void QuicTestServer::SetCryptoStreamFactory(CryptoStreamFactory* factory) {
  static_cast<QuicTestDispatcher*>(dispatcher())
      ->SetCryptoStreamFactory(factory);
}

///////////////////////////   TEST SESSIONS ///////////////////////////////

ImmediateGoAwaySession::ImmediateGoAwaySession(
    const QuicConfig& config,
    QuicConnection* connection,
    QuicSession::Visitor* visitor,
    QuicCryptoServerStream::Helper* helper,
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSimpleServerSession(config,
                              CurrentSupportedVersions(),
                              connection,
                              visitor,
                              helper,
                              crypto_config,
                              compressed_certs_cache,
                              quic_simple_server_backend) {}

void ImmediateGoAwaySession::OnStreamFrame(const QuicStreamFrame& frame) {
  SendGoAway(QUIC_PEER_GOING_AWAY, "");
  QuicSimpleServerSession::OnStreamFrame(frame);
}

}  // namespace test

}  // namespace quic
