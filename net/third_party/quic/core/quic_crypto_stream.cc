// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_crypto_stream.h"

#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

#define ENDPOINT                                                   \
  (session()->perspective() == Perspective::IS_SERVER ? "Server: " \
                                                      : "Client:"  \
                                                        " ")

QuicCryptoStream::QuicCryptoStream(QuicSession* session)
    : QuicStream(QuicUtils::GetCryptoStreamId(
                     session->connection()->transport_version()),
                 session,
                 /*is_static=*/true,
                 BIDIRECTIONAL) {
  // The crypto stream is exempt from connection level flow control.
  DisableConnectionFlowControlForThisStream();
}

QuicCryptoStream::~QuicCryptoStream() {}

// static
QuicByteCount QuicCryptoStream::CryptoMessageFramingOverhead(
    QuicTransportVersion version) {
  return QuicPacketCreator::StreamFramePacketOverhead(
      version, PACKET_8BYTE_CONNECTION_ID, PACKET_0BYTE_CONNECTION_ID,
      /*include_version=*/true,
      /*include_diversification_nonce=*/true,
      version > QUIC_VERSION_43 ? PACKET_4BYTE_PACKET_NUMBER
                                : PACKET_1BYTE_PACKET_NUMBER,
      /*offset=*/0);
}

void QuicCryptoStream::OnDataAvailable() {
  struct iovec iov;
  // When calling CryptoMessageParser::ProcessInput, an EncryptionLevel needs to
  // be provided. Note that in the general case, the following code may be
  // incorrect. When a stream frame is added to the sequencer, the encryption
  // level provided by the connection will be the encryption level that the
  // frame was received under, but stream frames can be received out of order.
  // If a later stream frame at a higher encryption level is received before an
  // earlier stream frame at a lower encryption level, this code will call
  // CryptoMessageParser::Process input with the data from both frames, but
  // indicate that they both were received at the higher encryption level.
  //
  // For QUIC crypto, this is not a problem, because the CryptoFramer (which
  // implements CryptoMessageParser) ignores the EncryptionLevel passed into
  // ProcessInput.
  //
  // For the TLS handshake, this does not cause an issue for the transition from
  // initial encryption (ClientHello, HelloRetryRequest, and ServerHello) to
  // handshake encryption, as all data from the initial encryption level is
  // needed to derive the handshake encryption keys. For the transition from
  // handshake encryption to 1-RTT application data encryption, all messages at
  // the handshake encryption level *except* the client Finished are needed. The
  // only place this logic would be broken is if a server receives a crypto
  // handshake message that is encrypted under the 1-RTT data keys before
  // receiving the client's Finished message (under handshake encryption keys).
  // Right now, this implementation of TLS in QUIC does not support doing that,
  // but it is possible (although unlikely) that other implementations could.
  // Therefore, this needs to be fixed before the TLS handshake is enabled.
  //
  // TODO(nharper): Use a more robust and correct mechanism to provide the
  // EncryptionLevel to CryptoMessageParser::ProcessInput. This must be done
  // before enabling the TLS handshake.
  EncryptionLevel level = session()->connection()->last_decrypted_level();
  while (sequencer()->GetReadableRegion(&iov)) {
    QuicStringPiece data(static_cast<char*>(iov.iov_base), iov.iov_len);
    if (!crypto_message_parser()->ProcessInput(data, level)) {
      CloseConnectionWithDetails(crypto_message_parser()->error(),
                                 crypto_message_parser()->error_detail());
      return;
    }
    sequencer()->MarkConsumed(iov.iov_len);
    if (handshake_confirmed() &&
        crypto_message_parser()->InputBytesRemaining() == 0) {
      // If the handshake is complete and the current message has been fully
      // processed then no more handshake messages are likely to arrive soon
      // so release the memory in the stream sequencer.
      sequencer()->ReleaseBufferIfEmpty();
    }
  }
}

bool QuicCryptoStream::ExportKeyingMaterial(QuicStringPiece label,
                                            QuicStringPiece context,
                                            size_t result_len,
                                            QuicString* result) const {
  if (!handshake_confirmed()) {
    QUIC_DLOG(ERROR) << "ExportKeyingMaterial was called before forward-secure"
                     << "encryption was established.";
    return false;
  }
  return CryptoUtils::ExportKeyingMaterial(
      crypto_negotiated_params().subkey_secret, label, context, result_len,
      result);
}

bool QuicCryptoStream::ExportTokenBindingKeyingMaterial(
    QuicString* result) const {
  if (!encryption_established()) {
    QUIC_BUG << "ExportTokenBindingKeyingMaterial was called before initial"
             << "encryption was established.";
    return false;
  }
  return CryptoUtils::ExportKeyingMaterial(
      crypto_negotiated_params().initial_subkey_secret,
      "EXPORTER-Token-Binding",
      /* context= */ "", 32, result);
}

void QuicCryptoStream::WriteCryptoData(const QuicStringPiece& data) {
  WriteOrBufferData(data, /* fin */ false, /* ack_listener */ nullptr);
}

void QuicCryptoStream::OnSuccessfulVersionNegotiation(
    const ParsedQuicVersion& version) {}

void QuicCryptoStream::NeuterUnencryptedStreamData() {
  for (const auto& interval : bytes_consumed_[ENCRYPTION_NONE]) {
    QuicByteCount newly_acked_length = 0;
    send_buffer().OnStreamDataAcked(
        interval.min(), interval.max() - interval.min(), &newly_acked_length);
  }
}

void QuicCryptoStream::OnStreamDataConsumed(size_t bytes_consumed) {
  if (bytes_consumed > 0) {
    bytes_consumed_[session()->connection()->encryption_level()].Add(
        stream_bytes_written(), stream_bytes_written() + bytes_consumed);
  }
  QuicStream::OnStreamDataConsumed(bytes_consumed);
}

void QuicCryptoStream::WritePendingRetransmission() {
  while (HasPendingRetransmission()) {
    StreamPendingRetransmission pending =
        send_buffer().NextPendingRetransmission();
    QuicIntervalSet<QuicStreamOffset> retransmission(
        pending.offset, pending.offset + pending.length);
    EncryptionLevel retransmission_encryption_level = ENCRYPTION_NONE;
    // Determine the encryption level to write the retransmission
    // at. The retransmission should be written at the same encryption level
    // as the original transmission.
    for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
      if (retransmission.Intersects(bytes_consumed_[i])) {
        retransmission_encryption_level = static_cast<EncryptionLevel>(i);
        retransmission.Intersection(bytes_consumed_[i]);
        break;
      }
    }
    pending.offset = retransmission.begin()->min();
    pending.length =
        retransmission.begin()->max() - retransmission.begin()->min();
    EncryptionLevel current_encryption_level =
        session()->connection()->encryption_level();
    // Set appropriate encryption level.
    session()->connection()->SetDefaultEncryptionLevel(
        retransmission_encryption_level);
    QuicConsumedData consumed = session()->WritevData(
        this, id(), pending.length, pending.offset, NO_FIN);
    QUIC_DVLOG(1) << ENDPOINT << "stream " << id()
                  << " tries to retransmit stream data [" << pending.offset
                  << ", " << pending.offset + pending.length
                  << ") with encryption level: "
                  << retransmission_encryption_level
                  << ", consumed: " << consumed;
    OnStreamFrameRetransmitted(pending.offset, consumed.bytes_consumed,
                               consumed.fin_consumed);
    // Restore encryption level.
    session()->connection()->SetDefaultEncryptionLevel(
        current_encryption_level);
    if (consumed.bytes_consumed < pending.length) {
      // The connection is write blocked.
      break;
    }
  }
}

bool QuicCryptoStream::RetransmitStreamData(QuicStreamOffset offset,
                                            QuicByteCount data_length,
                                            bool /*fin*/) {
  QuicIntervalSet<QuicStreamOffset> retransmission(offset,
                                                   offset + data_length);
  // Determine the encryption level to send data. This only needs to be once as
  // [offset, offset + data_length) is guaranteed to be in the same packet.
  EncryptionLevel send_encryption_level = ENCRYPTION_NONE;
  for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
    if (retransmission.Intersects(bytes_consumed_[i])) {
      send_encryption_level = static_cast<EncryptionLevel>(i);
      break;
    }
  }
  retransmission.Difference(bytes_acked());
  EncryptionLevel current_encryption_level =
      session()->connection()->encryption_level();
  for (const auto& interval : retransmission) {
    QuicStreamOffset retransmission_offset = interval.min();
    QuicByteCount retransmission_length = interval.max() - interval.min();
    // Set appropriate encryption level.
    session()->connection()->SetDefaultEncryptionLevel(send_encryption_level);
    QuicConsumedData consumed = session()->WritevData(
        this, id(), retransmission_length, retransmission_offset, NO_FIN);
    QUIC_DVLOG(1) << ENDPOINT << "stream " << id()
                  << " is forced to retransmit stream data ["
                  << retransmission_offset << ", "
                  << retransmission_offset + retransmission_length
                  << "), with encryption level: " << send_encryption_level
                  << ", consumed: " << consumed;
    OnStreamFrameRetransmitted(retransmission_offset, consumed.bytes_consumed,
                               consumed.fin_consumed);
    // Restore encryption level.
    session()->connection()->SetDefaultEncryptionLevel(
        current_encryption_level);
    if (consumed.bytes_consumed < retransmission_length) {
      // The connection is write blocked.
      return false;
    }
  }

  return true;
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
