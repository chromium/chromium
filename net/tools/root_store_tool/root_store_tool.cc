// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <inttypes.h>

#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/cert/root_store_proto_full/root_store.pb.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

using chrome_root_store::RootStore;

namespace {

// Returns a map from hex-encoded SHA-256 hash to DER certificate, or
// `std::nullopt` if not found.
std::optional<std::map<std::string, std::string>> DecodeCerts(
    std::string_view in) {
  // TODO(crbug.com/40770548): net/cert/pem.h has a much nicer API, but
  // it would require some build refactoring to avoid a circular dependency.
  // This is assuming that the chrome trust store code goes in
  // net/cert/internal, which it may not.
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(in.data(), in.size()));
  if (!bio) {
    return std::nullopt;
  }
  std::map<std::string, std::string> certs;
  for (;;) {
    char* name;
    char* header;
    unsigned char* data;
    long len;
    if (!PEM_read_bio(bio.get(), &name, &header, &data, &len)) {
      uint32_t err = ERR_get_error();
      if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
          ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
        // Found the last PEM block.
        break;
      }
      LOG(ERROR) << "Error reading PEM.";
      return std::nullopt;
    }
    bssl::UniquePtr<char> scoped_name(name);
    bssl::UniquePtr<char> scoped_header(header);
    bssl::UniquePtr<unsigned char> scoped_data(data);
    if (strcmp(name, "CERTIFICATE") != 0) {
      LOG(ERROR) << "Found PEM block of type " << name
                 << " instead of CERTIFICATE";
      return std::nullopt;
    }
    std::string sha256_hex =
        base::ToLowerASCII(base::HexEncode(crypto::SHA256Hash(
            base::make_span(data, base::checked_cast<size_t>(len)))));
    certs[sha256_hex] = std::string(data, data + len);
  }
  return std::move(certs);
}

std::optional<RootStore> ReadTextRootStore(
    const base::FilePath& root_store_path,
    const base::FilePath& certs_path) {
  std::string root_store_text;
  if (!base::ReadFileToString(base::MakeAbsoluteFilePath(root_store_path),
                              &root_store_text)) {
    LOG(ERROR) << "Could not read " << root_store_path;
    return std::nullopt;
  }

  RootStore root_store;
  if (!google::protobuf::TextFormat::ParseFromString(root_store_text,
                                                     &root_store)) {
    LOG(ERROR) << "Could not parse " << root_store_path;
    return std::nullopt;
  }

  std::map<std::string, std::string> certs;
  if (!certs_path.empty()) {
    std::string certs_data;
    if (!base::ReadFileToString(base::MakeAbsoluteFilePath(certs_path),
                                &certs_data)) {
      LOG(ERROR) << "Could not read " << certs_path;
      return std::nullopt;
    }
    auto certs_opt = DecodeCerts(certs_data);
    if (!certs_opt) {
      LOG(ERROR) << "Could not decode " << certs_path;
      return std::nullopt;
    }
    certs = std::move(*certs_opt);
  }

  // Replace the filenames with the actual certificate contents.
  for (auto& anchor : *root_store.mutable_trust_anchors()) {
    if (anchor.certificate_case() !=
        chrome_root_store::TrustAnchor::kSha256Hex) {
      continue;
    }

    auto iter = certs.find(anchor.sha256_hex());
    if (iter == certs.end()) {
      LOG(ERROR) << "Could not find certificate " << anchor.sha256_hex();
      return std::nullopt;
    }

    // Remove the certificate from `certs`. This both checks for duplicate
    // certificates and allows us to check for unused certificates later.
    anchor.set_der(std::move(iter->second));
    certs.erase(iter);
  }

  if (!certs.empty()) {
    LOG(ERROR) << "Unused certificate (SHA-256 hash " << certs.begin()->first
               << ") in " << certs_path;
    return std::nullopt;
  }

  return std::move(root_store);
}

std::string SecondsFromEpochToBaseTime(int64_t t) {
  return base::StrCat({"base::Time::UnixEpoch() + base::Seconds(",
                       base::NumberToString(t), ")"});
}

std::string VersionFromString(std::string_view version_str) {
  return base::StrCat({"\"", version_str, "\""});
}

// Returns true if file was correctly written, false otherwise.
bool WriteRootCppFile(const RootStore& root_store,
                      const base::FilePath cpp_path) {
  // Root store should have at least one trust anchors.
  CHECK_GT(root_store.trust_anchors_size(), 0);

  const std::string kNulloptString = "std::nullopt";

  std::string string_to_write =
      "// This file is auto-generated, DO NOT EDIT.\n\n";

  for (int i = 0; i < root_store.trust_anchors_size(); i++) {
    const auto& anchor = root_store.trust_anchors(i);
    // Every trust anchor at this point should have a DER.
    CHECK(!anchor.der().empty());
    std::string der = anchor.der();

    base::StringAppendF(&string_to_write,
                        "constexpr uint8_t kChromeRootCert%d[] = {", i);

    // Convert each character to hex representation, escaped.
    for (auto c : der) {
      base::StringAppendF(&string_to_write, "0x%02xu,",
                          static_cast<uint8_t>(c));
    }

    // End struct
    string_to_write += "};\n";

    if (anchor.constraints_size() > 0) {
      base::StringAppendF(&string_to_write,
                          "constexpr StaticChromeRootCertConstraints "
                          "kChromeRootConstraints%d[] = {",
                          i);

      std::vector<std::string> constraint_strings;
      for (const auto& constraint : anchor.constraints()) {
        std::vector<std::string> constraint_params;

        constraint_params.push_back(
            constraint.has_sct_not_after_sec()
                ? SecondsFromEpochToBaseTime(constraint.sct_not_after_sec())
                : kNulloptString);

        constraint_params.push_back(
            constraint.has_sct_all_after_sec()
                ? SecondsFromEpochToBaseTime(constraint.sct_all_after_sec())
                : kNulloptString);

        constraint_params.push_back(
            constraint.has_min_version()
                ? VersionFromString(constraint.min_version())
                : kNulloptString);

        constraint_params.push_back(
            constraint.has_max_version_exclusive()
                ? VersionFromString(constraint.max_version_exclusive())
                : kNulloptString);

        constraint_strings.push_back(
            base::StrCat({"{", base::JoinString(constraint_params, ","), "}"}));
      }

      string_to_write += base::JoinString(constraint_strings, ",");
      string_to_write += "};\n";
    }
  }

  string_to_write += "constexpr ChromeRootCertInfo kChromeRootCertList[] = {\n";

  for (int i = 0; i < root_store.trust_anchors_size(); i++) {
    const auto& anchor = root_store.trust_anchors(i);
    base::StringAppendF(&string_to_write, "    {kChromeRootCert%d, ", i);
    if (anchor.constraints_size() > 0) {
      base::StringAppendF(&string_to_write, "kChromeRootConstraints%d", i);
    } else {
      string_to_write += "{}";
    }
    string_to_write += "},\n";
  }
  string_to_write += "};";

  base::StringAppendF(&string_to_write,
                      "\n\n\nstatic const int64_t kRootStoreVersion = %" PRId64
                      ";\n",
                      root_store.version_major());
  if (!base::WriteFile(cpp_path, string_to_write)) {
    return false;
  }
  return true;
}

// Returns true if file was correctly written, false otherwise.
bool WriteEvCppFile(const RootStore& root_store,
                    const base::FilePath cpp_path) {
  // There should be at least one EV root.
  CHECK_GT(root_store.trust_anchors_size(), 0);

  std::string string_to_write =
      "// This file is auto-generated, DO NOT EDIT.\n\n"
      "static const EVMetadata kEvRootCaMetadata[] = {\n";

  for (auto& anchor : root_store.trust_anchors()) {
    // Every trust anchor at this point should have a DER.
    CHECK(!anchor.der().empty());
    if (anchor.ev_policy_oids_size() == 0) {
      // The same input file is used for the Chrome Root Store and EV enabled
      // certificates. Skip anchors that have no EV policy OIDs when generating
      // the EV include file.
      continue;
    }

    std::string sha256_hash = crypto::SHA256HashString(anchor.der());

    // Begin struct. Assumed type of EVMetadata:
    //
    // struct EVMetadata {
    //  static const size_t kMaxOIDsPerCA = 2;
    //  SHA256HashValue fingerprint;
    //  const std::string_view policy_oids[kMaxOIDsPerCA];
    // };
    string_to_write += "    {\n";
    string_to_write += "        {{";

    int wrap_count = 0;
    for (auto c : sha256_hash) {
      if (wrap_count != 0) {
        if (wrap_count % 11 == 0) {
          string_to_write += ",\n          ";
        } else {
          string_to_write += ", ";
        }
      }
      base::StringAppendF(&string_to_write, "0x%02x", static_cast<uint8_t>(c));
      wrap_count++;
    }

    string_to_write += "}},\n";
    string_to_write += "        {\n";

    // struct expects exactly two policy oids, and we can only support 1 or 2
    // policy OIDs. These checks will need to change if we ever merge the EV and
    // Chrome Root Store textprotos.
    const int kMaxPolicyOids = 2;
    int oids_size = anchor.ev_policy_oids_size();
    std::string hexencode_hash = base::HexEncode(sha256_hash);
    if (oids_size > kMaxPolicyOids) {
      PLOG(ERROR) << hexencode_hash << " has too many OIDs!";
      return false;
    }
    for (int i = 0; i < kMaxPolicyOids; i++) {
      std::string oid;
      if (i < oids_size) {
        oid = anchor.ev_policy_oids(i);
      }
      string_to_write += "            \"" + oid + "\",\n";
    }

    // End struct
    string_to_write += "        },\n";
    string_to_write += "    },\n";
  }
  string_to_write += "};\n";
  if (!base::WriteFile(cpp_path, string_to_write)) {
    PLOG(ERROR) << "Error writing cpp include file";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath proto_path = command_line.GetSwitchValuePath("write-proto");
  base::FilePath root_store_cpp_path =
      command_line.GetSwitchValuePath("write-cpp-root-store");
  base::FilePath ev_roots_cpp_path =
      command_line.GetSwitchValuePath("write-cpp-ev-roots");
  base::FilePath root_store_path =
      command_line.GetSwitchValuePath("root-store");
  base::FilePath certs_path = command_line.GetSwitchValuePath("certs");

  if ((proto_path.empty() && root_store_cpp_path.empty() &&
       ev_roots_cpp_path.empty()) ||
      root_store_path.empty() || command_line.HasSwitch("help")) {
    std::cerr << "Usage: root_store_tool "
              << "--root-store=TEXTPROTO_FILE "
              << "[--certs=CERTS_FILE] "
              << "[--write-proto=PROTO_FILE] "
              << "[--write-cpp-root-store=CPP_FILE] "
              << "[--write-cpp-ev-roots=CPP_FILE] " << std::endl;
    return 1;
  }

  std::optional<RootStore> root_store =
      ReadTextRootStore(root_store_path, certs_path);
  if (!root_store) {
    return 1;
  }

  // TODO(crbug.com/40770548): Figure out how to use the serialized
  // proto to support component update.
  // components/resources/ssl/ssl_error_assistant/push_proto.py
  // does it through a GCS bucket (I think) so that might be an option.
  if (!proto_path.empty()) {
    std::string serialized;
    if (!root_store->SerializeToString(&serialized)) {
      LOG(ERROR) << "Error serializing root store proto"
                 << root_store->DebugString();
      return 1;
    }
    if (!base::WriteFile(proto_path, serialized)) {
      PLOG(ERROR) << "Error writing serialized proto root store";
      return 1;
    }
  }

  if (!root_store_cpp_path.empty() &&
      !WriteRootCppFile(*root_store, root_store_cpp_path)) {
    PLOG(ERROR) << "Error writing root store C++ include file";
    return 1;
  }
  if (!ev_roots_cpp_path.empty() &&
      !WriteEvCppFile(*root_store, ev_roots_cpp_path)) {
    PLOG(ERROR) << "Error writing EV roots C++ include file";
    return 1;
  }

  return 0;
}
