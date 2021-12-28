// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_name.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "net/der/parse_values.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net {

namespace {

// Returns a string containing the dotted numeric form of |oid|, or an empty
// string on error.
std::string OidToString(der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.UnsafeData(), oid.Length());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text)
    return std::string();
  return text.get();
}

}  // namespace

der::Input TypeCommonNameOid() {
  // id-at-commonName: 2.5.4.3 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x03};
  return der::Input(oid);
}

der::Input TypeSurnameOid() {
  // id-at-surname: 2.5.4.4 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x04};
  return der::Input(oid);
}

der::Input TypeSerialNumberOid() {
  // id-at-serialNumber: 2.5.4.5 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x05};
  return der::Input(oid);
}

der::Input TypeCountryNameOid() {
  // id-at-countryName: 2.5.4.6 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x06};
  return der::Input(oid);
}

der::Input TypeLocalityNameOid() {
  // id-at-localityName: 2.5.4.7 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x07};
  return der::Input(oid);
}

der::Input TypeStateOrProvinceNameOid() {
  // id-at-stateOrProvinceName: 2.5.4.8 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x08};
  return der::Input(oid);
}

der::Input TypeStreetAddressOid() {
  // street (streetAddress): 2.5.4.9 (RFC 4519)
  static const uint8_t oid[] = {0x55, 0x04, 0x09};
  return der::Input(oid);
}

der::Input TypeOrganizationNameOid() {
  // id-at-organizationName: 2.5.4.10 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0a};
  return der::Input(oid);
}

der::Input TypeOrganizationUnitNameOid() {
  // id-at-organizationalUnitName: 2.5.4.11 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0b};
  return der::Input(oid);
}

der::Input TypeTitleOid() {
  // id-at-title: 2.5.4.12 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x0c};
  return der::Input(oid);
}

der::Input TypeNameOid() {
  // id-at-name: 2.5.4.41 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x29};
  return der::Input(oid);
}

der::Input TypeGivenNameOid() {
  // id-at-givenName: 2.5.4.42 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2a};
  return der::Input(oid);
}

der::Input TypeInitialsOid() {
  // id-at-initials: 2.5.4.43 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2b};
  return der::Input(oid);
}

der::Input TypeGenerationQualifierOid() {
  // id-at-generationQualifier: 2.5.4.44 (RFC 5280)
  static const uint8_t oid[] = {0x55, 0x04, 0x2c};
  return der::Input(oid);
}

der::Input TypeDomainComponentOid() {
  // dc (domainComponent): 0.9.2342.19200300.100.1.25 (RFC 4519)
  static const uint8_t oid[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                                0xF2, 0x2C, 0x64, 0x01, 0x19};
  return der::Input(oid);
}

der::Input TypeEmailAddressOid() {
  // RFC 5280 section A.1:
  //
  // pkcs-9 OBJECT IDENTIFIER ::=
  //   { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 9 }
  //
  // id-emailAddress      AttributeType ::= { pkcs-9 1 }
  //
  // In dotted form: 1.2.840.113549.1.9.1
  const uint8_t oid[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x01};
  return der::Input(oid);
}

bool X509NameAttribute::ValueAsString(std::string* out) const {
  switch (value_tag) {
    case der::kTeletexString:
      return der::ParseTeletexStringAsLatin1(value, out);
    case der::kIA5String:
      return der::ParseIA5String(value, out);
    case der::kPrintableString:
      return der::ParsePrintableString(value, out);
    case der::kUtf8String:
      *out = value.AsString();
      return true;
    case der::kUniversalString:
      return der::ParseUniversalString(value, out);
    case der::kBmpString:
      return der::ParseBmpString(value, out);
    default:
      return false;
  }
}

bool X509NameAttribute::ValueAsStringWithUnsafeOptions(
    PrintableStringHandling printable_string_handling,
    std::string* out) const {
  if (printable_string_handling == PrintableStringHandling::kAsUTF8Hack &&
      value_tag == der::kPrintableString) {
    *out = value.AsString();
    return true;
  }
  return ValueAsString(out);
}

bool X509NameAttribute::ValueAsStringUnsafe(std::string* out) const {
  switch (value_tag) {
    case der::kIA5String:
    case der::kPrintableString:
    case der::kTeletexString:
    case der::kUtf8String:
      *out = value.AsString();
      return true;
    case der::kUniversalString:
      return der::ParseUniversalString(value, out);
    case der::kBmpString:
      return der::ParseBmpString(value, out);
    default:
      NOTREACHED();
      return false;
  }
}

bool X509NameAttribute::AsRFC2253String(std::string* out) const {
  std::string type_string;
  std::string value_string;
  // TODO(mattm): Add streetAddress and domainComponent here?
  if (type == TypeCommonNameOid()) {
    type_string = "CN";
  } else if (type == TypeSurnameOid()) {
    type_string = "SN";
  } else if (type == TypeCountryNameOid()) {
    type_string = "C";
  } else if (type == TypeLocalityNameOid()) {
    type_string = "L";
  } else if (type == TypeStateOrProvinceNameOid()) {
    type_string = "ST";
  } else if (type == TypeOrganizationNameOid()) {
    type_string = "O";
  } else if (type == TypeOrganizationUnitNameOid()) {
    type_string = "OU";
  } else if (type == TypeGivenNameOid()) {
    type_string = "givenName";
  } else if (type == TypeEmailAddressOid()) {
    type_string = "emailAddress";
  } else {
    type_string = OidToString(type);
    if (type_string.empty())
      return false;
    value_string = "#" + base::HexEncode(value.UnsafeData(), value.Length());
  }

  if (value_string.empty()) {
    std::string unescaped;
    if (!ValueAsStringUnsafe(&unescaped))
      return false;

    bool nonprintable = false;
    for (unsigned int i = 0; i < unescaped.length(); ++i) {
      unsigned char c = static_cast<unsigned char>(unescaped[i]);
      if (i == 0 && c == '#') {
        value_string += "\\#";
      } else if (i == 0 && c == ' ') {
        value_string += "\\ ";
      } else if (i == unescaped.length() - 1 && c == ' ') {
        value_string += "\\ ";
      } else if (c == ',' || c == '+' || c == '"' || c == '\\' || c == '<' ||
                 c == '>' || c == ';') {
        value_string += "\\";
        value_string += c;
      } else if (c < 32 || c > 126) {
        nonprintable = true;
        std::string h;
        h += c;
        value_string += "\\" + base::HexEncode(h.data(), h.length());
      } else {
        value_string += c;
      }
    }

    // If we have non-printable characters in a TeletexString, we hex encode
    // since we don't handle Teletex control codes.
    if (nonprintable && value_tag == der::kTeletexString)
      value_string = "#" + base::HexEncode(value.UnsafeData(), value.Length());
  }

  *out = type_string + "=" + value_string;
  return true;
}

bool ReadRdn(der::Parser* parser, RelativeDistinguishedName* out) {
  while (parser->HasMore()) {
    der::Parser attr_type_and_value;
    if (!parser->ReadSequence(&attr_type_and_value))
      return false;
    // Read the attribute type, which must be an OBJECT IDENTIFIER.
    der::Input type;
    if (!attr_type_and_value.ReadTag(der::kOid, &type))
      return false;

    // Read the attribute value.
    der::Tag tag;
    der::Input value;
    if (!attr_type_and_value.ReadTagAndValue(&tag, &value))
      return false;

    // There should be no more elements in the sequence after reading the
    // attribute type and value.
    if (attr_type_and_value.HasMore())
      return false;

    out->push_back(X509NameAttribute(type, tag, value));
  }

  // RFC 5280 section 4.1.2.4
  // RelativeDistinguishedName ::= SET SIZE (1..MAX) OF AttributeTypeAndValue
  return out->size() != 0;
}

bool ParseName(const der::Input& name_tlv, RDNSequence* out) {
  der::Parser name_parser(name_tlv);
  der::Input name_value;
  if (!name_parser.ReadTag(der::kSequence, &name_value))
    return false;
  return ParseNameValue(name_value, out);
}

bool ParseNameValue(const der::Input& name_value, RDNSequence* out) {
  der::Parser rdn_sequence_parser(name_value);
  while (rdn_sequence_parser.HasMore()) {
    der::Parser rdn_parser;
    if (!rdn_sequence_parser.ReadConstructed(der::kSet, &rdn_parser))
      return false;
    RelativeDistinguishedName type_and_values;
    if (!ReadRdn(&rdn_parser, &type_and_values))
      return false;
    out->push_back(type_and_values);
  }

  return true;
}

bool ConvertToRFC2253(const RDNSequence& rdn_sequence, std::string* out) {
  std::string rdns_string;
  size_t size = rdn_sequence.size();
  for (size_t i = 0; i < size; ++i) {
    RelativeDistinguishedName rdn = rdn_sequence[size - i - 1];
    std::string rdn_string;
    for (const auto& atv : rdn) {
      if (!rdn_string.empty())
        rdn_string += "+";
      std::string atv_string;
      if (!atv.AsRFC2253String(&atv_string))
        return false;
      rdn_string += atv_string;
    }
    if (!rdns_string.empty())
      rdns_string += ",";
    rdns_string += rdn_string;
  }

  *out = rdns_string;
  return true;
}

}  // namespace net
