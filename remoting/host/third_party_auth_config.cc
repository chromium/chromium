// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/third_party_auth_config.h"

#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"

namespace remoting {

namespace {

bool ParseUrlPolicy(const std::string& str, GURL* out) {
  if (str.empty()) {
    *out = GURL();
    return true;
  }

  GURL gurl(str);
  if (!gurl.is_valid()) {
    LOG(ERROR) << "Not a valid URL: " << str;
    return false;
  }
// We validate https-vs-http only on Release builds to help with manual testing.
#if defined(NDEBUG)
  if (!gurl.SchemeIsCryptographic()) {
    LOG(ERROR) << "Not a secure URL: " << str;
    return false;
  }
#endif

  *out = gurl;
  return true;
}

}  // namespace

bool ThirdPartyAuthConfig::ParseStrings(
    const std::string& token_url,
    const std::string& token_validation_url,
    const std::string& token_validation_cert_issuer,
    ThirdPartyAuthConfig* result) {
  ThirdPartyAuthConfig tmp;

  // Extract raw values for the 3 individual fields.
  bool urls_valid = true;
  urls_valid &= ParseUrlPolicy(token_url, &tmp.token_url);
  urls_valid &= ParseUrlPolicy(token_validation_url, &tmp.token_validation_url);
  if (!urls_valid) {
    return false;
  }
  tmp.token_validation_cert_issuer = token_validation_cert_issuer;

  // Validate inter-dependencies between the 3 fields.
  if (tmp.token_url.is_empty() ^ tmp.token_validation_url.is_empty()) {
    LOG(ERROR) << "TokenUrl and TokenValidationUrl "
               << "have to be specified together.";
    return false;
  }
  if (!tmp.token_validation_cert_issuer.empty() && tmp.token_url.is_empty()) {
    LOG(ERROR) << "TokenValidationCertificateIssuer cannot be used "
               << "without TokenUrl and TokenValidationUrl.";
    return false;
  }

  *result = tmp;
  return true;
}

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
void ExtractHelper(const base::Value::Dict& policy_dict,
                   const std::string& policy_name,
                   bool* policy_present,
                   std::string* policy_value) {
  DCHECK(policy_value);
  if (const std::string* value = policy_dict.FindString(policy_name)) {
    *policy_value = *value;
    *policy_present = true;
  } else {
    policy_value->clear();
  }
}
#endif

}  // namespace

bool ThirdPartyAuthConfig::ExtractStrings(
    const base::Value::Dict& policy_dict,
    std::string* token_url,
    std::string* token_validation_url,
    std::string* token_validation_cert_issuer) {
  bool policies_present = false;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
  ExtractHelper(policy_dict, policy::key::kRemoteAccessHostTokenUrl,
                &policies_present, token_url);
  ExtractHelper(policy_dict, policy::key::kRemoteAccessHostTokenValidationUrl,
                &policies_present, token_validation_url);
  ExtractHelper(policy_dict,
                policy::key::kRemoteAccessHostTokenValidationCertificateIssuer,
                &policies_present, token_validation_cert_issuer);
#endif
  return policies_present;
}

ThirdPartyAuthConfig::ParseStatus ThirdPartyAuthConfig::Parse(
    const base::Value::Dict& policy_dict,
    ThirdPartyAuthConfig* result) {
  // Extract 3 individial policy values.
  std::string token_url;
  std::string token_validation_url;
  std::string token_validation_cert_issuer;
  if (!ThirdPartyAuthConfig::ExtractStrings(policy_dict, &token_url,
                                            &token_validation_url,
                                            &token_validation_cert_issuer)) {
    return NoPolicy;
  }

  // Parse the policy value.
  if (!ThirdPartyAuthConfig::ParseStrings(token_url, token_validation_url,
                                          token_validation_cert_issuer,
                                          result)) {
    return InvalidPolicy;
  }

  return ParsingSuccess;
}

std::ostream& operator<<(std::ostream& os, const ThirdPartyAuthConfig& cfg) {
  if (cfg.is_null()) {
    os << "<no 3rd party auth config specified>";
  } else {
    os << "TokenUrl = <" << cfg.token_url << ">, ";
    os << "TokenValidationUrl = <" << cfg.token_validation_url << ">, ";
    os << "TokenValidationCertificateIssuer = <"
       << cfg.token_validation_cert_issuer << ">";
  }
  return os;
}

}  // namespace remoting
