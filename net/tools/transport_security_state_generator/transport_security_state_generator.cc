// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/openssl_util.h"
#include "net/tools/transport_security_state_generator/input_file_parsers.h"
#include "net/tools/transport_security_state_generator/pinsets.h"
#include "net/tools/transport_security_state_generator/preloaded_state_generator.h"
#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"

using net::transport_security_state::TransportSecurityStateEntries;
using net::transport_security_state::Pinsets;
using net::transport_security_state::PreloadedStateGenerator;

namespace {

// Print the command line help.
void PrintHelp() {
  std::cout << "transport_security_state_generator <json-file> <pins-file>"
            << " <template-file> <output-file> [--v=1]" << std::endl;
}

// Checks if there are pins with the same name or the same hash.
bool CheckForDuplicatePins(const Pinsets& pinsets) {
  std::set<std::string> seen_names;
  std::map<std::string, std::string> seen_hashes;

  for (const auto& pin : pinsets.spki_hashes()) {
    if (seen_names.find(pin.first) != seen_names.cend()) {
      LOG(ERROR) << "Duplicate pin name " << pin.first << " in pins file";
      return false;
    }
    seen_names.insert(pin.first);

    std::string hash =
        std::string(pin.second.data(), pin.second.data() + pin.second.size());
    auto it = seen_hashes.find(hash);
    if (it != seen_hashes.cend()) {
      LOG(ERROR) << "Duplicate pin hash for " << pin.first
                 << ", already seen as " << it->second;
      return false;
    }
    seen_hashes.insert(std::pair<std::string, std::string>(hash, pin.first));
  }

  return true;
}

// Checks if there are pinsets that reference non-existing pins, if two
// pinsets share the same name, or if there are unused pins.
bool CheckCertificatesInPinsets(const Pinsets& pinsets) {
  std::set<std::string> pin_names;
  for (const auto& pin : pinsets.spki_hashes()) {
    pin_names.insert(pin.first);
  }

  std::set<std::string> used_pin_names;
  std::set<std::string> pinset_names;
  for (const auto& pinset : pinsets.pinsets()) {
    if (pinset_names.find(pinset.second->name()) != pinset_names.cend()) {
      LOG(ERROR) << "Duplicate pinset name " << pinset.second->name();
      return false;
    }
    pinset_names.insert(pinset.second->name());

    const std::vector<std::string>& good_hashes =
        pinset.second->static_spki_hashes();
    const std::vector<std::string>& bad_hashes =
        pinset.second->bad_static_spki_hashes();

    std::vector<std::string> all_pin_names;
    all_pin_names.reserve(good_hashes.size() + bad_hashes.size());
    all_pin_names.insert(all_pin_names.end(), good_hashes.begin(),
                         good_hashes.end());
    all_pin_names.insert(all_pin_names.end(), bad_hashes.begin(),
                         bad_hashes.end());

    for (const auto& pin_name : all_pin_names) {
      if (pin_names.find(pin_name) == pin_names.cend()) {
        LOG(ERROR) << "Pinset " << pinset.second->name()
                   << " references pin " + pin_name << " which doesn't exist";
        return false;
      }
      used_pin_names.insert(pin_name);
    }
  }

  for (const auto& pin_name : pin_names) {
    if (used_pin_names.find(pin_name) == used_pin_names.cend()) {
      LOG(ERROR) << "Pin " << pin_name << " is unused.";
      return false;
    }
  }

  return true;
}

// Checks if there are two or more entries for the same hostname.
bool CheckDuplicateEntries(const TransportSecurityStateEntries& entries) {
  std::set<std::string> seen_entries;
  for (const auto& entry : entries) {
    if (seen_entries.find(entry->hostname) != seen_entries.cend()) {
      LOG(ERROR) << "Duplicate entry for " << entry->hostname;
      return false;
    }
    seen_entries.insert(entry->hostname);
  }
  return true;
}

// Checks for entries which have no effect.
bool CheckNoopEntries(const TransportSecurityStateEntries& entries) {
  for (const auto& entry : entries) {
    if (!entry->force_https && entry->pinset.empty() && !entry->expect_ct) {
      if (entry->hostname == "learn.doubleclick.net") {
        // This entry is deliberately used as an exclusion.
        continue;
      }

      LOG(ERROR) << "Entry for " << entry->hostname
                 << " has no mode, no pins and is not expect-CT";
      return false;
    }
  }
  return true;
}

// Checks all entries for incorrect usage of the includeSubdomains flags.
bool CheckSubdomainsFlags(const TransportSecurityStateEntries& entries) {
  for (const auto& entry : entries) {
    if (entry->include_subdomains && entry->hpkp_include_subdomains) {
      LOG(ERROR) << "Entry for " << entry->hostname
                 << " sets include_subdomains_for_pinning but also sets "
                    "include_subdomains, which implies it";
      return false;
    }
  }
  return true;
}

bool IsLowercaseAlphanumeric(char c) {
  return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

// Checks the well-formedness of the hostnames. All hostnames should be in their
// canonicalized form because they will be matched against canonicalized input.
bool CheckHostnames(const TransportSecurityStateEntries& entries) {
  for (const auto& entry : entries) {
    const std::string& hostname = entry->hostname;

    bool in_component = false;
    bool most_recent_component_started_alphanumeric = false;
    for (const char& c : hostname) {
      if (!in_component) {
        most_recent_component_started_alphanumeric = IsLowercaseAlphanumeric(c);
        if (!most_recent_component_started_alphanumeric && (c != '-') &&
            (c != '_')) {
          LOG(ERROR) << hostname << " is not in canonicalized form";
          return false;
        }
        in_component = true;
      } else if (c == '.') {
        in_component = false;
      } else if (!IsLowercaseAlphanumeric(c) && (c != '-') && (c != '_')) {
        LOG(ERROR) << hostname << " is not in canonicalized form";
        return false;
      }
    }

    if (!most_recent_component_started_alphanumeric) {
      LOG(ERROR) << "The last label of " << hostname
                 << " must start with a lowercase alphanumeric character";
      return false;
    }

    if (!in_component) {
      LOG(ERROR) << hostname << " must not end with a \".\"";
      return false;
    }
  }

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  crypto::EnsureOpenSSLInit();

  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if defined(OS_WIN)
  std::vector<std::string> args;
  base::CommandLine::StringVector wide_args = command_line.GetArgs();
  for (const auto& arg : wide_args) {
    args.push_back(base::UTF16ToUTF8(arg));
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 4U) {
    PrintHelp();
    return 1;
  }

  base::FilePath json_filepath = base::FilePath::FromUTF8Unsafe(argv[1]);
  if (!base::PathExists(json_filepath)) {
    LOG(ERROR) << "Input JSON file doesn't exist.";
    return 1;
  }
  json_filepath = base::MakeAbsoluteFilePath(json_filepath);

  std::string json_input;
  if (!base::ReadFileToString(json_filepath, &json_input)) {
    LOG(ERROR) << "Could not read input JSON file.";
    return 1;
  }

  base::FilePath pins_filepath = base::FilePath::FromUTF8Unsafe(argv[2]);
  if (!base::PathExists(pins_filepath)) {
    LOG(ERROR) << "Input pins file doesn't exist.";
    return 1;
  }
  pins_filepath = base::MakeAbsoluteFilePath(pins_filepath);

  std::string certs_input;
  if (!base::ReadFileToString(pins_filepath, &certs_input)) {
    LOG(ERROR) << "Could not read input pins file.";
    return 1;
  }

  TransportSecurityStateEntries entries;
  Pinsets pinsets;

  if (!ParseCertificatesFile(certs_input, &pinsets) ||
      !ParseJSON(json_input, &entries, &pinsets)) {
    LOG(ERROR) << "Error while parsing the input files.";
    return 1;
  }

  if (!CheckDuplicateEntries(entries) || !CheckNoopEntries(entries) ||
      !CheckSubdomainsFlags(entries) || !CheckForDuplicatePins(pinsets) ||
      !CheckCertificatesInPinsets(pinsets) || !CheckHostnames(entries)) {
    LOG(ERROR) << "Checks failed. Aborting.";
    return 1;
  }

  base::FilePath template_path = base::FilePath::FromUTF8Unsafe(argv[3]);
  if (!base::PathExists(template_path)) {
    LOG(ERROR) << "Template file doesn't exist.";
    return 1;
  }
  template_path = base::MakeAbsoluteFilePath(template_path);

  std::string preload_template;
  if (!base::ReadFileToString(template_path, &preload_template)) {
    LOG(ERROR) << "Could not read template file.";
    return 1;
  }

  std::string output;
  PreloadedStateGenerator generator;
  output = generator.Generate(preload_template, entries, pinsets);
  if (output.empty()) {
    LOG(ERROR) << "Trie generation failed.";
    return 1;
  }

  base::FilePath output_path;
  output_path = base::FilePath::FromUTF8Unsafe(argv[4]);

  if (base::WriteFile(output_path, output.c_str(),
                      static_cast<uint32_t>(output.size())) <= 0) {
    LOG(ERROR) << "Failed to write output.";
    return 1;
  }

  VLOG(1) << "Wrote trie containing " << entries.size()
          << " entries, referencing " << pinsets.size() << " pinsets to "
          << output_path.AsUTF8Unsafe() << std::endl;

  return 0;
}
