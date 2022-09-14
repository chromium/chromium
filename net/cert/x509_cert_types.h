// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERT_TYPES_H_
#define NET_CERT_X509_CERT_TYPES_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

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

}  // namespace net

#endif  // NET_CERT_X509_CERT_TYPES_H_
