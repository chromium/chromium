// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERT_TYPES_H_
#define NET_CERT_X509_CERT_TYPES_H_

#include <stddef.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/cert_status_flags.h"

namespace base {
class Time;
}  // namespace base

namespace net {

// CertPrincipal represents the issuer or subject field of an X.509 certificate.
struct NET_EXPORT CertPrincipal {
  CertPrincipal();
  explicit CertPrincipal(const std::string& name);
  ~CertPrincipal();

  // Configures handling of PrintableString values in the DistinguishedName. Do
  // not use non-default handling without consulting //net owners. With
  // kAsUTF8Hack, PrintableStrings are interpreted as UTF-8 strings.
  enum class PrintableStringHandling { kDefault, kAsUTF8Hack };

  // Parses a BER-format DistinguishedName.
  // TODO(mattm): change this to take a der::Input.
  bool ParseDistinguishedName(
      const void* ber_name_data,
      size_t length,
      PrintableStringHandling printable_string_handling =
          PrintableStringHandling::kDefault);

  // Returns a name that can be used to represent the issuer.  It tries in this
  // order: CN, O and OU and returns the first non-empty one found.
  std::string GetDisplayName() const;

  // The different attributes for a principal, stored in UTF-8.  They may be "".
  // Note that some of them can have several values.

  std::string common_name;
  std::string locality_name;
  std::string state_or_province_name;
  std::string country_name;

  std::vector<std::string> street_addresses;
  std::vector<std::string> organization_names;
  std::vector<std::string> organization_unit_names;
  std::vector<std::string> domain_components;
};

// A list of ASN.1 date/time formats that ParseCertificateDate() supports,
// encoded in the canonical forms specified in RFC 2459/3280/5280.
enum CertDateFormat {
  // UTCTime: Format is YYMMDDHHMMSSZ
  CERT_DATE_FORMAT_UTC_TIME,

  // GeneralizedTime: Format is YYYYMMDDHHMMSSZ
  CERT_DATE_FORMAT_GENERALIZED_TIME,
};

// Attempts to parse |raw_date|, an ASN.1 date/time string encoded as
// |format|, and writes the result into |*time|. If an invalid date is
// specified, or if parsing fails, returns false, and |*time| will not be
// updated.
NET_EXPORT_PRIVATE bool ParseCertificateDate(const base::StringPiece& raw_date,
                                             CertDateFormat format,
                                             base::Time* time);
}  // namespace net

#endif  // NET_CERT_X509_CERT_TYPES_H_
