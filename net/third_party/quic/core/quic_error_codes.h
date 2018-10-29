// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_ERROR_CODES_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_ERROR_CODES_H_

#include <cstdint>
#include <limits>

#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

enum QuicRstStreamErrorCode {
  // Complete response has been sent, sending a RST to ask the other endpoint
  // to stop sending request data without discarding the response.
  QUIC_STREAM_NO_ERROR = 0,

  // There was some error which halted stream processing.
  QUIC_ERROR_PROCESSING_STREAM,
  // We got two fin or reset offsets which did not match.
  QUIC_MULTIPLE_TERMINATION_OFFSETS,
  // We got bad payload and can not respond to it at the protocol level.
  QUIC_BAD_APPLICATION_PAYLOAD,
  // Stream closed due to connection error. No reset frame is sent when this
  // happens.
  QUIC_STREAM_CONNECTION_ERROR,
  // GoAway frame sent. No more stream can be created.
  QUIC_STREAM_PEER_GOING_AWAY,
  // The stream has been cancelled.
  QUIC_STREAM_CANCELLED,
  // Closing stream locally, sending a RST to allow for proper flow control
  // accounting. Sent in response to a RST from the peer.
  QUIC_RST_ACKNOWLEDGEMENT,
  // Receiver refused to create the stream (because its limit on open streams
  // has been reached).  The sender should retry the request later (using
  // another stream).
  QUIC_REFUSED_STREAM,
  // Invalid URL in PUSH_PROMISE request header.
  QUIC_INVALID_PROMISE_URL,
  // Server is not authoritative for this URL.
  QUIC_UNAUTHORIZED_PROMISE_URL,
  // Can't have more than one active PUSH_PROMISE per URL.
  QUIC_DUPLICATE_PROMISE_URL,
  // Vary check failed.
  QUIC_PROMISE_VARY_MISMATCH,
  // Only GET and HEAD methods allowed.
  QUIC_INVALID_PROMISE_METHOD,
  // The push stream is unclaimed and timed out.
  QUIC_PUSH_STREAM_TIMED_OUT,
  // Received headers were too large.
  QUIC_HEADERS_TOO_LARGE,
  // The data is not likely arrive in time.
  QUIC_STREAM_TTL_EXPIRED,
  // No error. Used as bound while iterating.
  QUIC_STREAM_LAST_ERROR,
};
// QuicRstStreamErrorCode is encoded as a single octet on-the-wire.
static_assert(static_cast<int>(QUIC_STREAM_LAST_ERROR) <=
                  std::numeric_limits<uint8_t>::max(),
              "QuicRstStreamErrorCode exceeds single octet");

// These values must remain stable as they are uploaded to UMA histograms.
// To add a new error code, use the current value of QUIC_LAST_ERROR and
// increment QUIC_LAST_ERROR.
enum QuicErrorCode {
  QUIC_NO_ERROR = 0,

  // Connection has reached an invalid state.
  QUIC_INTERNAL_ERROR = 1,
  // There were data frames after the a fin or reset.
  QUIC_STREAM_DATA_AFTER_TERMINATION = 2,
  // Control frame is malformed.
  QUIC_INVALID_PACKET_HEADER = 3,
  // Frame data is malformed.
  QUIC_INVALID_FRAME_DATA = 4,
  // The packet contained no payload.
  QUIC_MISSING_PAYLOAD = 48,
  // FEC data is malformed.
  QUIC_INVALID_FEC_DATA = 5,
  // STREAM frame data is malformed.
  QUIC_INVALID_STREAM_DATA = 46,
  // STREAM frame data overlaps with buffered data.
  QUIC_OVERLAPPING_STREAM_DATA = 87,
  // Received STREAM frame data is not encrypted.
  QUIC_UNENCRYPTED_STREAM_DATA = 61,
  // Attempt to send unencrypted STREAM frame.
  QUIC_ATTEMPT_TO_SEND_UNENCRYPTED_STREAM_DATA = 88,
  // Received a frame which is likely the result of memory corruption.
  QUIC_MAYBE_CORRUPTED_MEMORY = 89,
  // FEC frame data is not encrypted.
  QUIC_UNENCRYPTED_FEC_DATA = 77,
  // RST_STREAM frame data is malformed.
  QUIC_INVALID_RST_STREAM_DATA = 6,
  // CONNECTION_CLOSE frame data is malformed.
  QUIC_INVALID_CONNECTION_CLOSE_DATA = 7,
  // GOAWAY frame data is malformed.
  QUIC_INVALID_GOAWAY_DATA = 8,
  // WINDOW_UPDATE frame data is malformed.
  QUIC_INVALID_WINDOW_UPDATE_DATA = 57,
  // BLOCKED frame data is malformed.
  QUIC_INVALID_BLOCKED_DATA = 58,
  // STOP_WAITING frame data is malformed.
  QUIC_INVALID_STOP_WAITING_DATA = 60,
  // PATH_CLOSE frame data is malformed.
  QUIC_INVALID_PATH_CLOSE_DATA = 78,
  // ACK frame data is malformed.
  QUIC_INVALID_ACK_DATA = 9,
  // Message frame data is malformed.
  QUIC_INVALID_MESSAGE_DATA = 112,

  // Version negotiation packet is malformed.
  QUIC_INVALID_VERSION_NEGOTIATION_PACKET = 10,
  // Public RST packet is malformed.
  QUIC_INVALID_PUBLIC_RST_PACKET = 11,
  // There was an error decrypting.
  QUIC_DECRYPTION_FAILURE = 12,
  // There was an error encrypting.
  QUIC_ENCRYPTION_FAILURE = 13,
  // The packet exceeded kMaxPacketSize.
  QUIC_PACKET_TOO_LARGE = 14,
  // The peer is going away.  May be a client or server.
  QUIC_PEER_GOING_AWAY = 16,
  // A stream ID was invalid.
  QUIC_INVALID_STREAM_ID = 17,
  // A priority was invalid.
  QUIC_INVALID_PRIORITY = 49,
  // Too many streams already open.
  QUIC_TOO_MANY_OPEN_STREAMS = 18,
  // The peer created too many available streams.
  QUIC_TOO_MANY_AVAILABLE_STREAMS = 76,
  // Received public reset for this connection.
  QUIC_PUBLIC_RESET = 19,
  // Invalid protocol version.
  QUIC_INVALID_VERSION = 20,

  // The Header ID for a stream was too far from the previous.
  QUIC_INVALID_HEADER_ID = 22,
  // Negotiable parameter received during handshake had invalid value.
  QUIC_INVALID_NEGOTIATED_VALUE = 23,
  // There was an error decompressing data.
  QUIC_DECOMPRESSION_FAILURE = 24,
  // The connection timed out due to no network activity.
  QUIC_NETWORK_IDLE_TIMEOUT = 25,
  // The connection timed out waiting for the handshake to complete.
  QUIC_HANDSHAKE_TIMEOUT = 67,
  // There was an error encountered migrating addresses.
  QUIC_ERROR_MIGRATING_ADDRESS = 26,
  // There was an error encountered migrating port only.
  QUIC_ERROR_MIGRATING_PORT = 86,
  // There was an error while writing to the socket.
  QUIC_PACKET_WRITE_ERROR = 27,
  // There was an error while reading from the socket.
  QUIC_PACKET_READ_ERROR = 51,
  // We received a STREAM_FRAME with no data and no fin flag set.
  QUIC_EMPTY_STREAM_FRAME_NO_FIN = 50,
  // We received invalid data on the headers stream.
  QUIC_INVALID_HEADERS_STREAM_DATA = 56,
  // Invalid data on the headers stream received because of decompression
  // failure.
  QUIC_HEADERS_STREAM_DATA_DECOMPRESS_FAILURE = 97,
  // The peer received too much data, violating flow control.
  QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA = 59,
  // The peer sent too much data, violating flow control.
  QUIC_FLOW_CONTROL_SENT_TOO_MUCH_DATA = 63,
  // The peer received an invalid flow control window.
  QUIC_FLOW_CONTROL_INVALID_WINDOW = 64,
  // The connection has been IP pooled into an existing connection.
  QUIC_CONNECTION_IP_POOLED = 62,
  // The connection has too many outstanding sent packets.
  QUIC_TOO_MANY_OUTSTANDING_SENT_PACKETS = 68,
  // The connection has too many outstanding received packets.
  QUIC_TOO_MANY_OUTSTANDING_RECEIVED_PACKETS = 69,
  // The quic connection has been cancelled.
  QUIC_CONNECTION_CANCELLED = 70,
  // Disabled QUIC because of high packet loss rate.
  QUIC_BAD_PACKET_LOSS_RATE = 71,
  // Disabled QUIC because of too many PUBLIC_RESETs post handshake.
  QUIC_PUBLIC_RESETS_POST_HANDSHAKE = 73,
  // Closed because we failed to serialize a packet.
  QUIC_FAILED_TO_SERIALIZE_PACKET = 75,
  // QUIC timed out after too many RTOs.
  QUIC_TOO_MANY_RTOS = 85,

  // Crypto errors.

  // Handshake failed.
  QUIC_HANDSHAKE_FAILED = 28,
  // Handshake message contained out of order tags.
  QUIC_CRYPTO_TAGS_OUT_OF_ORDER = 29,
  // Handshake message contained too many entries.
  QUIC_CRYPTO_TOO_MANY_ENTRIES = 30,
  // Handshake message contained an invalid value length.
  QUIC_CRYPTO_INVALID_VALUE_LENGTH = 31,
  // A crypto message was received after the handshake was complete.
  QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE = 32,
  // A crypto message was received with an illegal message tag.
  QUIC_INVALID_CRYPTO_MESSAGE_TYPE = 33,
  // A crypto message was received with an illegal parameter.
  QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER = 34,
  // An invalid channel id signature was supplied.
  QUIC_INVALID_CHANNEL_ID_SIGNATURE = 52,
  // A crypto message was received with a mandatory parameter missing.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND = 35,
  // A crypto message was received with a parameter that has no overlap
  // with the local parameter.
  QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP = 36,
  // A crypto message was received that contained a parameter with too few
  // values.
  QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND = 37,
  // A demand for an unsupport proof type was received.
  QUIC_UNSUPPORTED_PROOF_DEMAND = 94,
  // An internal error occurred in crypto processing.
  QUIC_CRYPTO_INTERNAL_ERROR = 38,
  // A crypto handshake message specified an unsupported version.
  QUIC_CRYPTO_VERSION_NOT_SUPPORTED = 39,
  // A crypto handshake message resulted in a stateless reject.
  QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT = 72,
  // There was no intersection between the crypto primitives supported by the
  // peer and ourselves.
  QUIC_CRYPTO_NO_SUPPORT = 40,
  // The server rejected our client hello messages too many times.
  QUIC_CRYPTO_TOO_MANY_REJECTS = 41,
  // The client rejected the server's certificate chain or signature.
  QUIC_PROOF_INVALID = 42,
  // A crypto message was received with a duplicate tag.
  QUIC_CRYPTO_DUPLICATE_TAG = 43,
  // A crypto message was received with the wrong encryption level (i.e. it
  // should have been encrypted but was not.)
  QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT = 44,
  // The server config for a server has expired.
  QUIC_CRYPTO_SERVER_CONFIG_EXPIRED = 45,
  // We failed to setup the symmetric keys for a connection.
  QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED = 53,
  // A handshake message arrived, but we are still validating the
  // previous handshake message.
  QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO = 54,
  // A server config update arrived before the handshake is complete.
  QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE = 65,
  // CHLO cannot fit in one packet.
  QUIC_CRYPTO_CHLO_TOO_LARGE = 90,
  // This connection involved a version negotiation which appears to have been
  // tampered with.
  QUIC_VERSION_NEGOTIATION_MISMATCH = 55,

  // Multipath errors.
  // Multipath is not enabled, but a packet with multipath flag on is received.
  QUIC_BAD_MULTIPATH_FLAG = 79,
  // A path is supposed to exist but does not.
  QUIC_MULTIPATH_PATH_DOES_NOT_EXIST = 91,
  // A path is supposed to be active but is not.
  QUIC_MULTIPATH_PATH_NOT_ACTIVE = 92,

  // IP address changed causing connection close.
  QUIC_IP_ADDRESS_CHANGED = 80,

  // Connection migration errors.
  // Network changed, but connection had no migratable streams.
  QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS = 81,
  // Connection changed networks too many times.
  QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES = 82,
  // Connection migration was attempted, but there was no new network to
  // migrate to.
  QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK = 83,
  // Network changed, but connection had one or more non-migratable streams.
  QUIC_CONNECTION_MIGRATION_NON_MIGRATABLE_STREAM = 84,
  // Network changed, but connection migration was disabled by config.
  QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG = 99,
  // Network changed, but error was encountered on the alternative network.
  QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR = 100,
  // Network changed, but handshake is not confirmed yet.
  QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED = 111,

  // Stream frames arrived too discontiguously so that stream sequencer buffer
  // maintains too many intervals.
  QUIC_TOO_MANY_STREAM_DATA_INTERVALS = 93,

  // Sequencer buffer get into weird state where continuing read/write will lead
  // to crash.
  QUIC_STREAM_SEQUENCER_INVALID_STATE = 95,

  // Connection closed because of server hits max number of sessions allowed.
  QUIC_TOO_MANY_SESSIONS_ON_SERVER = 96,

  // Receive a RST_STREAM with offset larger than kMaxStreamLength.
  QUIC_STREAM_LENGTH_OVERFLOW = 98,
  // APPLICATION_CLOSE frame data is malformed.
  QUIC_INVALID_APPLICATION_CLOSE_DATA = 101,
  // Received a MAX DATA frame with errors.
  QUIC_INVALID_MAX_DATA_FRAME_DATA = 102,
  // Received a MAX STREAM DATA frame with errors.
  QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA = 103,
  // Received a MAX STREAM ID frame with bad data
  QUIC_MAX_STREAM_ID_DATA = 104,
  // Received a STREAM ID BLOCKED frame with bad data
  QUIC_STREAM_ID_BLOCKED_DATA = 105,
  // Error deframing a STREAM BLOCKED frame.
  QUIC_INVALID_STREAM_BLOCKED_DATA = 106,
  // NEW CONNECTION ID frame data is malformed.
  QUIC_INVALID_NEW_CONNECTION_ID_DATA = 107,
  // Received a MAX STREAM DATA frame with errors.
  QUIC_INVALID_STOP_SENDING_FRAME_DATA = 108,
  // Error deframing PATH CHALLENGE or PATH RESPONSE frames.
  QUIC_INVALID_PATH_CHALLENGE_DATA = 109,
  QUIC_INVALID_PATH_RESPONSE_DATA = 110,
  // This is used to indicate an IETF QUIC PROTOCOL VIOLATION
  // transport error within Google (pre-v99) QUIC.
  IETF_QUIC_PROTOCOL_VIOLATION = 113,
  QUIC_INVALID_NEW_TOKEN = 114,

  // Received stream data on a WRITE_UNIDIRECTIONAL stream.
  QUIC_DATA_RECEIVED_ON_WRITE_UNIDIRECTIONAL_STREAM = 115,
  // Try to send stream data on a READ_UNIDIRECTIONAL stream.
  QUIC_TRY_TO_WRITE_DATA_ON_READ_UNIDIRECTIONAL_STREAM = 116,

  // RETIRE CONNECTION ID frame data is malformed.
  QUIC_INVALID_RETIRE_CONNECTION_ID_DATA = 117,

  // No error. Used as bound while iterating.
  QUIC_LAST_ERROR = 118,
};
// QuicErrorCodes is encoded as a single octet on-the-wire.
static_assert(static_cast<int>(QUIC_LAST_ERROR) <=
                  std::numeric_limits<uint8_t>::max(),
              "QuicErrorCode exceeds single octet");

// Returns the name of the QuicRstStreamErrorCode as a char*
QUIC_EXPORT_PRIVATE const char* QuicRstStreamErrorCodeToString(
    QuicRstStreamErrorCode error);

// Returns the name of the QuicErrorCode as a char*
QUIC_EXPORT const char* QuicErrorCodeToString(QuicErrorCode error);

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_ERROR_CODES_H_
