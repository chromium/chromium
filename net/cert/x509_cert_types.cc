// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_cert_types.h"

#include "net/cert/pki/parse_name.h"
#include "net/der/input.h"

namespace net {

CertPrincipal::CertPrincipal() = default;

CertPrincipal::CertPrincipal(const std::string& name) : common_name(name) {}

CertPrincipal::~CertPrincipal() = default;

bool CertPrincipal::ParseDistinguishedName(
    der::Input ber_name_data,
    PrintableStringHandling printable_string_handling) {
  RDNSequence rdns;
  if (!ParseName(ber_name_data, &rdns)) {
    return false;
  }

  auto string_handling =
      printable_string_handling == PrintableStringHandling::kAsUTF8Hack
          ? X509NameAttribute::PrintableStringHandling::kAsUTF8Hack
          : X509NameAttribute::PrintableStringHandling::kDefault;
  for (const RelativeDistinguishedName& rdn : rdns) {
    for (const X509NameAttribute& name_attribute : rdn) {
      if (name_attribute.type == der::Input(kTypeCommonNameOid)) {
        if (common_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &common_name)) {
          return false;
        }
      } else if (name_attribute.type == der::Input(kTypeLocalityNameOid)) {
        if (locality_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &locality_name)) {
          return false;
        }
      } else if (name_attribute.type ==
                 der::Input(kTypeStateOrProvinceNameOid)) {
        if (state_or_province_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(
                string_handling, &state_or_province_name)) {
          return false;
        }
      } else if (name_attribute.type == der::Input(kTypeCountryNameOid)) {
        if (country_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &country_name)) {
          return false;
        }
      } else if (name_attribute.type == der::Input(kTypeStreetAddressOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        street_addresses.push_back(s);
      } else if (name_attribute.type == der::Input(kTypeOrganizationNameOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_names.push_back(s);
      } else if (name_attribute.type ==
                 der::Input(kTypeOrganizationUnitNameOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_unit_names.push_back(s);
      } else if (name_attribute.type == der::Input(kTypeDomainComponentOid)) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        domain_components.push_back(s);
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
