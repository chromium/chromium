// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definitions and utility functions related to handling of QUIC versions.
//
// QUIC version is a four-byte tag that can be represented in memory as a
// QuicVersionLabel type (which is an alias to uint32_t).  In actuality, all
// versions supported by this implementation have the following format:
//   [QT]0\d\d
// e.g. Q046.  Q or T distinguishes the type of handshake used (Q for QUIC
// Crypto handshake, T for TLS-based handshake), and the two digits at the end
// is the actual numeric value of transport version used by the code.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_VERSIONS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_VERSIONS_H_

#include <vector>

#include "net/third_party/quic/core/quic_tag.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

// The available versions of QUIC.  The numeric value of the enum is guaranteed
// to match the number in the name.  The versions not currently supported are
// documented in comments.
//
// See go/new-quic-version for more details on how to roll out new versions.
enum QuicTransportVersion {
  // Special case to indicate unknown/unsupported QUIC version.
  QUIC_VERSION_UNSUPPORTED = 0,

  // Version 1 was the first version of QUIC that supported versioning.
  // Version 2 decoupled versioning of non-cryptographic parameters from the
  //           SCFG.
  // Version 3 moved public flags into the beginning of the packet.
  // Version 4 added support for variable-length connection IDs.
  // Version 5 made specifying FEC groups optional.
  // Version 6 introduced variable-length packet numbers.
  // Version 7 introduced a lower-overhead encoding for stream frames.
  // Version 8 made salt length equal to digest length for the RSA-PSS
  //           signatures.
  // Version 9 added stream priority.
  // Version 10 redid the frame type numbering.
  // Version 11 reduced the length of null encryption authentication tag
  //            from 16 to 12 bytes.
  // Version 12 made the sequence numbers in the ACK frames variable-sized.
  // Version 13 added the dedicated header stream.
  // Version 14 added byte_offset to RST_STREAM frame.
  // Version 15 added a list of packets recovered using FEC to the ACK frame.
  // Version 16 added STOP_WAITING frame.
  // Version 17 added per-stream flow control.
  // Version 18 added PING frame.
  // Version 19 added connection-level flow control
  // Version 20 allowed to set stream- and connection-level flow control windows
  //            to different values.
  // Version 21 made header and crypto streams flow-controlled.
  // Version 22 added support for SCUP (server config update) messages.
  // Version 23 added timestamps into the ACK frame.
  // Version 24 added SPDY/4 header compression.
  // Version 25 added support for SPDY/4 header keys and removed error_details
  //            from RST_STREAM frame.
  // Version 26 added XLCT (expected leaf certificate) tag into CHLO.
  // Version 27 added a nonce into SHLO.
  // Version 28 allowed receiver to refuse creating a requested stream.
  // Version 29 added support for QUIC_STREAM_NO_ERROR.
  // Version 30 added server-side support for certificate transparency.
  // Version 31 incorporated the hash of CHLO into the crypto proof supplied by
  //            the server.
  // Version 32 removed FEC-related fields from wire format.
  // Version 33 added diversification nonces.
  // Version 34 removed entropy bits from packets and ACK frames, removed
  //            private flag from packet header and changed the ACK format to
  //            specify ranges of packets acknowledged rather than missing
  //            ranges.

  QUIC_VERSION_35 = 35,  // Allows endpoints to independently set stream limit.

  // Version 36 added support for forced head-of-line blocking experiments.
  // Version 37 added perspective into null encryption.
  // Version 38 switched to IETF padding frame format and support for NSTP (no
  //            stop waiting frame) connection option.

  QUIC_VERSION_39 = 39,  // Integers and floating numbers are written in big
                         // endian. Dot not ack acks. Send a connection level
                         // WINDOW_UPDATE every 20 sent packets which do not
                         // contain retransmittable frames.

  // Version 40 was an attempt to convert QUIC to IETF frame format; it was
  //            never shipped due to a bug.
  // Version 41 was a bugfix for version 40.  The working group changed the wire
  //            format before it shipped, which caused it to be never shipped
  //            and all the changes from it to be reverted.  No changes from v40
  //            or v41 are present in subsequent versions.
  // Version 42 allowed receiving overlapping stream data.

  QUIC_VERSION_43 = 43,  // PRIORITY frames are sent by client and accepted by
                         // server.
  QUIC_VERSION_44 = 44,  // Use IETF header format.
  QUIC_VERSION_45 = 45,  // Added MESSAGE frame.
  QUIC_VERSION_46 = 46,  // Use CRYPTO frames for QuicCryptoStreams.
  QUIC_VERSION_99 = 99,  // Dumping ground for IETF QUIC changes which are not
                         // yet ready for production.
};

// The crypto handshake protocols that can be used with QUIC.
enum HandshakeProtocol {
  PROTOCOL_UNSUPPORTED,
  PROTOCOL_QUIC_CRYPTO,
  PROTOCOL_TLS1_3,
};

// A parsed QUIC version label which determines that handshake protocol
// and the transport version.
struct QUIC_EXPORT_PRIVATE ParsedQuicVersion {
  HandshakeProtocol handshake_protocol;
  QuicTransportVersion transport_version;

  ParsedQuicVersion(HandshakeProtocol handshake_protocol,
                    QuicTransportVersion transport_version);

  ParsedQuicVersion(const ParsedQuicVersion& other)
      : handshake_protocol(other.handshake_protocol),
        transport_version(other.transport_version) {}

  ParsedQuicVersion& operator=(const ParsedQuicVersion& other) {
    if (this != &other) {
      handshake_protocol = other.handshake_protocol;
      transport_version = other.transport_version;
    }
    return *this;
  }

  bool operator==(const ParsedQuicVersion& other) const {
    return handshake_protocol == other.handshake_protocol &&
           transport_version == other.transport_version;
  }

  bool operator!=(const ParsedQuicVersion& other) const {
    return handshake_protocol != other.handshake_protocol ||
           transport_version != other.transport_version;
  }
};

QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                             const ParsedQuicVersion& version);

using ParsedQuicVersionVector = std::vector<ParsedQuicVersion>;

// Representation of the on-the-wire QUIC version number. Will be written/read
// to the wire in network-byte-order.
using QuicVersionLabel = uint32_t;
using QuicVersionLabelVector = std::vector<QuicVersionLabel>;

// This vector contains QUIC versions which we currently support.
// This should be ordered such that the highest supported version is the first
// element, with subsequent elements in descending order (versions can be
// skipped as necessary).
//
// See go/new-quic-version for more details on how to roll out new versions.
static const QuicTransportVersion kSupportedTransportVersions[] = {
    QUIC_VERSION_99, QUIC_VERSION_46, QUIC_VERSION_45, QUIC_VERSION_44,
    QUIC_VERSION_43, QUIC_VERSION_39, QUIC_VERSION_35};

// This vector contains all crypto handshake protocols that are supported.
static const HandshakeProtocol kSupportedHandshakeProtocols[] = {
    PROTOCOL_QUIC_CRYPTO, PROTOCOL_TLS1_3};

typedef std::vector<QuicTransportVersion> QuicTransportVersionVector;

// Returns a vector of QUIC versions in kSupportedTransportVersions.
QUIC_EXPORT_PRIVATE QuicTransportVersionVector AllSupportedTransportVersions();

// Returns a vector of QUIC versions that is the cartesian product of
// kSupportedTransportVersions and kSupportedHandshakeProtocols.
QUIC_EXPORT_PRIVATE ParsedQuicVersionVector AllSupportedVersions();

// Returns a vector of QUIC versions from kSupportedTransportVersions which
// exclude any versions which are disabled by flags.
QUIC_EXPORT_PRIVATE QuicTransportVersionVector
CurrentSupportedTransportVersions();

// Returns a vector of QUIC versions that is the cartesian product of
// kSupportedTransportVersions and kSupportedHandshakeProtocols, with any
// versions disabled by flags excluded.
QUIC_EXPORT_PRIVATE ParsedQuicVersionVector CurrentSupportedVersions();

// Returns a vector of QUIC versions from |versions| which exclude any versions
// which are disabled by flags.
QUIC_EXPORT_PRIVATE QuicTransportVersionVector
FilterSupportedTransportVersions(QuicTransportVersionVector versions);

// Returns a vector of QUIC versions from |versions| which exclude any versions
// which are disabled by flags.
QUIC_EXPORT_PRIVATE ParsedQuicVersionVector
FilterSupportedVersions(ParsedQuicVersionVector versions);

// Returns QUIC version of |index| in result of |versions|. Returns
// QUIC_VERSION_UNSUPPORTED if |index| is out of bounds.
QUIC_EXPORT_PRIVATE QuicTransportVersionVector
VersionOfIndex(const QuicTransportVersionVector& versions, int index);

// Returns QUIC version of |index| in result of |versions|. Returns
// ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED) if |index|
// is out of bounds.
QUIC_EXPORT_PRIVATE ParsedQuicVersionVector
ParsedVersionOfIndex(const ParsedQuicVersionVector& versions, int index);

// Returns a vector of QuicTransportVersions corresponding to just the transport
// versions in |versions|. If the input vector contains multiple parsed versions
// with different handshake protocols (but the same transport version), that
// transport version will appear in the resulting vector multiple times.
QUIC_EXPORT_PRIVATE QuicTransportVersionVector
ParsedVersionsToTransportVersions(const ParsedQuicVersionVector& versions);

// QuicVersionLabel is written to and read from the wire, but we prefer to use
// the more readable ParsedQuicVersion at other levels.
// Helper function which translates from a QuicVersionLabel to a
// ParsedQuicVersion.
QUIC_EXPORT_PRIVATE ParsedQuicVersion
ParseQuicVersionLabel(QuicVersionLabel version_label);

// Constructs a QuicVersionLabel from the provided ParsedQuicVersion.
QUIC_EXPORT_PRIVATE QuicVersionLabel
CreateQuicVersionLabel(ParsedQuicVersion parsed_version);

// Constructs a QuicVersionLabelVector from the provided
// ParsedQuicVersionVector.
QUIC_EXPORT_PRIVATE QuicVersionLabelVector
CreateQuicVersionLabelVector(const ParsedQuicVersionVector& versions);

// QuicVersionLabel is written to and read from the wire, but we prefer to use
// the more readable QuicTransportVersion at other levels.
// Helper function which translates from a QuicTransportVersion to a
// QuicVersionLabel. Returns 0 if |version| is unsupported.
QUIC_EXPORT_PRIVATE QuicVersionLabel
QuicVersionToQuicVersionLabel(QuicTransportVersion transport_version);

// Helper function which translates from a QuicVersionLabel to a string.
QUIC_EXPORT_PRIVATE QuicString
QuicVersionLabelToString(QuicVersionLabel version_label);

// Returns |separator|-separated list of string representations of
// QuicVersionLabel values in the supplied |version_labels| vector. The values
// after the (0-based) |skip_after_nth_version|'th are skipped.
QUIC_EXPORT_PRIVATE QuicString
QuicVersionLabelVectorToString(const QuicVersionLabelVector& version_labels,
                               const QuicString& separator,
                               size_t skip_after_nth_version);

// Returns comma separated list of string representations of QuicVersionLabel
// values in the supplied |version_labels| vector.
QUIC_EXPORT_PRIVATE inline QuicString QuicVersionLabelVectorToString(
    const QuicVersionLabelVector& version_labels) {
  return QuicVersionLabelVectorToString(version_labels, ",",
                                        std::numeric_limits<size_t>::max());
}

// Returns appropriate QuicTransportVersion from a QuicVersionLabel.
// Returns QUIC_VERSION_UNSUPPORTED if |version_label| cannot be understood.
QUIC_EXPORT_PRIVATE QuicTransportVersion
QuicVersionLabelToQuicVersion(QuicVersionLabel version_label);

// Returns the HandshakeProtocol used with the given |version_label|, returning
// PROTOCOL_UNSUPPORTED if it is unknown.
QUIC_EXPORT_PRIVATE HandshakeProtocol
QuicVersionLabelToHandshakeProtocol(QuicVersionLabel version_label);

// Helper function which translates from a QuicTransportVersion to a string.
// Returns strings corresponding to enum names (e.g. QUIC_VERSION_6).
QUIC_EXPORT_PRIVATE QuicString
QuicVersionToString(QuicTransportVersion transport_version);

// Helper function which translates from a ParsedQuicVersion to a string.
// Returns strings corresponding to the on-the-wire tag.
QUIC_EXPORT_PRIVATE QuicString
ParsedQuicVersionToString(ParsedQuicVersion version);

// Returns comma separated list of string representations of
// QuicTransportVersion enum values in the supplied |versions| vector.
QUIC_EXPORT_PRIVATE QuicString
QuicTransportVersionVectorToString(const QuicTransportVersionVector& versions);

// Returns comma separated list of string representations of ParsedQuicVersion
// values in the supplied |versions| vector.
QUIC_EXPORT_PRIVATE QuicString
ParsedQuicVersionVectorToString(const ParsedQuicVersionVector& versions);

// Returns |separator|-separated list of string representations of
// ParsedQuicVersion values in the supplied |versions| vector. The values after
// the (0-based) |skip_after_nth_version|'th are skipped.
QUIC_EXPORT_PRIVATE QuicString
ParsedQuicVersionVectorToString(const ParsedQuicVersionVector& versions,
                                const QuicString& separator,
                                size_t skip_after_nth_version);

// Returns comma separated list of string representations of ParsedQuicVersion
// values in the supplied |versions| vector.
QUIC_EXPORT_PRIVATE inline QuicString ParsedQuicVersionVectorToString(
    const ParsedQuicVersionVector& versions) {
  return ParsedQuicVersionVectorToString(versions, ",",
                                         std::numeric_limits<size_t>::max());
}

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_VERSIONS_H_
