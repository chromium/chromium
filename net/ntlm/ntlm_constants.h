// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NTLM_NTLM_CONSTANTS_H_
#define NET_NTLM_NTLM_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include <vector>

#include "net/base/net_export.h"

namespace net::ntlm {

// A security buffer is a structure within an NTLM message that indicates
// the offset from the beginning of the message and the length of a payload
// that occurs later in the message. Within the raw message there is also
// an additional field, however the field is always written with the same
// value as length, and readers must always ignore it.
struct SecurityBuffer {
  SecurityBuffer(uint32_t offset, uint16_t length)
      : offset(offset), length(length) {}
  SecurityBuffer() : SecurityBuffer(0, 0) {}

  uint32_t offset;
  uint16_t length;
};

struct NtlmFeatures {
  explicit NtlmFeatures(bool enable_NTLMv2) : enable_NTLMv2(enable_NTLMv2) {}

  // Whether the use NTLMv2.
  bool enable_NTLMv2 = true;

  // Enables Message Integrity Check (MIC). This flag is ignored if
  // enable_NTLMv2 is false.
  bool enable_MIC = true;

  // Enables Extended Protection for Authentication (EPA). This flag is
  // ignored if enable_NTLMv2 is false.
  bool enable_EPA = true;
};

// There are 3 types of messages in NTLM. The message type is a field in
// every NTLM message header. See [MS-NLMP] Section 2.2.
enum class MessageType : uint32_t {
  kNegotiate = 0x01,
  kChallenge = 0x02,
  kAuthenticate = 0x03,
};

// Defined in [MS-NLMP] Section 2.2.2.5
// Only the used subset is defined.
enum class NegotiateFlags : uint32_t {
  kNone = 0,
  kUnicode = 0x01,
  kOem = 0x02,
  kRequestTarget = 0x04,
  kNtlm = 0x200,
  kAlwaysSign = 0x8000,
  kExtendedSessionSecurity = 0x80000,
  kTargetInfo = 0x800000,
};

constexpr NegotiateFlags operator|(NegotiateFlags lhs, NegotiateFlags rhs) {
  using TFlagsInt = std::underlying_type<NegotiateFlags>::type;

  return static_cast<NegotiateFlags>(static_cast<TFlagsInt>(lhs) |
                                     static_cast<TFlagsInt>(rhs));
}

constexpr NegotiateFlags operator&(NegotiateFlags lhs, NegotiateFlags rhs) {
  using TFlagsInt = std::underlying_type<NegotiateFlags>::type;

  return static_cast<NegotiateFlags>(static_cast<TFlagsInt>(lhs) &
                                     static_cast<TFlagsInt>(rhs));
}

// Identifies the payload type in an AV Pair. See [MS-NLMP] 2.2.2.1
enum class TargetInfoAvId : uint16_t {
  kEol = 0x0000,
  kServerName = 0x00001,
  kDomainName = 0x00002,
  kFlags = 0x0006,
  kTimestamp = 0x0007,
  kTargetName = 0x0009,
  kChannelBindings = 0x000A,
};

// Flags used in an TargetInfoAvId::kFlags AV Pair. See [MS-NLMP] 2.2.2.1
enum class TargetInfoAvFlags : uint32_t {
  kNone = 0,
  kMicPresent = 0x00000002,
};

using TAvFlagsInt = std::underlying_type<TargetInfoAvFlags>::type;

constexpr TargetInfoAvFlags operator|(TargetInfoAvFlags lhs,
                                      TargetInfoAvFlags rhs) {
  return static_cast<TargetInfoAvFlags>(static_cast<TAvFlagsInt>(lhs) |
                                        static_cast<TAvFlagsInt>(rhs));
}

constexpr TargetInfoAvFlags operator&(TargetInfoAvFlags lhs,
                                      TargetInfoAvFlags rhs) {
  return static_cast<TargetInfoAvFlags>(static_cast<TAvFlagsInt>(lhs) &
                                        static_cast<TAvFlagsInt>(rhs));
}

// An AV Pair is a structure that appears inside the target info field. It
// consists of an |avid| to identify the data type and an |avlen| specifying
// the size of the payload. Following that is |avlen| bytes of inline payload.
// AV Pairs are concatenated together and a special terminator with |avid|
// equal to |kEol| and |avlen| equal to zero signals that no further pairs
// follow. See [MS-NLMP] 2.2.2.1
//
// AV Pairs from the Challenge message are read from the challenge message
// and a potentially modified version is written into the authenticate
// message. In some cases the existing AV Pair is modified, eg. flags. In
// some cases new AV Pairs are add, eg. channel bindings and spn.
//
// For simplicity of processing two special fields |flags|, and |timestamp|
// are populated during the initial parsing phase for AVIDs |kFlags| and
// |kTimestamp| respectively. This avoids subsequent code having to
// manipulate the payload value through the buffer directly. For all
// other AvPairs the value of these 2 fields is undefined and the payload
// is in the |buffer| field. For these fields the payload is copied verbatim
// and it's content is not read or validated in any way.
struct NET_EXPORT_PRIVATE AvPair {
  AvPair();
  AvPair(TargetInfoAvId avid, uint16_t avlen);
  AvPair(TargetInfoAvId avid, std::vector<uint8_t> buffer);
  AvPair(const AvPair& other);
  AvPair(AvPair&& other);
  ~AvPair();

  AvPair& operator=(const AvPair& other);
  AvPair& operator=(AvPair&& other);

  std::vector<uint8_t> buffer;
  uint64_t timestamp;
  TargetInfoAvFlags flags;
  TargetInfoAvId avid;
  uint16_t avlen;
};

static constexpr uint8_t kSignature[] = "NTLMSSP";
static constexpr size_t kSignatureLen = std::size(kSignature);
static constexpr uint16_t kProofInputVersionV2 = 0x0101;
static constexpr size_t kSecurityBufferLen =
    (2 * sizeof(uint16_t)) + sizeof(uint32_t);
static constexpr size_t kNegotiateMessageLen = 32;
static constexpr size_t kMinChallengeHeaderLen = 32;
static constexpr size_t kChallengeHeaderLen = 48;
static constexpr size_t kResponseLenV1 = 24;
static constexpr size_t kChallengeLen = 8;
static constexpr size_t kVersionFieldLen = 8;
static constexpr size_t kNtlmHashLen = 16;
static constexpr size_t kNtlmProofLenV2 = kNtlmHashLen;
static constexpr size_t kSessionKeyLenV2 = kNtlmHashLen;
static constexpr size_t kMicLenV2 = kNtlmHashLen;
static constexpr size_t kChannelBindingsHashLen = kNtlmHashLen;
static constexpr size_t kEpaUnhashedStructHeaderLen = 20;
static constexpr size_t kProofInputLenV2 = 28;
static constexpr size_t kAvPairHeaderLen = 2 * sizeof(uint16_t);
static constexpr size_t kNtlmResponseHeaderLenV2 =
    kNtlmProofLenV2 + kProofInputLenV2;
static constexpr size_t kAuthenticateHeaderLenV1 = 64;
static constexpr size_t kMicOffsetV2 = 72;
static constexpr size_t kAuthenticateHeaderLenV2 = 88;

static constexpr size_t kMaxFqdnLen = 255;
static constexpr size_t kMaxUsernameLen = 104;
static constexpr size_t kMaxPasswordLen = 256;

static constexpr NegotiateFlags kNegotiateMessageFlags =
    NegotiateFlags::kUnicode | NegotiateFlags::kOem |
    NegotiateFlags::kRequestTarget | NegotiateFlags::kNtlm |
    NegotiateFlags::kAlwaysSign | NegotiateFlags::kExtendedSessionSecurity;

}  // namespace net::ntlm

#endif  // NET_NTLM_NTLM_CONSTANTS_H_
