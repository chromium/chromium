// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERT_TYPES_H_
#define NET_CERT_X509_CERT_TYPES_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "third_party/boringssl/src/pki/input.h"

namespace net {

// CertPrincipal represents the issuer or subject field of an X.509 certificate.
struct NET_EXPORT CertPrincipal {
  CertPrincipal();
  CertPrincipal(const CertPrincipal&);
  CertPrincipal(CertPrincipal&&);
  ~CertPrincipal();

  // Configures handling of PrintableString values in the DistinguishedName. Do
  // not use non-default handling without consulting //net owners. With
  // kAsUTF8Hack, PrintableStrings are interpreted as UTF-8 strings.
  enum class PrintableStringHandling { kDefault, kAsUTF8Hack };

  // Parses a BER-format DistinguishedName.
  bool ParseDistinguishedName(
      bssl::der::Input ber_name_data,
      PrintableStringHandling printable_string_handling =
          PrintableStringHandling::kDefault);

  // Returns a name that can be used to represent the issuer.  It tries in this
  // order: CN, O and OU and returns the first non-empty one found.
  std::string GetDisplayName() const;

  // True if this object is equal to `other`. This is only exposed for testing,
  // as a CertPrincipal object does not fully represent the X.509 Name it was
  // parsed from, and comparing them likely does not mean what you want.
  bool EqualsForTesting(const CertPrincipal& other) const;

  // The different attributes for a principal, stored in UTF-8.  They may be "".
  // Note that some of them can have several values.

  std::string common_name;
  std::string locality_name;
  std::string state_or_province_name;
  std::string country_name;

  std::vector<std::string> organization_names;
  std::vector<std::string> organization_unit_names;

 private:
  // Comparison operator is private and only defined for use by
  // EqualsForTesting, see comment there for more details.
  bool operator==(const CertPrincipal& other) const;
};

}  // namespace net

#endif  // NET_CERT_X509_CERT_TYPES_H_
