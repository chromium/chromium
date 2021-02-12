// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/input_file_parsers.h"

#include <set>
#include <sstream>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/tools/transport_security_state_generator/cert_util.h"
#include "net/tools/transport_security_state_generator/pinset.h"
#include "net/tools/transport_security_state_generator/pinsets.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"
#include "third_party/boringssl/src/include/openssl/x509v3.h"

namespace net {

namespace transport_security_state {

namespace {

bool IsImportantWordInCertificateName(base::StringPiece name) {
  const char* const important_words[] = {"Universal", "Global", "EV", "G1",
                                         "G2",        "G3",     "G4", "G5"};
  for (auto* important_word : important_words) {
    if (name == important_word) {
      return true;
    }
  }
  return false;
}

// Strips all characters not matched by the RegEx [A-Za-z0-9_] from |name| and
// returns the result.
std::string FilterName(base::StringPiece name) {
  std::string filtered;
  for (const char& character : name) {
    if ((character >= '0' && character <= '9') ||
        (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') || character == '_') {
      filtered += character;
    }
  }
  return base::ToLowerASCII(filtered);
}

// Returns true if |pin_name| is a reasonable match for the certificate name
// |name|.
bool MatchCertificateName(base::StringPiece name, base::StringPiece pin_name) {
  std::vector<base::StringPiece> words = base::SplitStringPiece(
      name, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (words.empty()) {
    LOG(ERROR) << "No words in certificate name for pin " << pin_name;
    return false;
  }
  base::StringPiece first_word = words[0];

  if (base::EndsWith(first_word, ",")) {
    first_word = first_word.substr(0, first_word.size() - 1);
  }

  if (base::StartsWith(first_word, "*.")) {
    first_word = first_word.substr(2, first_word.size() - 2);
  }

  size_t pos = first_word.find('.');
  if (pos != std::string::npos) {
    first_word = first_word.substr(0, first_word.size() - pos);
  }

  pos = first_word.find('-');
  if (pos != std::string::npos) {
    first_word = first_word.substr(0, first_word.size() - pos);
  }

  if (first_word.empty()) {
    LOG(ERROR) << "First word of certificate name (" << name << ") is empty";
    return false;
  }

  std::string filtered_word = FilterName(first_word);
  first_word = filtered_word;
  if (!base::EqualsCaseInsensitiveASCII(pin_name.substr(0, first_word.size()),
                                        first_word)) {
    LOG(ERROR) << "The first word of the certificate name (" << first_word
               << ") isn't a prefix of the variable name (" << pin_name << ")";
    return false;
  }

  for (size_t i = 0; i < words.size(); ++i) {
    const base::StringPiece& word = words[i];
    if (word == "Class" && (i + 1) < words.size()) {
      std::string class_name = base::StrCat({word, words[i + 1]});

      size_t pos = pin_name.find(class_name);
      if (pos == std::string::npos) {
        LOG(ERROR)
            << "Certficate class specification doesn't appear in the variable "
               "name ("
            << pin_name << ")";
        return false;
      }
    } else if (word.size() == 1 && word[0] >= '0' && word[0] <= '9') {
      size_t pos = pin_name.find(word);
      if (pos == std::string::npos) {
        LOG(ERROR) << "Number doesn't appear in the certificate variable name ("
                   << pin_name << ")";
        return false;
      }
    } else if (IsImportantWordInCertificateName(word)) {
      size_t pos = pin_name.find(word);
      if (pos == std::string::npos) {
        LOG(ERROR) << std::string(word) +
                          " doesn't appear in the certificate variable name ("
                   << pin_name << ")";
        return false;
      }
    }
  }

  return true;
}

// Returns true iff |candidate| is not empty, the first character is in the
// range A-Z, and the remaining characters are in the ranges a-Z, 0-9, or '_'.
bool IsValidName(base::StringPiece candidate) {
  if (candidate.empty() || candidate[0] < 'A' || candidate[0] > 'Z') {
    return false;
  }

  bool isValid = true;
  for (const char& character : candidate) {
    isValid = (character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') || character == '_';
    if (!isValid) {
      return false;
    }
  }
  return true;
}

static const char kStartOfCert[] = "-----BEGIN CERTIFICATE";
static const char kStartOfPublicKey[] = "-----BEGIN PUBLIC KEY";
static const char kEndOfCert[] = "-----END CERTIFICATE";
static const char kEndOfPublicKey[] = "-----END PUBLIC KEY";
static const char kStartOfSHA256[] = "sha256/";

enum class CertificateParserState {
  PRE_NAME,
  POST_NAME,
  IN_CERTIFICATE,
  IN_PUBLIC_KEY
};

// Valid keys for entries in the input JSON. These fields will be included in
// the output.
static const char kNameJSONKey[] = "name";
static const char kIncludeSubdomainsJSONKey[] = "include_subdomains";
static const char kIncludeSubdomainsForPinningJSONKey[] =
    "include_subdomains_for_pinning";
static const char kModeJSONKey[] = "mode";
static const char kPinsJSONKey[] = "pins";
static const char kExpectCTJSONKey[] = "expect_ct";
static const char kExpectCTReportURIJSONKey[] = "expect_ct_report_uri";

// Additional valid keys for entries in the input JSON that will not be included
// in the output and contain metadata (e.g., for list maintenance).
static const char kPolicyJSONKey[] = "policy";

}  // namespace

bool ParseCertificatesFile(base::StringPiece certs_input, Pinsets* pinsets) {
  if (certs_input.find("\r\n") != base::StringPiece::npos) {
    LOG(ERROR) << "CRLF line-endings found in the pins file. All files must "
                  "use LF (unix style) line-endings.";
    return false;
  }

  std::string line;
  CertificateParserState current_state = CertificateParserState::PRE_NAME;

  const base::CompareCase& compare_mode = base::CompareCase::INSENSITIVE_ASCII;
  std::string name;
  std::string buffer;
  std::string subject_name;
  bssl::UniquePtr<X509> certificate;
  SPKIHash hash;

  for (const base::StringPiece& line : SplitStringPiece(
           certs_input, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (!line.empty() && line[0] == '#') {
      continue;
    }

    if (line.empty() && current_state == CertificateParserState::PRE_NAME) {
      continue;
    }

    switch (current_state) {
      case CertificateParserState::PRE_NAME:
        if (!IsValidName(line)) {
          LOG(ERROR) << "Invalid name in pins file: " << line;
          return false;
        }
        name = std::string(line);
        current_state = CertificateParserState::POST_NAME;
        break;
      case CertificateParserState::POST_NAME:
        if (base::StartsWith(line, kStartOfSHA256, compare_mode)) {
          if (!hash.FromString(line)) {
            LOG(ERROR) << "Invalid hash value in pins file for " << name;
            return false;
          }

          pinsets->RegisterSPKIHash(name, hash);
          current_state = CertificateParserState::PRE_NAME;
        } else if (base::StartsWith(line, kStartOfCert, compare_mode)) {
          buffer = std::string(line) + '\n';
          current_state = CertificateParserState::IN_CERTIFICATE;
        } else if (base::StartsWith(line, kStartOfPublicKey, compare_mode)) {
          buffer = std::string(line) + '\n';
          current_state = CertificateParserState::IN_PUBLIC_KEY;
        } else {
          LOG(ERROR) << "Invalid value in pins file for " << name;
          return false;
        }
        break;
      case CertificateParserState::IN_CERTIFICATE:
        buffer += std::string(line) + '\n';
        if (!base::StartsWith(line, kEndOfCert, compare_mode)) {
          continue;
        }

        certificate = GetX509CertificateFromPEM(buffer);
        if (!certificate) {
          LOG(ERROR) << "Could not parse certificate " << name;
          return false;
        }

        if (!CalculateSPKIHashFromCertificate(certificate.get(), &hash)) {
          LOG(ERROR) << "Could not extract SPKI from certificate " << name;
          return false;
        }

        if (!ExtractSubjectNameFromCertificate(certificate.get(),
                                               &subject_name)) {
          LOG(ERROR) << "Could not extract name from certificate " << name;
          return false;
        }

        if (!MatchCertificateName(subject_name, name)) {
          LOG(ERROR) << name << " is not a reasonable name for "
                     << subject_name;
          return false;
        }

        pinsets->RegisterSPKIHash(name, hash);
        current_state = CertificateParserState::PRE_NAME;
        break;
      case CertificateParserState::IN_PUBLIC_KEY:
        buffer += std::string(line) + '\n';
        if (!base::StartsWith(line, kEndOfPublicKey, compare_mode)) {
          continue;
        }

        if (!CalculateSPKIHashFromKey(buffer, &hash)) {
          LOG(ERROR) << "Could not parse the public key for " << name;
          return false;
        }

        pinsets->RegisterSPKIHash(name, hash);
        current_state = CertificateParserState::PRE_NAME;
        break;
      default:
        DCHECK(false) << "Unknown parser state";
    }
  }

  return true;
}

bool ParseJSON(base::StringPiece json,
               TransportSecurityStateEntries* entries,
               Pinsets* pinsets) {
  std::set<std::string> valid_keys = {kNameJSONKey,
                                      kPolicyJSONKey,
                                      kIncludeSubdomainsJSONKey,
                                      kIncludeSubdomainsForPinningJSONKey,
                                      kModeJSONKey,
                                      kPinsJSONKey,
                                      kExpectCTJSONKey,
                                      kExpectCTReportURIJSONKey};

  // See the comments in net/http/transport_security_state_static.json for more
  // info on these policies.
  std::set<std::string> valid_policies = {
      "test",        "public-suffix", "google",      "custom",
      "bulk-legacy", "bulk-18-weeks", "bulk-1-year", "public-suffix-requested"};

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(json);
  base::DictionaryValue* dict_value = nullptr;
  if (!value.get() || !value->GetAsDictionary(&dict_value)) {
    LOG(ERROR) << "Could not parse the input JSON file";
    return false;
  }

  const base::ListValue* preload_entries = nullptr;
  if (!dict_value->GetList("entries", &preload_entries)) {
    LOG(ERROR) << "Could not parse the entries in the input JSON";
    return false;
  }

  for (size_t i = 0; i < preload_entries->GetSize(); ++i) {
    const base::DictionaryValue* parsed = nullptr;
    if (!preload_entries->GetDictionary(i, &parsed)) {
      LOG(ERROR) << "Could not parse entry " << base::NumberToString(i)
                 << " in the input JSON";
      return false;
    }

    std::unique_ptr<TransportSecurityStateEntry> entry(
        new TransportSecurityStateEntry());

    if (!parsed->GetString(kNameJSONKey, &entry->hostname)) {
      LOG(ERROR) << "Could not extract the hostname for entry "
                 << base::NumberToString(i) << " from the input JSON";
      return false;
    }

    if (entry->hostname.empty()) {
      LOG(ERROR) << "The hostname for entry " << base::NumberToString(i)
                 << " is empty";
      return false;
    }

    for (const auto& entry_value : *parsed) {
      if (valid_keys.find(entry_value.first) == valid_keys.cend()) {
        LOG(ERROR) << "The entry for " << entry->hostname
                   << " contains an unknown " << entry_value.first << " field";
        return false;
      }
    }

    std::string policy;
    parsed->GetString(kPolicyJSONKey, &policy);
    if (valid_policies.find(policy) == valid_policies.cend()) {
      LOG(ERROR) << "The entry for " << entry->hostname
                 << " does not have a valid policy";
      return false;
    }

    std::string mode;
    parsed->GetString(kModeJSONKey, &mode);
    entry->force_https = false;
    if (mode == "force-https") {
      entry->force_https = true;
    } else if (!mode.empty()) {
      LOG(ERROR) << "An unknown mode is set for entry " << entry->hostname;
      return false;
    }

    parsed->GetBoolean(kIncludeSubdomainsJSONKey, &entry->include_subdomains);
    parsed->GetBoolean(kIncludeSubdomainsForPinningJSONKey,
                       &entry->hpkp_include_subdomains);
    parsed->GetString(kPinsJSONKey, &entry->pinset);
    parsed->GetBoolean(kExpectCTJSONKey, &entry->expect_ct);
    parsed->GetString(kExpectCTReportURIJSONKey, &entry->expect_ct_report_uri);

    entries->push_back(std::move(entry));
  }

  const base::ListValue* pinsets_list = nullptr;
  if (!dict_value->GetList("pinsets", &pinsets_list)) {
    LOG(ERROR) << "Could not parse the pinsets in the input JSON";
    return false;
  }

  for (size_t i = 0; i < pinsets_list->GetSize(); ++i) {
    const base::DictionaryValue* parsed = nullptr;
    if (!pinsets_list->GetDictionary(i, &parsed)) {
      LOG(ERROR) << "Could not parse pinset " << base::NumberToString(i)
                 << " in the input JSON";
      return false;
    }

    std::string name;
    if (!parsed->GetString("name", &name)) {
      LOG(ERROR) << "Could not extract the name for pinset "
                 << base::NumberToString(i) << " from the input JSON";
      return false;
    }

    std::string report_uri;
    parsed->GetString("report_uri", &report_uri);

    std::unique_ptr<Pinset> pinset(new Pinset(name, report_uri));

    const base::ListValue* pinset_static_hashes_list = nullptr;
    if (parsed->GetList("static_spki_hashes", &pinset_static_hashes_list)) {
      for (size_t i = 0; i < pinset_static_hashes_list->GetSize(); ++i) {
        std::string hash;
        pinset_static_hashes_list->GetString(i, &hash);
        pinset->AddStaticSPKIHash(hash);
      }
    }

    const base::ListValue* pinset_bad_static_hashes_list = nullptr;
    if (parsed->GetList("bad_static_spki_hashes",
                        &pinset_bad_static_hashes_list)) {
      for (size_t i = 0; i < pinset_bad_static_hashes_list->GetSize(); ++i) {
        std::string hash;
        pinset_bad_static_hashes_list->GetString(i, &hash);
        pinset->AddBadStaticSPKIHash(hash);
      }
    }

    pinsets->RegisterPinset(std::move(pinset));
  }

  return true;
}

}  // namespace transport_security_state

}  // namespace net
