// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_cert_types.h"

#include <cstdlib>
#include <cstring>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/parse_number.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/x509_certificate.h"
#include "net/der/input.h"

namespace net {

namespace {

// Helper for ParseCertificateDate. |*field| must contain at least
// |field_len| characters. |*field| will be advanced by |field_len| on exit.
// |*ok| is set to false if there is an error in parsing the number, but left
// untouched otherwise. Returns the parsed integer.
int ParseIntAndAdvance(const char** field, size_t field_len, bool* ok) {
  int result = 0;
  *ok &= ParseInt32(base::StringPiece(*field, field_len),
                    ParseIntFormat::NON_NEGATIVE, &result);
  *field += field_len;
  return result;
}

}  // anonymous namespace

CertPrincipal::CertPrincipal() = default;

CertPrincipal::CertPrincipal(const std::string& name) : common_name(name) {}

CertPrincipal::~CertPrincipal() = default;

bool CertPrincipal::ParseDistinguishedName(
    const void* ber_name_data,
    size_t length,
    PrintableStringHandling printable_string_handling) {
  RDNSequence rdns;
  if (!ParseName(
          der::Input(reinterpret_cast<const uint8_t*>(ber_name_data), length),
          &rdns))
    return false;

  auto string_handling =
      printable_string_handling == PrintableStringHandling::kAsUTF8Hack
          ? X509NameAttribute::PrintableStringHandling::kAsUTF8Hack
          : X509NameAttribute::PrintableStringHandling::kDefault;
  for (const RelativeDistinguishedName& rdn : rdns) {
    for (const X509NameAttribute& name_attribute : rdn) {
      if (name_attribute.type == TypeCommonNameOid()) {
        if (common_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &common_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeLocalityNameOid()) {
        if (locality_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &locality_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeStateOrProvinceNameOid()) {
        if (state_or_province_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(
                string_handling, &state_or_province_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeCountryNameOid()) {
        if (country_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &country_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeStreetAddressOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        street_addresses.push_back(s);
      } else if (name_attribute.type == TypeOrganizationNameOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_names.push_back(s);
      } else if (name_attribute.type == TypeOrganizationUnitNameOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_unit_names.push_back(s);
      } else if (name_attribute.type == TypeDomainComponentOid()) {
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

bool ParseCertificateDate(const base::StringPiece& raw_date,
                          CertDateFormat format,
                          base::Time* time) {
  size_t year_length = format == CERT_DATE_FORMAT_UTC_TIME ? 2 : 4;

  if (raw_date.length() < 11 + year_length)
    return false;

  const char* field = raw_date.data();
  bool valid = true;
  base::Time::Exploded exploded = {0};

  exploded.year =         ParseIntAndAdvance(&field, year_length, &valid);
  exploded.month =        ParseIntAndAdvance(&field, 2, &valid);
  exploded.day_of_month = ParseIntAndAdvance(&field, 2, &valid);
  exploded.hour =         ParseIntAndAdvance(&field, 2, &valid);
  exploded.minute =       ParseIntAndAdvance(&field, 2, &valid);
  exploded.second =       ParseIntAndAdvance(&field, 2, &valid);
  if (valid && year_length == 2)
    exploded.year += exploded.year < 50 ? 2000 : 1900;

  if (!valid)
    return false;
  return base::Time::FromUTCExploded(exploded, time);
}

}  // namespace net
