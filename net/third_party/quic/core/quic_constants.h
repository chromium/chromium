// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_CONSTANTS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_CONSTANTS_H_

#include <stddef.h>

#include <cstdint>
#include <limits>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"

// Definitions of constant values used throughout the QUIC code.

namespace quic {

// Simple time constants.
const uint64_t kNumSecondsPerMinute = 60;
const uint64_t kNumSecondsPerHour = kNumSecondsPerMinute * 60;
const uint64_t kNumSecondsPerWeek = kNumSecondsPerHour * 24 * 7;
const uint64_t kNumMicrosPerMilli = 1000;
const uint64_t kNumMicrosPerSecond = 1000 * 1000;

// Default number of connections for N-connection emulation.
const uint32_t kDefaultNumConnections = 2;
// Default initial maximum size in bytes of a QUIC packet.
const QuicByteCount kDefaultMaxPacketSize = 1350;
// Default initial maximum size in bytes of a QUIC packet for servers.
const QuicByteCount kDefaultServerMaxPacketSize = 1000;
// The maximum packet size of any QUIC packet, based on ethernet's max size,
// minus the IP and UDP headers. IPv6 has a 40 byte header, UDP adds an
// additional 8 bytes.  This is a total overhead of 48 bytes.  Ethernet's
// max packet size is 1500 bytes,  1500 - 48 = 1452.
const QuicByteCount kMaxPacketSize = 1452;
// The maximum packet size of any QUIC packet over IPv4.
// 1500(Ethernet) - 20(IPv4 header) - 8(UDP header) = 1472.
const QuicByteCount kMaxV4PacketSize = 1472;
// ETH_MAX_MTU - MAX(sizeof(iphdr), sizeof(ip6_hdr)) - sizeof(udphdr).
const QuicByteCount kMaxGsoPacketSize = 65535 - 40 - 8;
// Default maximum packet size used in the Linux TCP implementation.
// Used in QUIC for congestion window computations in bytes.
const QuicByteCount kDefaultTCPMSS = 1460;
const QuicByteCount kMaxSegmentSize = kDefaultTCPMSS;

// We match SPDY's use of 32 (since we'd compete with SPDY).
const QuicPacketCount kInitialCongestionWindow = 32;

// Minimum size of initial flow control window, for both stream and session.
const uint32_t kMinimumFlowControlSendWindow = 16 * 1024;  // 16 KB

// Maximum flow control receive window limits for connection and stream.
const QuicByteCount kStreamReceiveWindowLimit = 16 * 1024 * 1024;   // 16 MB
const QuicByteCount kSessionReceiveWindowLimit = 24 * 1024 * 1024;  // 24 MB

// Default limit on the size of uncompressed headers.
const QuicByteCount kDefaultMaxUncompressedHeaderSize = 16 * 1024;  // 16 KB

// Minimum size of the CWND, in packets, when doing bandwidth resumption.
const QuicPacketCount kMinCongestionWindowForBandwidthResumption = 10;

// Maximum number of tracked packets.
const QuicPacketCount kMaxTrackedPackets = 10000;

// Default size of the socket receive buffer in bytes.
const QuicByteCount kDefaultSocketReceiveBuffer = 1024 * 1024;

// Don't allow a client to suggest an RTT shorter than 10ms.
const uint32_t kMinInitialRoundTripTimeUs = 10 * kNumMicrosPerMilli;

// Don't allow a client to suggest an RTT longer than 15 seconds.
const uint32_t kMaxInitialRoundTripTimeUs = 15 * kNumMicrosPerSecond;

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Number of bytes reserved for public flags in the packet header.
const size_t kPublicFlagsSize = 1;
// Number of bytes reserved for version number in the packet header.
const size_t kQuicVersionSize = 4;
// Number of bytes reserved for path id in the packet header.
const size_t kQuicPathIdSize = 1;
// Number of bytes reserved for private flags in the packet header.
const size_t kPrivateFlagsSize = 1;

// Signifies that the QuicPacket will contain version of the protocol.
const bool kIncludeVersion = true;
// Signifies that the QuicPacket will contain path id.
const bool kIncludePathId = true;
// Signifies that the QuicPacket will include a diversification nonce.
const bool kIncludeDiversificationNonce = true;

// Header key used to identify final offset on data stream when sending HTTP/2
// trailing headers over QUIC.
QUIC_EXPORT_PRIVATE extern const char* const kFinalOffsetHeaderKey;

// Default maximum delayed ack time, in ms.
// Uses a 25ms delayed ack timer. Helps with better signaling
// in low-bandwidth (< ~384 kbps), where an ack is sent per packet.
const int64_t kDefaultDelayedAckTimeMs = 25;

// Minimum tail loss probe time in ms.
static const int64_t kMinTailLossProbeTimeoutMs = 10;

// The timeout before the handshake succeeds.
const int64_t kInitialIdleTimeoutSecs = 5;
// The default idle timeout.
const int64_t kDefaultIdleTimeoutSecs = 30;
// The maximum idle timeout that can be negotiated.
const int64_t kMaximumIdleTimeoutSecs = 60 * 10;  // 10 minutes.
// The default timeout for a connection until the crypto handshake succeeds.
const int64_t kMaxTimeForCryptoHandshakeSecs = 10;  // 10 secs.

// Default limit on the number of undecryptable packets the connection buffers
// before the CHLO/SHLO arrive.
const size_t kDefaultMaxUndecryptablePackets = 10;

// Default ping timeout.
const int64_t kPingTimeoutSecs = 15;  // 15 secs.

// Minimum number of RTTs between Server Config Updates (SCUP) sent to client.
const int kMinIntervalBetweenServerConfigUpdatesRTTs = 10;

// Minimum time between Server Config Updates (SCUP) sent to client.
const int kMinIntervalBetweenServerConfigUpdatesMs = 1000;

// Minimum number of packets between Server Config Updates (SCUP).
const int kMinPacketsBetweenServerConfigUpdates = 100;

// The number of open streams that a server will accept is set to be slightly
// larger than the negotiated limit. Immediately closing the connection if the
// client opens slightly too many streams is not ideal: the client may have sent
// a FIN that was lost, and simultaneously opened a new stream. The number of
// streams a server accepts is a fixed increment over the negotiated limit, or a
// percentage increase, whichever is larger.
const float kMaxStreamsMultiplier = 1.1f;
const int kMaxStreamsMinimumIncrement = 10;

// Available streams are ones with IDs less than the highest stream that has
// been opened which have neither been opened or reset. The limit on the number
// of available streams is 10 times the limit on the number of open streams.
const int kMaxAvailableStreamsMultiplier = 10;

// Track the number of promises that are not yet claimed by a
// corresponding get.  This must be smaller than
// kMaxAvailableStreamsMultiplier, because RST on a promised stream my
// create available streams entries.
const int kMaxPromisedStreamsMultiplier = kMaxAvailableStreamsMultiplier - 1;

// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int64_t kMinRetransmissionTimeMs = 200;
// The delayed ack time must not be greater than half the min RTO.
static_assert(kDefaultDelayedAckTimeMs <= kMinRetransmissionTimeMs / 2,
              "Delayed ack time must be less than or equal half the MinRTO");

// We define an unsigned 16-bit floating point value, inspired by IEEE floats
// (http://en.wikipedia.org/wiki/Half_precision_floating-point_format),
// with 5-bit exponent (bias 1), 11-bit mantissa (effective 12 with hidden
// bit) and denormals, but without signs, transfinites or fractions. Wire format
// 16 bits (little-endian byte order) are split into exponent (high 5) and
// mantissa (low 11) and decoded as:
//   uint64_t value;
//   if (exponent == 0) value = mantissa;
//   else value = (mantissa | 1 << 11) << (exponent - 1)
const int kUFloat16ExponentBits = 5;
const int kUFloat16MaxExponent = (1 << kUFloat16ExponentBits) - 2;     // 30
const int kUFloat16MantissaBits = 16 - kUFloat16ExponentBits;          // 11
const int kUFloat16MantissaEffectiveBits = kUFloat16MantissaBits + 1;  // 12
const uint64_t kUFloat16MaxValue =  // 0x3FFC0000000
    ((UINT64_C(1) << kUFloat16MantissaEffectiveBits) - 1)
    << kUFloat16MaxExponent;

// kDiversificationNonceSize is the size, in bytes, of the nonce that a server
// may set in the packet header to ensure that its INITIAL keys are not
// duplicated.
const size_t kDiversificationNonceSize = 32;

// The largest gap in packets we'll accept without closing the connection.
// This will likely have to be tuned.
const QuicPacketNumber kMaxPacketGap = 5000;

// The maximum number of random padding bytes to add.
const QuicByteCount kMaxNumRandomPaddingBytes = 256;

// The size of stream send buffer data slice size in bytes. A data slice is
// piece of stream data stored in contiguous memory, and a stream frame can
// contain data from multiple data slices.
const QuicByteCount kQuicStreamSendBufferSliceSize = 4 * 1024;

// For When using Random Initial Packet Numbers, they can start
// anyplace in the range 1...((2^31)-1) or 0x7fffffff
const QuicPacketNumber kMaxRandomInitialPacketNumber = 0x7fffffff;

// Used to represent an invalid or no control frame id.
const QuicControlFrameId kInvalidControlFrameId = 0;

// The max length a stream can have.
const QuicByteCount kMaxStreamLength = (UINT64_C(1) << 62) - 1;

// The max value that can be encoded using IETF Var Ints.
const uint64_t kMaxIetfVarInt = UINT64_C(0x3fffffffffffffff);

// The maximum stream id value that is supported - (2^32)-1
const QuicStreamId kMaxQuicStreamId = 0xffffffff;

// Number of bytes reserved for packet header type.
const size_t kPacketHeaderTypeSize = 1;

// Number of bytes reserved for connection ID length.
const size_t kConnectionIdLengthSize = 1;

// Length of an encoded variable length connection ID, in bytes.
const size_t kQuicConnectionIdLength = 8;

// Minimum length of random bytes in IETF stateless reset packet.
const size_t kMinRandomBytesLengthInStatelessReset = 20;

// Maximum length allowed for the token in a NEW_TOKEN frame.
const size_t kMaxNewTokenTokenLength = 0xffff;

// Used to represent an invalid packet number.
const QuicPacketNumber kInvalidPacketNumber = 0;

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_CONSTANTS_H_
