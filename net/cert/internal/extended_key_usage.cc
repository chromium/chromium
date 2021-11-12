// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/extended_key_usage.h"

#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

const der::Input AnyEKU() {
  // The arc for the anyExtendedKeyUsage OID is found under the id-ce arc,
  // defined in section 4.2.1 of RFC 5280:
  // id-ce   OBJECT IDENTIFIER ::=  { joint-iso-ccitt(2) ds(5) 29 }
  //
  // From RFC 5280 section 4.2.1.12:
  // id-ce-extKeyUsage OBJECT IDENTIFIER ::= { id-ce 37 }
  // anyExtendedKeyUsage OBJECT IDENTIFIER ::= { id-ce-extKeyUsage 0 }
  // In dotted notation: 2.5.29.37.0
  static const uint8_t any_eku[] = {0x55, 0x1d, 0x25, 0x00};
  return der::Input(any_eku);
}

const der::Input ServerAuth() {
  // All other key usage purposes defined in RFC 5280 are found in the id-kp
  // arc, defined in section 4.2.1.12 as:
  // id-kp OBJECT IDENTIFIER ::= { id-pkix 3 }
  //
  // With id-pkix defined in RFC 5280 section 4.2.2 as:
  // id-pkix  OBJECT IDENTIFIER  ::=
  //          { iso(1) identified-organization(3) dod(6) internet(1)
  //                  security(5) mechanisms(5) pkix(7) }
  //
  // From RFC 5280 section 4.2.1.12:
  // id-kp-serverAuth             OBJECT IDENTIFIER ::= { id-kp 1 }
  // In dotted notation: 1.3.6.1.5.5.7.3.1
  static const uint8_t server_auth[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
  return der::Input(server_auth);
}

// In dotted notation: 2.16.840.1.113730.4.1
const der::Input NetscapeServerGatedCrypto() {
  static const uint8_t data[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                 0xf8, 0x42, 0x04, 0x01};
  return der::Input(data);
}

const der::Input ClientAuth() {
  // From RFC 5280 section 4.2.1.12:
  // id-kp-clientAuth             OBJECT IDENTIFIER ::= { id-kp 2 }
  // In dotted notation: 1.3.6.1.5.5.7.3.2
  static const uint8_t client_auth[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};
  return der::Input(client_auth);
}

const der::Input CodeSigning() {
  // From RFC 5280 section 4.2.1.12:
  // id-kp-codeSigning             OBJECT IDENTIFIER ::= { id-kp 3 }
  // In dotted notation: 1.3.6.1.5.5.7.3.3
  static const uint8_t code_signing[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03};
  return der::Input(code_signing);
}

const der::Input EmailProtection() {
  // From RFC 5280 section 4.2.1.12:
  // id-kp-emailProtection         OBJECT IDENTIFIER ::= { id-kp 4 }
  // In dotted notation: 1.3.6.1.5.5.7.3.4
  static const uint8_t email_protection[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04};
  return der::Input(email_protection);
}

const der::Input TimeStamping() {
  // From RFC 5280 section 4.2.1.12:
  // id-kp-timeStamping            OBJECT IDENTIFIER ::= { id-kp 8 }
  // In dotted notation: 1.3.6.1.5.5.7.3.8
  static const uint8_t time_stamping[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x08};
  return der::Input(time_stamping);
}

const der::Input OCSPSigning() {
  // From RFC 5280 section 4.2.1.12:
  // id-kp-OCSPSigning            OBJECT IDENTIFIER ::= { id-kp 9 }
  // In dotted notation: 1.3.6.1.5.5.7.3.9
  static const uint8_t ocsp_signing[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x09};
  return der::Input(ocsp_signing);
}

bool ParseEKUExtension(const der::Input& extension_value,
                       std::vector<der::Input>* eku_oids) {
  der::Parser extension_parser(extension_value);
  der::Parser sequence_parser;
  if (!extension_parser.ReadSequence(&sequence_parser))
    return false;

  // Section 4.2.1.12 of RFC 5280 defines ExtKeyUsageSyntax as:
  // ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
  //
  // Therefore, the sequence must contain at least one KeyPurposeId.
  if (!sequence_parser.HasMore())
    return false;
  while (sequence_parser.HasMore()) {
    der::Input eku_oid;
    if (!sequence_parser.ReadTag(der::kOid, &eku_oid))
      // The SEQUENCE OF must contain only KeyPurposeIds (OIDs).
      return false;
    eku_oids->push_back(eku_oid);
  }
  if (extension_parser.HasMore())
    // The extension value must follow ExtKeyUsageSyntax - there is no way that
    // it could be extended to allow for something after the SEQUENCE OF.
    return false;
  return true;
}

}  // namespace net
