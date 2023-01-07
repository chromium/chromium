// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_PROTOCOL_H_
#define NET_DNS_PUBLIC_DNS_PROTOCOL_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

// General constants and structs defined by the DNS and MDNS protocols.
//
// Direct interaction with DNS and MDNS, as well as parsing DNS and MDNS
// messages, should generally only be done within network stack code.
// Network-stack-external code should interact indirectly through network
// service APIs, e.g. NetworkContext::ResolveHost(). But these constants may
// still be useful for other minor purposes.
namespace dns_protocol {

static const uint16_t kDefaultPort = 53;
// RFC 5353.
static const uint16_t kDefaultPortMulticast = 5353;

// https://www.iana.org/assignments/multicast-addresses/multicast-addresses.xhtml#multicast-addresses-1
static const char kMdnsMulticastGroupIPv4[] = "224.0.0.251";
// https://www.iana.org/assignments/ipv6-multicast-addresses/ipv6-multicast-addresses.xhtml#link-local
static const char kMdnsMulticastGroupIPv6[] = "FF02::FB";

// DNS packet consists of a header followed by questions and/or answers.
// For the meaning of specific fields, please see RFC 1035 and 2535

// Header format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      ID                       |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |QR|   Opcode  |AA|TC|RD|RA| Z|AD|CD|   RCODE   |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    QDCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ANCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    NSCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ARCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Question format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                     QNAME                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QTYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QCLASS                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Answer format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                                               /
//  /                      NAME                     /
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     CLASS                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TTL                      |
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                   RDLENGTH                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
//  /                     RDATA                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

#pragma pack(push)
#pragma pack(1)

// On-the-wire header. All uint16_t are in network order.
struct NET_EXPORT Header {
  uint16_t id = 0;
  uint16_t flags = 0;
  uint16_t qdcount = 0;
  uint16_t ancount = 0;
  uint16_t nscount = 0;
  uint16_t arcount = 0;
};

#pragma pack(pop)

static const uint8_t kLabelMask = 0xc0;
static const uint8_t kLabelPointer = 0xc0;
static const uint8_t kLabelDirect = 0x0;
static const uint16_t kOffsetMask = 0x3fff;

// In MDns the most significant bit of the rrclass is designated as the
// "cache-flush bit", as described in http://www.rfc-editor.org/rfc/rfc6762.txt
// section 10.2.
static const uint16_t kMDnsClassMask = 0x7FFF;

// RFC 1035, section 3.1: To simplify implementations, the total length of
// a domain name in wire form (i.e., label octets and label length octets) is
// restricted to 255 octets or less.
//
// Note that RFC 1035 is ambiguous over whether or not this limit includes the
// final zero-length terminating label, but RFC 6762 unambiguously uses the
// more permissive interpretation of not including the terminating label against
// the limit for mDNS and argues in RFC 6762 Appendix C that that is the correct
// interpretation for unicast DNS. To avoid overcomplicating logic, Chrome
// universally uses the more permissive RFC 6762 interpretation for all parsing.
static const int kMaxNameLength = 255;

// The maximum number of ASCII characters allowed in a domain in dotted form,
// derived from `kMaxNameLength` above by subtracting one from the count to
// correspond to the first byte, which is not available to encode characters and
// does not correspond to a dot after conversion.
static const uint16_t kMaxCharNameLength = 254;

// RFC 1035, section 2.3.4: labels 63 octets or less.
// Section 3.1: Each label is represented as a one octet length field followed
// by that number of octets.
const int kMaxLabelLength = 63;

// RFC 1035, section 4.2.1: Messages carried by UDP are restricted to 512
// bytes (not counting the IP nor UDP headers).
static const int kMaxUDPSize = 512;

// RFC 6762, section 17: Messages over the local link are restricted by the
// medium's MTU, and must be under 9000 bytes
static const int kMaxMulticastSize = 9000;

// RFC 1035, Section 4.1.3.
// TYPE (2 bytes) + CLASS (2 bytes) + TTL (4 bytes) + RDLENGTH (2 bytes)
static const int kResourceRecordSizeInBytesWithoutNameAndRData = 10;

// DNS class types.
//
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-2
static const uint16_t kClassIN = 1;
// RFC 6762, Section 10.2.
//
// For resource records sent through mDNS, the top bit of the class field in a
// resource record is repurposed to the cache-flush bit. This bit should only be
// used in mDNS transactions.
static const uint16_t kFlagCacheFlush = 0x8000;

// DNS resource record types.
//
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-4
static const uint16_t kTypeA = 1;
static const uint16_t kTypeCNAME = 5;
static const uint16_t kTypeSOA = 6;
static const uint16_t kTypePTR = 12;
static const uint16_t kTypeTXT = 16;
static const uint16_t kTypeAAAA = 28;
static const uint16_t kTypeSRV = 33;
static const uint16_t kTypeOPT = 41;
static const uint16_t kTypeNSEC = 47;
static const uint16_t kTypeHttps = 65;
static const uint16_t kTypeANY = 255;

// DNS reply codes (RCODEs).
//
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-6
static const uint8_t kRcodeNOERROR = 0;
static const uint8_t kRcodeFORMERR = 1;
static const uint8_t kRcodeSERVFAIL = 2;
static const uint8_t kRcodeNXDOMAIN = 3;
static const uint8_t kRcodeNOTIMP = 4;
static const uint8_t kRcodeREFUSED = 5;

// DNS EDNS(0) option codes (OPT)
//
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-11
static constexpr uint16_t kEdnsPadding = 12;
static constexpr uint16_t kEdnsExtendedDnsError = 15;

// DNS header flags.
//
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-12
static const uint16_t kFlagResponse = 0x8000;
static const uint16_t kFlagAA = 0x400;  // Authoritative Answer - response flag.
static const uint16_t kFlagRD = 0x100;  // Recursion Desired - query flag.
static const uint16_t kFlagTC = 0x200;  // Truncated - server flag.

// SVCB/HTTPS ServiceParamKey
//
// IANA registration pending. Values from draft-ietf-dnsop-svcb-https-08.
static constexpr uint16_t kHttpsServiceParamKeyMandatory = 0;
static constexpr uint16_t kHttpsServiceParamKeyAlpn = 1;
static constexpr uint16_t kHttpsServiceParamKeyNoDefaultAlpn = 2;
static constexpr uint16_t kHttpsServiceParamKeyPort = 3;
static constexpr uint16_t kHttpsServiceParamKeyIpv4Hint = 4;
static constexpr uint16_t kHttpsServiceParamKeyEchConfig = 5;
static constexpr uint16_t kHttpsServiceParamKeyIpv6Hint = 6;

// draft-ietf-dnsop-svcb-https-08#section-9
inline constexpr char kHttpsServiceDefaultAlpn[] = "http/1.1";

}  // namespace dns_protocol

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_PROTOCOL_H_
