// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/input_file_parsers.h"

#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/tools/transport_security_state_generator/cert_util.h"
#include "net/tools/transport_security_state_generator/pinset.h"
#include "net/tools/transport_security_state_generator/pinsets.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"
#include "third_party/boringssl/src/include/openssl/x509v3.h"

namespace net::transport_security_state {

namespace {

bool IsImportantWordInCertificateName(std::string_view name) {
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
std::string FilterName(std::string_view name) {
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
bool MatchCertificateName(std::string_view name, std::string_view pin_name) {
  std::vector<std::string_view> words = base::SplitStringPiece(
      name, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (words.empty()) {
    LOG(ERROR) << "No words in certificate name for pin " << pin_name;
    return false;
  }
  std::string_view first_word = words[0];

  if (first_word.ends_with(",")) {
    first_word = first_word.substr(0, first_word.size() - 1);
  }

  if (first_word.starts_with("*.")) {
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
    std::string_view word = words[i];
    if (word == "Class" && (i + 1) < words.size()) {
      std::string class_name = base::StrCat({word, words[i + 1]});

      pos = pin_name.find(class_name);
      if (pos == std::string::npos) {
        LOG(ERROR)
            << "Certficate class specification doesn't appear in the variable "
               "name ("
            << pin_name << ")";
        return false;
      }
    } else if (word.size() == 1 && word[0] >= '0' && word[0] <= '9') {
      pos = pin_name.find(word);
      if (pos == std::string::npos) {
        LOG(ERROR) << "Number doesn't appear in the certificate variable name ("
                   << pin_name << ")";
        return false;
      }
    } else if (IsImportantWordInCertificateName(word)) {
      pos = pin_name.find(word);
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
bool IsValidName(std::string_view candidate) {
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
  IN_PUBLIC_KEY,
  PRE_TIMESTAMP,
};

// Valid keys for entries in the input JSON. These fields will be included in
// the output.
static constexpr char kNameJSONKey[] = "name";
static constexpr char kIncludeSubdomainsJSONKey[] = "include_subdomains";
static constexpr char kModeJSONKey[] = "mode";
static constexpr char kPinsJSONKey[] = "pins";
static constexpr char kTimestampName[] = "PinsListTimestamp";

// Additional valid keys for entries in the input JSON that will not be included
// in the output and contain metadata (e.g., for list maintenance).
static constexpr char kPolicyJSONKey[] = "policy";

}  // namespace

bool ParseCertificatesFile(std::string_view certs_input,
                           Pinsets* pinsets,
                           base::Time* timestamp) {
  if (certs_input.find("\r\n") != std::string_view::npos) {
    LOG(ERROR) << "CRLF line-endings found in the pins file. All files must "
                  "use LF (unix style) line-endings.";
    return false;
  }

  CertificateParserState current_state = CertificateParserState::PRE_NAME;
  bool timestamp_parsed = false;

  const base::CompareCase& compare_mode = base::CompareCase::INSENSITIVE_ASCII;
  std::string name;
  std::string buffer;
  std::string subject_name;
  bssl::UniquePtr<X509> certificate;
  SPKIHash hash;

  for (std::string_view line : SplitStringPiece(
           certs_input, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (!line.empty() && line[0] == '#') {
      continue;
    }

    if (line.empty() && current_state == CertificateParserState::PRE_NAME) {
      continue;
    }

    switch (current_state) {
      case CertificateParserState::PRE_NAME:
        if (line == kTimestampName) {
          current_state = CertificateParserState::PRE_TIMESTAMP;
          break;
        }
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
      case CertificateParserState::PRE_TIMESTAMP:
        uint64_t timestamp_epoch;
        if (!base::StringToUint64(line, &timestamp_epoch) ||
            !base::IsValueInRangeForNumericType<time_t>(timestamp_epoch)) {
          LOG(ERROR) << "Could not parse the timestamp value";
          return false;
        }
        *timestamp = base::Time::FromTimeT(timestamp_epoch);
        if (timestamp_parsed) {
          LOG(ERROR) << "File contains multiple timestamps";
          return false;
        }
        timestamp_parsed = true;
        current_state = CertificateParserState::PRE_NAME;
        break;
      default:
        DCHECK(false) << "Unknown parser state";
    }
  }

  if (!timestamp_parsed) {
    LOG(ERROR) << "Timestamp is missing";
    return false;
  }
  return true;
}

bool ParseJSON(std::string_view hsts_json,
               std::string_view pins_json,
               TransportSecurityStateEntries* entries,
               Pinsets* pinsets) {
  static constexpr auto valid_hsts_keys =
      base::MakeFixedFlatSet<std::string_view>({
          kNameJSONKey,
          kPolicyJSONKey,
          kIncludeSubdomainsJSONKey,
          kModeJSONKey,
          kPinsJSONKey,
      });

  static constexpr auto valid_pins_keys =
      base::MakeFixedFlatSet<std::string_view>({
          kNameJSONKey,
          kIncludeSubdomainsJSONKey,
          kPinsJSONKey,
      });

  // See the comments in net/http/transport_security_state_static.json for more
  // info on these policies.
  std::set<std::string> valid_policies = {
      "test",        "public-suffix", "google",      "custom",
      "bulk-legacy", "bulk-18-weeks", "bulk-1-year", "public-suffix-requested"};

  std::optional<base::Value> hsts_value = base::JSONReader::Read(hsts_json);
  if (!hsts_value.has_value() || !hsts_value->is_dict()) {
    LOG(ERROR) << "Could not parse the input HSTS JSON file";
    return false;
  }

  std::optional<base::Value> pins_value = base::JSONReader::Read(pins_json);
  if (!pins_value.has_value()) {
    LOG(ERROR) << "Could not parse the input pins JSON file";
    return false;
  }
  base::Value::Dict* pins_dict = pins_value->GetIfDict();
  if (!pins_dict) {
    LOG(ERROR) << "Input pins JSON file does not contain a dictionary";
    return false;
  }

  const base::Value::List* pinning_entries_list =
      pins_dict->FindList("entries");
  if (!pinning_entries_list) {
    LOG(ERROR) << "Could not parse the entries in the input pins JSON";
    return false;
  }
  std::map<std::string, std::pair<std::string, bool>> pins_map;
  for (size_t i = 0; i < pinning_entries_list->size(); ++i) {
    const base::Value::Dict* parsed = (*pinning_entries_list)[i].GetIfDict();
    if (!parsed) {
      LOG(ERROR) << "Could not parse entry " << base::NumberToString(i)
                 << " in the input pins JSON";
      return false;
    }
    const std::string* maybe_hostname = parsed->FindString(kNameJSONKey);
    if (!maybe_hostname) {
      LOG(ERROR) << "Could not extract the hostname for entry "
                 << base::NumberToString(i) << " from the input pins JSON";
      return false;
    }

    if (maybe_hostname->empty()) {
      LOG(ERROR) << "The hostname for entry " << base::NumberToString(i)
                 << " is empty";
      return false;
    }

    for (auto entry_value : *parsed) {
      if (!base::Contains(valid_pins_keys, entry_value.first)) {
        LOG(ERROR) << "The entry for " << *maybe_hostname
                   << " contains an unknown " << entry_value.first << " field";
        return false;
      }
    }

    const std::string* maybe_pinset = parsed->FindString(kPinsJSONKey);
    if (!maybe_pinset) {
      LOG(ERROR) << "Could not extract the pinset for entry "
                 << base::NumberToString(i) << " from the input pins JSON";
      return false;
    }

    if (pins_map.find(*maybe_hostname) != pins_map.end()) {
      LOG(ERROR) << *maybe_hostname
                 << " has duplicate entries in the input pins JSON";
      return false;
    }

    pins_map[*maybe_hostname] =
        std::pair(*maybe_pinset,
                  parsed->FindBool(kIncludeSubdomainsJSONKey).value_or(false));
  }

  const base::Value::List* preload_entries_list =
      hsts_value->GetDict().FindList("entries");
  if (!preload_entries_list) {
    LOG(ERROR) << "Could not parse the entries in the input HSTS JSON";
    return false;
  }

  for (size_t i = 0; i < preload_entries_list->size(); ++i) {
    const base::Value::Dict* parsed = (*preload_entries_list)[i].GetIfDict();
    if (!parsed) {
      LOG(ERROR) << "Could not parse entry " << base::NumberToString(i)
                 << " in the input HSTS JSON";
      return false;
    }

    auto entry = std::make_unique<TransportSecurityStateEntry>();
    const std::string* maybe_hostname = parsed->FindString(kNameJSONKey);
    if (!maybe_hostname) {
      LOG(ERROR) << "Could not extract the hostname for entry "
                 << base::NumberToString(i) << " from the input HSTS JSON";
      return false;
    }
    entry->hostname = *maybe_hostname;

    if (entry->hostname.empty()) {
      LOG(ERROR) << "The hostname for entry " << base::NumberToString(i)
                 << " is empty";
      return false;
    }

    for (auto entry_value : *parsed) {
      if (!base::Contains(valid_hsts_keys, entry_value.first)) {
        LOG(ERROR) << "The entry for " << entry->hostname
                   << " contains an unknown " << entry_value.first << " field";
        return false;
      }
    }

    const std::string* policy = parsed->FindString(kPolicyJSONKey);
    if (!policy || !base::Contains(valid_policies, *policy)) {
      LOG(ERROR) << "The entry for " << entry->hostname
                 << " does not have a valid policy";
      return false;
    }

    const std::string* maybe_mode = parsed->FindString(kModeJSONKey);
    std::string mode = maybe_mode ? *maybe_mode : std::string();
    entry->force_https = false;
    if (mode == "force-https") {
      entry->force_https = true;
    } else if (!mode.empty()) {
      LOG(ERROR) << "An unknown mode is set for entry " << entry->hostname;
      return false;
    }

    entry->include_subdomains =
        parsed->FindBool(kIncludeSubdomainsJSONKey).value_or(false);

    auto pins_it = pins_map.find(entry->hostname);
    if (pins_it != pins_map.end()) {
      entry->pinset = pins_it->second.first;
      entry->hpkp_include_subdomains = pins_it->second.second;
      pins_map.erase(entry->hostname);
    }

    entries->push_back(std::move(entry));
  }

  // Any remaining entries in pins_map have pinning information, but are not
  // HSTS preloaded.
  for (auto const& pins_entry : pins_map) {
    auto entry = std::make_unique<TransportSecurityStateEntry>();
    entry->hostname = pins_entry.first;
    entry->force_https = false;
    entry->pinset = pins_entry.second.first;
    entry->hpkp_include_subdomains = pins_entry.second.second;
    entries->push_back(std::move(entry));
  }

  base::Value::List* pinsets_list = pins_dict->FindList("pinsets");
  if (!pinsets_list) {
    LOG(ERROR) << "Could not parse the pinsets in the input JSON";
    return false;
  }

  for (size_t i = 0; i < pinsets_list->size(); ++i) {
    const base::Value::Dict* parsed = (*pinsets_list)[i].GetIfDict();
    if (!parsed) {
      LOG(ERROR) << "Could not parse pinset " << base::NumberToString(i)
                 << " in the input JSON";
      return false;
    }

    const std::string* maybe_name = parsed->FindString("name");
    if (!maybe_name) {
      LOG(ERROR) << "Could not extract the name for pinset "
                 << base::NumberToString(i) << " from the input JSON";
      return false;
    }
    std::string name = *maybe_name;

    const std::string* maybe_report_uri = parsed->FindString("report_uri");
    std::string report_uri =
        maybe_report_uri ? *maybe_report_uri : std::string();

    auto pinset = std::make_unique<Pinset>(name, report_uri);

    const base::Value::List* pinset_static_hashes_list =
        parsed->FindList("static_spki_hashes");
    if (pinset_static_hashes_list) {
      for (const auto& hash : *pinset_static_hashes_list) {
        if (!hash.is_string()) {
          LOG(ERROR) << "Could not parse static spki hash "
                     << hash.DebugString() << " in the input JSON";
          return false;
        }
        pinset->AddStaticSPKIHash(hash.GetString());
      }
    }

    const base::Value::List* pinset_bad_static_hashes_list =
        parsed->FindList("bad_static_spki_hashes");
    if (pinset_bad_static_hashes_list) {
      for (const auto& hash : *pinset_bad_static_hashes_list) {
        if (!hash.is_string()) {
          LOG(ERROR) << "Could not parse bad static spki hash "
                     << hash.DebugString() << " in the input JSON";
          return false;
        }
        pinset->AddBadStaticSPKIHash(hash.GetString());
      }
    }

    pinsets->RegisterPinset(std::move(pinset));
  }

  return true;
}

}  // namespace net::transport_security_state
