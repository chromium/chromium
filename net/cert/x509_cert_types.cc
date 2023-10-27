// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_cert_types.h"

#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_name.h"

namespace net {

CertPrincipal::CertPrincipal() = default;

CertPrincipal::CertPrincipal(const CertPrincipal&) = default;

CertPrincipal::CertPrincipal(CertPrincipal&&) = default;

CertPrincipal::~CertPrincipal() = default;

bool CertPrincipal::operator==(const CertPrincipal& other) const = default;

bool CertPrincipal::EqualsForTesting(const CertPrincipal& other) const {
  return *this == other;
}

bool CertPrincipal::ParseDistinguishedName(
    bssl::der::Input ber_name_data,
    PrintableStringHandling printable_string_handling) {
  bssl::RDNSequence rdns;
  if (!ParseName(ber_name_data, &rdns)) {
    return false;
  }

  auto string_handling =
      printable_string_handling == PrintableStringHandling::kAsUTF8Hack
          ? bssl::X509NameAttribute::PrintableStringHandling::kAsUTF8Hack
          : bssl::X509NameAttribute::PrintableStringHandling::kDefault;
  for (const bssl::RelativeDistinguishedName& rdn : rdns) {
    for (const bssl::X509NameAttribute& name_attribute : rdn) {
      if (name_attribute.type == bssl::der::Input(bssl::kTypeCommonNameOid)) {
        if (common_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &common_name)) {
          return false;
        }
      } else if (name_attribute.type ==
                 bssl::der::Input(bssl::kTypeLocalityNameOid)) {
        if (locality_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &locality_name)) {
          return false;
        }
      } else if (name_attribute.type ==
                 bssl::der::Input(bssl::kTypeStateOrProvinceNameOid)) {
        if (state_or_province_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(
                string_handling, &state_or_province_name)) {
          return false;
        }
      } else if (name_attribute.type ==
                 bssl::der::Input(bssl::kTypeCountryNameOid)) {
        if (country_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &country_name)) {
          return false;
        }
      } else if (name_attribute.type ==
                 bssl::der::Input(bssl::kTypeOrganizationNameOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_names.push_back(s);
      } else if (name_attribute.type ==
                 bssl::der::Input(bssl::kTypeOrganizationUnitNameOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_unit_names.push_back(s);
      }
    }
  }
  return true;
}

std::string CertPrincipal::GetDisplayName() const {
  if (!common_name.empty())
    return common_name;
  if (!organization_names.empty())
    return organization_names[0];
  if (!organization_unit_names.empty())
    return organization_unit_names[0];

  return std::string();
}

}  // namespace net
