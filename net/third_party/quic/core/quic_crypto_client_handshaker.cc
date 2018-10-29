// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_crypto_client_handshaker.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

QuicCryptoClientHandshaker::ChannelIDSourceCallbackImpl::
    ChannelIDSourceCallbackImpl(QuicCryptoClientHandshaker* parent)
    : parent_(parent) {}

QuicCryptoClientHandshaker::ChannelIDSourceCallbackImpl::
    ~ChannelIDSourceCallbackImpl() {}

void QuicCryptoClientHandshaker::ChannelIDSourceCallbackImpl::Run(
    std::unique_ptr<ChannelIDKey>* channel_id_key) {
  if (parent_ == nullptr) {
    return;
  }

  parent_->channel_id_key_ = std::move(*channel_id_key);
  parent_->channel_id_source_callback_run_ = true;
  parent_->channel_id_source_callback_ = nullptr;
  parent_->DoHandshakeLoop(nullptr);

  // The ChannelIDSource owns this object and will delete it when this method
  // returns.
}

void QuicCryptoClientHandshaker::ChannelIDSourceCallbackImpl::Cancel() {
  parent_ = nullptr;
}

QuicCryptoClientHandshaker::ProofVerifierCallbackImpl::
    ProofVerifierCallbackImpl(QuicCryptoClientHandshaker* parent)
    : parent_(parent) {}

QuicCryptoClientHandshaker::ProofVerifierCallbackImpl::
    ~ProofVerifierCallbackImpl() {}

void QuicCryptoClientHandshaker::ProofVerifierCallbackImpl::Run(
    bool ok,
    const QuicString& error_details,
    std::unique_ptr<ProofVerifyDetails>* details) {
  if (parent_ == nullptr) {
    return;
  }

  parent_->verify_ok_ = ok;
  parent_->verify_error_details_ = error_details;
  parent_->verify_details_ = std::move(*details);
  parent_->proof_verify_callback_ = nullptr;
  parent_->DoHandshakeLoop(nullptr);

  // The ProofVerifier owns this object and will delete it when this method
  // returns.
}

void QuicCryptoClientHandshaker::ProofVerifierCallbackImpl::Cancel() {
  parent_ = nullptr;
}

QuicCryptoClientHandshaker::QuicCryptoClientHandshaker(
    const QuicServerId& server_id,
    QuicCryptoClientStream* stream,
    QuicSession* session,
    std::unique_ptr<ProofVerifyContext> verify_context,
    QuicCryptoClientConfig* crypto_config,
    QuicCryptoClientStream::ProofHandler* proof_handler)
    : QuicCryptoHandshaker(stream, session),
      stream_(stream),
      session_(session),
      next_state_(STATE_IDLE),
      num_client_hellos_(0),
      crypto_config_(crypto_config),
      server_id_(server_id),
      generation_counter_(0),
      channel_id_sent_(false),
      channel_id_source_callback_run_(false),
      channel_id_source_callback_(nullptr),
      verify_context_(std::move(verify_context)),
      proof_verify_callback_(nullptr),
      proof_handler_(proof_handler),
      verify_ok_(false),
      stateless_reject_received_(false),
      num_scup_messages_received_(0),
      encryption_established_(false),
      handshake_confirmed_(false),
      crypto_negotiated_params_(new QuicCryptoNegotiatedParameters) {}

QuicCryptoClientHandshaker::~QuicCryptoClientHandshaker() {
  if (channel_id_source_callback_) {
    channel_id_source_callback_->Cancel();
  }
  if (proof_verify_callback_) {
    proof_verify_callback_->Cancel();
  }
}

void QuicCryptoClientHandshaker::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  QuicCryptoHandshaker::OnHandshakeMessage(message);
  if (message.tag() == kSCUP) {
    if (!handshake_confirmed()) {
      stream_->CloseConnectionWithDetails(
          QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE,
          "Early SCUP disallowed");
      return;
    }

    // |message| is an update from the server, so we treat it differently from a
    // handshake message.
    HandleServerConfigUpdateMessage(message);
    num_scup_messages_received_++;
    return;
  }

  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed()) {
    stream_->CloseConnectionWithDetails(
        QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
        "Unexpected handshake message");
    return;
  }

  DoHandshakeLoop(&message);
}

bool QuicCryptoClientHandshaker::CryptoConnect() {
  next_state_ = STATE_INITIALIZE;
  DoHandshakeLoop(nullptr);
  return session()->connection()->connected();
}

int QuicCryptoClientHandshaker::num_sent_client_hellos() const {
  return num_client_hellos_;
}

int QuicCryptoClientHandshaker::num_scup_messages_received() const {
  return num_scup_messages_received_;
}

bool QuicCryptoClientHandshaker::WasChannelIDSent() const {
  return channel_id_sent_;
}

bool QuicCryptoClientHandshaker::WasChannelIDSourceCallbackRun() const {
  return channel_id_source_callback_run_;
}

QuicLongHeaderType QuicCryptoClientHandshaker::GetLongHeaderType(
    QuicStreamOffset offset) const {
  return offset == 0 ? INITIAL : HANDSHAKE;
}

QuicString QuicCryptoClientHandshaker::chlo_hash() const {
  return chlo_hash_;
}

bool QuicCryptoClientHandshaker::encryption_established() const {
  return encryption_established_;
}

bool QuicCryptoClientHandshaker::handshake_confirmed() const {
  return handshake_confirmed_;
}

const QuicCryptoNegotiatedParameters&
QuicCryptoClientHandshaker::crypto_negotiated_params() const {
  return *crypto_negotiated_params_;
}

CryptoMessageParser* QuicCryptoClientHandshaker::crypto_message_parser() {
  return QuicCryptoHandshaker::crypto_message_parser();
}

void QuicCryptoClientHandshaker::HandleServerConfigUpdateMessage(
    const CryptoHandshakeMessage& server_config_update) {
  DCHECK(server_config_update.tag() == kSCUP);
  QuicString error_details;
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_->LookupOrCreate(server_id_);
  QuicErrorCode error = crypto_config_->ProcessServerConfigUpdate(
      server_config_update, session()->connection()->clock()->WallNow(),
      session()->connection()->transport_version(), chlo_hash_, cached,
      crypto_negotiated_params_, &error_details);

  if (error != QUIC_NO_ERROR) {
    stream_->CloseConnectionWithDetails(
        error, "Server config update invalid: " + error_details);
    return;
  }

  DCHECK(handshake_confirmed());
  if (proof_verify_callback_) {
    proof_verify_callback_->Cancel();
  }
  next_state_ = STATE_INITIALIZE_SCUP;
  DoHandshakeLoop(nullptr);
}

void QuicCryptoClientHandshaker::DoHandshakeLoop(
    const CryptoHandshakeMessage* in) {
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_->LookupOrCreate(server_id_);

  QuicAsyncStatus rv = QUIC_SUCCESS;
  do {
    CHECK_NE(STATE_NONE, next_state_);
    const State state = next_state_;
    next_state_ = STATE_IDLE;
    rv = QUIC_SUCCESS;
    switch (state) {
      case STATE_INITIALIZE:
        DoInitialize(cached);
        break;
      case STATE_SEND_CHLO:
        DoSendCHLO(cached);
        return;  // return waiting to hear from server.
      case STATE_RECV_REJ:
        DoReceiveREJ(in, cached);
        break;
      case STATE_VERIFY_PROOF:
        rv = DoVerifyProof(cached);
        break;
      case STATE_VERIFY_PROOF_COMPLETE:
        DoVerifyProofComplete(cached);
        break;
      case STATE_GET_CHANNEL_ID:
        rv = DoGetChannelID(cached);
        break;
      case STATE_GET_CHANNEL_ID_COMPLETE:
        DoGetChannelIDComplete();
        break;
      case STATE_RECV_SHLO:
        DoReceiveSHLO(in, cached);
        break;
      case STATE_IDLE:
        // This means that the peer sent us a message that we weren't expecting.
        stream_->CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                            "Handshake in idle state");
        return;
      case STATE_INITIALIZE_SCUP:
        DoInitializeServerConfigUpdate(cached);
        break;
      case STATE_NONE:
        QUIC_NOTREACHED();
        return;  // We are done.
    }
  } while (rv != QUIC_PENDING && next_state_ != STATE_NONE);
}

void QuicCryptoClientHandshaker::DoInitialize(
    QuicCryptoClientConfig::CachedState* cached) {
  if (!cached->IsEmpty() && !cached->signature().empty()) {
    // Note that we verify the proof even if the cached proof is valid.
    // This allows us to respond to CA trust changes or certificate
    // expiration because it may have been a while since we last verified
    // the proof.
    DCHECK(crypto_config_->proof_verifier());
    // Track proof verification time when cached server config is used.
    proof_verify_start_time_ = base::TimeTicks::Now();
    chlo_hash_ = cached->chlo_hash();
    // If the cached state needs to be verified, do it now.
    next_state_ = STATE_VERIFY_PROOF;
  } else {
    next_state_ = STATE_GET_CHANNEL_ID;
  }
}

void QuicCryptoClientHandshaker::DoSendCHLO(
    QuicCryptoClientConfig::CachedState* cached) {
  if (stateless_reject_received_) {
    // If we've gotten to this point, we've sent at least one hello
    // and received a stateless reject in response.  We cannot
    // continue to send hellos because the server has abandoned state
    // for this connection.  Abandon further handshakes.
    next_state_ = STATE_NONE;
    if (session()->connection()->connected()) {
      session()->connection()->CloseConnection(
          QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, "stateless reject received",
          ConnectionCloseBehavior::SILENT_CLOSE);
    }
    return;
  }

  // Send the client hello in plaintext.
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_NONE);
  encryption_established_ = false;
  if (num_client_hellos_ > QuicCryptoClientStream::kMaxClientHellos) {
    stream_->CloseConnectionWithDetails(
        QUIC_CRYPTO_TOO_MANY_REJECTS,
        QuicStrCat("More than ", QuicCryptoClientStream::kMaxClientHellos,
                   " rejects"));
    return;
  }
  num_client_hellos_++;

  CryptoHandshakeMessage out;
  DCHECK(session() != nullptr);
  DCHECK(session()->config() != nullptr);
  // Send all the options, regardless of whether we're sending an
  // inchoate or subsequent hello.
  session()->config()->ToHandshakeMessage(&out);

  if (!cached->IsComplete(session()->connection()->clock()->WallNow())) {
    crypto_config_->FillInchoateClientHello(
        server_id_, session()->connection()->supported_versions().front(),
        cached, session()->connection()->random_generator(),
        /* demand_x509_proof= */ true, crypto_negotiated_params_, &out);
    // Pad the inchoate client hello to fill up a packet.
    const QuicByteCount kFramingOverhead = 50;  // A rough estimate.
    const QuicByteCount max_packet_size =
        session()->connection()->max_packet_length();
    if (max_packet_size <= kFramingOverhead) {
      QUIC_DLOG(DFATAL) << "max_packet_length (" << max_packet_size
                        << ") has no room for framing overhead.";
      stream_->CloseConnectionWithDetails(QUIC_INTERNAL_ERROR,
                                          "max_packet_size too smalll");
      return;
    }
    if (kClientHelloMinimumSize > max_packet_size - kFramingOverhead) {
      QUIC_DLOG(DFATAL) << "Client hello won't fit in a single packet.";
      stream_->CloseConnectionWithDetails(QUIC_INTERNAL_ERROR,
                                          "CHLO too large");
      return;
    }
    // TODO(rch): Remove this when we remove:
    // FLAGS_quic_reloadable_flag_quic_use_chlo_packet_size
    out.set_minimum_size(
        static_cast<size_t>(max_packet_size - kFramingOverhead));
    next_state_ = STATE_RECV_REJ;
    CryptoUtils::HashHandshakeMessage(out, &chlo_hash_, Perspective::IS_CLIENT);
    SendHandshakeMessage(out);
    return;
  }

  // If the server nonce is empty, copy over the server nonce from a previous
  // SREJ, if there is one.
  if (GetQuicReloadableFlag(enable_quic_stateless_reject_support) &&
      crypto_negotiated_params_->server_nonce.empty() &&
      cached->has_server_nonce()) {
    crypto_negotiated_params_->server_nonce = cached->GetNextServerNonce();
    DCHECK(!crypto_negotiated_params_->server_nonce.empty());
  }

  QuicString error_details;
  QuicErrorCode error = crypto_config_->FillClientHello(
      server_id_, session()->connection()->connection_id(),
      session()->connection()->supported_versions().front(), cached,
      session()->connection()->clock()->WallNow(),
      session()->connection()->random_generator(), channel_id_key_.get(),
      crypto_negotiated_params_, &out, &error_details);
  if (error != QUIC_NO_ERROR) {
    // Flush the cached config so that, if it's bad, the server has a
    // chance to send us another in the future.
    cached->InvalidateServerConfig();
    stream_->CloseConnectionWithDetails(error, error_details);
    return;
  }
  CryptoUtils::HashHandshakeMessage(out, &chlo_hash_, Perspective::IS_CLIENT);
  channel_id_sent_ = (channel_id_key_ != nullptr);
  if (cached->proof_verify_details()) {
    proof_handler_->OnProofVerifyDetailsAvailable(
        *cached->proof_verify_details());
  }
  next_state_ = STATE_RECV_SHLO;
  SendHandshakeMessage(out);
  // Be prepared to decrypt with the new server write key.
  session()->connection()->SetAlternativeDecrypter(
      ENCRYPTION_INITIAL,
      std::move(crypto_negotiated_params_->initial_crypters.decrypter),
      true /* latch once used */);
  // Send subsequent packets under encryption on the assumption that the
  // server will accept the handshake.
  session()->connection()->SetEncrypter(
      ENCRYPTION_INITIAL,
      std::move(crypto_negotiated_params_->initial_crypters.encrypter));
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);

  // TODO(ianswett): Merge ENCRYPTION_REESTABLISHED and
  // ENCRYPTION_FIRST_ESTABLSIHED
  encryption_established_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::ENCRYPTION_REESTABLISHED);
}

void QuicCryptoClientHandshaker::DoReceiveREJ(
    const CryptoHandshakeMessage* in,
    QuicCryptoClientConfig::CachedState* cached) {
  // We sent a dummy CHLO because we didn't have enough information to
  // perform a handshake, or we sent a full hello that the server
  // rejected. Here we hope to have a REJ that contains the information
  // that we need.
  if ((in->tag() != kREJ) && (in->tag() != kSREJ)) {
    next_state_ = STATE_NONE;
    stream_->CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                        "Expected REJ");
    return;
  }

  QuicTagVector reject_reasons;
  static_assert(sizeof(QuicTag) == sizeof(uint32_t), "header out of sync");
  if (in->GetTaglist(kRREJ, &reject_reasons) == QUIC_NO_ERROR) {
    uint32_t packed_error = 0;
    for (size_t i = 0; i < reject_reasons.size(); ++i) {
      // HANDSHAKE_OK is 0 and don't report that as error.
      if (reject_reasons[i] == HANDSHAKE_OK || reject_reasons[i] >= 32) {
        continue;
      }
      HandshakeFailureReason reason =
          static_cast<HandshakeFailureReason>(reject_reasons[i]);
      packed_error |= 1 << (reason - 1);
    }
    DVLOG(1) << "Reasons for rejection: " << packed_error;
    if (num_client_hellos_ == QuicCryptoClientStream::kMaxClientHellos) {
      base::UmaHistogramSparse("Net.QuicClientHelloRejectReasons.TooMany",
                               packed_error);
    }
    base::UmaHistogramSparse("Net.QuicClientHelloRejectReasons.Secure",
                             packed_error);
  }

  // Receipt of a REJ message means that the server received the CHLO
  // so we can cancel and retransmissions.
  session()->NeuterUnencryptedData();

  stateless_reject_received_ = in->tag() == kSREJ;
  QuicString error_details;
  QuicErrorCode error = crypto_config_->ProcessRejection(
      *in, session()->connection()->clock()->WallNow(),
      session()->connection()->transport_version(), chlo_hash_, cached,
      crypto_negotiated_params_, &error_details);

  if (error != QUIC_NO_ERROR) {
    next_state_ = STATE_NONE;
    stream_->CloseConnectionWithDetails(error, error_details);
    return;
  }
  if (!cached->proof_valid()) {
    if (!cached->signature().empty()) {
      // Note that we only verify the proof if the cached proof is not
      // valid. If the cached proof is valid here, someone else must have
      // just added the server config to the cache and verified the proof,
      // so we can assume no CA trust changes or certificate expiration
      // has happened since then.
      next_state_ = STATE_VERIFY_PROOF;
      return;
    }
  }
  next_state_ = STATE_GET_CHANNEL_ID;
}

QuicAsyncStatus QuicCryptoClientHandshaker::DoVerifyProof(
    QuicCryptoClientConfig::CachedState* cached) {
  ProofVerifier* verifier = crypto_config_->proof_verifier();
  DCHECK(verifier);
  next_state_ = STATE_VERIFY_PROOF_COMPLETE;
  generation_counter_ = cached->generation_counter();

  ProofVerifierCallbackImpl* proof_verify_callback =
      new ProofVerifierCallbackImpl(this);

  verify_ok_ = false;

  QuicAsyncStatus status = verifier->VerifyProof(
      server_id_.host(), server_id_.port(), cached->server_config(),
      session()->connection()->transport_version(), chlo_hash_, cached->certs(),
      cached->cert_sct(), cached->signature(), verify_context_.get(),
      &verify_error_details_, &verify_details_,
      std::unique_ptr<ProofVerifierCallback>(proof_verify_callback));

  switch (status) {
    case QUIC_PENDING:
      proof_verify_callback_ = proof_verify_callback;
      QUIC_DVLOG(1) << "Doing VerifyProof";
      break;
    case QUIC_FAILURE:
      break;
    case QUIC_SUCCESS:
      verify_ok_ = true;
      break;
  }
  return status;
}

void QuicCryptoClientHandshaker::DoVerifyProofComplete(
    QuicCryptoClientConfig::CachedState* cached) {
  if (!proof_verify_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.VerifyProofTime.CachedServerConfig",
                        base::TimeTicks::Now() - proof_verify_start_time_);
  }
  if (!verify_ok_) {
    if (verify_details_) {
      proof_handler_->OnProofVerifyDetailsAvailable(*verify_details_);
    }
    if (num_client_hellos_ == 0) {
      cached->Clear();
      next_state_ = STATE_INITIALIZE;
      return;
    }
    next_state_ = STATE_NONE;
    UMA_HISTOGRAM_BOOLEAN("Net.QuicVerifyProofFailed.HandshakeConfirmed",
                          handshake_confirmed());
    stream_->CloseConnectionWithDetails(
        QUIC_PROOF_INVALID, "Proof invalid: " + verify_error_details_);
    return;
  }

  // Check if generation_counter has changed between STATE_VERIFY_PROOF and
  // STATE_VERIFY_PROOF_COMPLETE state changes.
  if (generation_counter_ != cached->generation_counter()) {
    next_state_ = STATE_VERIFY_PROOF;
  } else {
    SetCachedProofValid(cached);
    cached->SetProofVerifyDetails(verify_details_.release());
    if (!handshake_confirmed()) {
      next_state_ = STATE_GET_CHANNEL_ID;
    } else {
      // TODO: Enable Expect-Staple. https://crbug.com/631101
      next_state_ = STATE_NONE;
    }
  }
}

QuicAsyncStatus QuicCryptoClientHandshaker::DoGetChannelID(
    QuicCryptoClientConfig::CachedState* cached) {
  next_state_ = STATE_GET_CHANNEL_ID_COMPLETE;
  channel_id_key_.reset();
  if (!RequiresChannelID(cached)) {
    next_state_ = STATE_SEND_CHLO;
    return QUIC_SUCCESS;
  }

  ChannelIDSourceCallbackImpl* channel_id_source_callback =
      new ChannelIDSourceCallbackImpl(this);
  QuicAsyncStatus status = crypto_config_->channel_id_source()->GetChannelIDKey(
      server_id_.host(), &channel_id_key_, channel_id_source_callback);

  switch (status) {
    case QUIC_PENDING:
      channel_id_source_callback_ = channel_id_source_callback;
      QUIC_DVLOG(1) << "Looking up channel ID";
      break;
    case QUIC_FAILURE:
      next_state_ = STATE_NONE;
      delete channel_id_source_callback;
      stream_->CloseConnectionWithDetails(QUIC_INVALID_CHANNEL_ID_SIGNATURE,
                                          "Channel ID lookup failed");
      break;
    case QUIC_SUCCESS:
      delete channel_id_source_callback;
      break;
  }
  return status;
}

void QuicCryptoClientHandshaker::DoGetChannelIDComplete() {
  if (!channel_id_key_.get()) {
    next_state_ = STATE_NONE;
    stream_->CloseConnectionWithDetails(QUIC_INVALID_CHANNEL_ID_SIGNATURE,
                                        "Channel ID lookup failed");
    return;
  }
  next_state_ = STATE_SEND_CHLO;
}

void QuicCryptoClientHandshaker::DoReceiveSHLO(
    const CryptoHandshakeMessage* in,
    QuicCryptoClientConfig::CachedState* cached) {
  next_state_ = STATE_NONE;
  // We sent a CHLO that we expected to be accepted and now we're
  // hoping for a SHLO from the server to confirm that.  First check
  // to see whether the response was a reject, and if so, move on to
  // the reject-processing state.
  if ((in->tag() == kREJ) || (in->tag() == kSREJ)) {
    // alternative_decrypter will be nullptr if the original alternative
    // decrypter latched and became the primary decrypter. That happens
    // if we received a message encrypted with the INITIAL key.
    if (session()->connection()->alternative_decrypter() == nullptr) {
      // The rejection was sent encrypted!
      stream_->CloseConnectionWithDetails(
          QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT, "encrypted REJ message");
      return;
    }
    next_state_ = STATE_RECV_REJ;
    return;
  }

  if (in->tag() != kSHLO) {
    stream_->CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                        "Expected SHLO or REJ");
    return;
  }

  // alternative_decrypter will be nullptr if the original alternative
  // decrypter latched and became the primary decrypter. That happens
  // if we received a message encrypted with the INITIAL key.
  if (session()->connection()->alternative_decrypter() != nullptr) {
    // The server hello was sent without encryption.
    stream_->CloseConnectionWithDetails(QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
                                        "unencrypted SHLO message");
    return;
  }

  QuicString error_details;
  QuicErrorCode error = crypto_config_->ProcessServerHello(
      *in, session()->connection()->connection_id(),
      session()->connection()->version(),
      session()->connection()->server_supported_versions(), cached,
      crypto_negotiated_params_, &error_details);

  if (error != QUIC_NO_ERROR) {
    stream_->CloseConnectionWithDetails(
        error, "Server hello invalid: " + error_details);
    return;
  }
  error = session()->config()->ProcessPeerHello(*in, SERVER, &error_details);
  if (error != QUIC_NO_ERROR) {
    stream_->CloseConnectionWithDetails(
        error, "Server hello invalid: " + error_details);
    return;
  }
  session()->OnConfigNegotiated();

  CrypterPair* crypters = &crypto_negotiated_params_->forward_secure_crypters;
  // TODO(agl): we don't currently latch this decrypter because the idea
  // has been floated that the server shouldn't send packets encrypted
  // with the FORWARD_SECURE key until it receives a FORWARD_SECURE
  // packet from the client.
  session()->connection()->SetAlternativeDecrypter(
      ENCRYPTION_FORWARD_SECURE, std::move(crypters->decrypter),
      false /* don't latch */);
  session()->connection()->SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                                        std::move(crypters->encrypter));
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  session()->connection()->OnHandshakeComplete();
}

void QuicCryptoClientHandshaker::DoInitializeServerConfigUpdate(
    QuicCryptoClientConfig::CachedState* cached) {
  bool update_ignored = false;
  if (!cached->IsEmpty() && !cached->signature().empty()) {
    // Note that we verify the proof even if the cached proof is valid.
    DCHECK(crypto_config_->proof_verifier());
    next_state_ = STATE_VERIFY_PROOF;
  } else {
    update_ignored = true;
    next_state_ = STATE_NONE;
  }
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicNumServerConfig.UpdateMessagesIgnored",
                          update_ignored);
}

void QuicCryptoClientHandshaker::SetCachedProofValid(
    QuicCryptoClientConfig::CachedState* cached) {
  cached->SetProofValid();
  proof_handler_->OnProofValid(*cached);
}

bool QuicCryptoClientHandshaker::RequiresChannelID(
    QuicCryptoClientConfig::CachedState* cached) {
  if (server_id_.privacy_mode_enabled() ||
      !crypto_config_->channel_id_source()) {
    return false;
  }
  const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
  if (!scfg) {  // scfg may be null then we send an inchoate CHLO.
    return false;
  }
  QuicTagVector their_proof_demands;
  if (scfg->GetTaglist(kPDMD, &their_proof_demands) != QUIC_NO_ERROR) {
    return false;
  }
  for (const QuicTag tag : their_proof_demands) {
    if (tag == kCHID) {
      return true;
    }
  }
  return false;
}

}  // namespace quic
