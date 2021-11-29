// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PARSE_NAME_H_
#define NET_CERT_INTERNAL_PARSE_NAME_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

NET_EXPORT der::Input TypeCommonNameOid();
NET_EXPORT der::Input TypeSurnameOid();
NET_EXPORT der::Input TypeSerialNumberOid();
NET_EXPORT der::Input TypeCountryNameOid();
NET_EXPORT der::Input TypeLocalityNameOid();
NET_EXPORT der::Input TypeStateOrProvinceNameOid();
NET_EXPORT der::Input TypeStreetAddressOid();
NET_EXPORT der::Input TypeOrganizationNameOid();
NET_EXPORT der::Input TypeOrganizationUnitNameOid();
NET_EXPORT der::Input TypeTitleOid();
NET_EXPORT der::Input TypeNameOid();
NET_EXPORT der::Input TypeGivenNameOid();
NET_EXPORT der::Input TypeInitialsOid();
NET_EXPORT der::Input TypeGenerationQualifierOid();
NET_EXPORT der::Input TypeDomainComponentOid();
NET_EXPORT der::Input TypeEmailAddressOid();

// X509NameAttribute contains a representation of a DER-encoded RFC 2253
// "AttributeTypeAndValue".
//
// AttributeTypeAndValue ::= SEQUENCE {
//     type  AttributeType,
//     value AttributeValue
// }
struct NET_EXPORT X509NameAttribute {
  X509NameAttribute(der::Input in_type,
                    der::Tag in_value_tag,
                    der::Input in_value)
      : type(in_type), value_tag(in_value_tag), value(in_value) {}

  // Configures handling of PrintableString in the attribute value. Do
  // not use non-default handling without consulting //net owners. With
  // kAsUTF8Hack, PrintableStrings are interpreted as UTF-8 strings.
  enum class PrintableStringHandling { kDefault, kAsUTF8Hack };

  // Attempts to convert the value represented by this struct into a
  // UTF-8 string and store it in |out|, returning whether the conversion
  // was successful.
  bool ValueAsString(std::string* out) const WARN_UNUSED_RESULT;

  // Attempts to convert the value represented by this struct into a
  // UTF-8 string and store it in |out|, returning whether the conversion
  // was successful. Allows configuring some non-standard string handling
  // options.
  //
  // Do not use without consulting //net owners.
  bool ValueAsStringWithUnsafeOptions(
      PrintableStringHandling printable_string_handling,
      std::string* out) const WARN_UNUSED_RESULT;

  // Attempts to convert the value represented by this struct into a
  // std::string and store it in |out|, returning whether the conversion was
  // successful. Due to some encodings being incompatible, the caller must
  // verify the attribute |value_tag|.
  //
  // Note: Don't use this function unless you know what you're doing. Use
  // ValueAsString instead.
  //
  // Note: The conversion doesn't verify that the value corresponds to the
  // ASN.1 definition of the value type.
  bool ValueAsStringUnsafe(std::string* out) const WARN_UNUSED_RESULT;

  // Formats the NameAttribute per RFC2253 into an ASCII string and stores
  // the result in |out|, returning whether the conversion was successful.
  bool AsRFC2253String(std::string* out) const WARN_UNUSED_RESULT;

  der::Input type;
  der::Tag value_tag;
  der::Input value;
};

typedef std::vector<X509NameAttribute> RelativeDistinguishedName;
typedef std::vector<RelativeDistinguishedName> RDNSequence;

// Parses all the ASN.1 AttributeTypeAndValue elements in |parser| and stores
// each as an AttributeTypeAndValue object in |out|.
//
// AttributeTypeAndValue is defined in RFC 5280 section 4.1.2.4:
//
// AttributeTypeAndValue ::= SEQUENCE {
//   type     AttributeType,
//   value    AttributeValue }
//
// AttributeType ::= OBJECT IDENTIFIER
//
// AttributeValue ::= ANY -- DEFINED BY AttributeType
//
// DirectoryString ::= CHOICE {
//       teletexString           TeletexString (SIZE (1..MAX)),
//       printableString         PrintableString (SIZE (1..MAX)),
//       universalString         UniversalString (SIZE (1..MAX)),
//       utf8String              UTF8String (SIZE (1..MAX)),
//       bmpString               BMPString (SIZE (1..MAX)) }
//
// The type of the component AttributeValue is determined by the AttributeType;
// in general it will be a DirectoryString.
NET_EXPORT bool ReadRdn(der::Parser* parser,
                        RelativeDistinguishedName* out) WARN_UNUSED_RESULT;

// Parses a DER-encoded "Name" as specified by 5280. Returns true on success
// and sets the results in |out|.
NET_EXPORT bool ParseName(const der::Input& name_tlv,
                          RDNSequence* out) WARN_UNUSED_RESULT;
// Parses a DER-encoded "Name" value (without the sequence tag & length) as
// specified by 5280. Returns true on success and sets the results in |out|.
NET_EXPORT bool ParseNameValue(const der::Input& name_value,
                               RDNSequence* out) WARN_UNUSED_RESULT;

// Formats a RDNSequence |rdn_sequence| per RFC2253 as an ASCII string and
// stores the result into |out|, and returns whether the conversion was
// successful.
NET_EXPORT bool ConvertToRFC2253(const RDNSequence& rdn_sequence,
                                 std::string* out) WARN_UNUSED_RESULT;
}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_NAME_H_
