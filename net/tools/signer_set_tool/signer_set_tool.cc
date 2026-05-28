// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "crypto/hash.h"
#include "net/cert/root_store_proto_full/signer_set.pb.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

using chrome_root_store::SignerSet;

namespace {

// Returns a map from SHA-256 hash (bytes) to DER key, or
// `std::nullopt` if error.
std::optional<std::map<std::vector<uint8_t>, std::vector<uint8_t>>> DecodeKeys(
    std::string_view in) {
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(in.data(), in.size()));
  if (!bio) {
    return std::nullopt;
  }
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> keys;
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

    std::string_view name_view(name);
    if (name_view != "PUBLIC KEY") {
      LOG(ERROR) << "Found PEM block of type " << name
                 << " instead of PUBLIC KEY";
      return std::nullopt;
    }

    // SAFETY: This is code used at compile time to generate a .cc file that is
    // included, and is not used at runtime.
    // len is returned from PEM_read_bio and guaranteed to be the size of data.
    UNSAFE_BUFFERS(base::span<const uint8_t> key_span(
        data, base::checked_cast<size_t>(len)));
    auto sha256_array = crypto::hash::Sha256(key_span);
    std::vector<uint8_t> sha256 = base::ToVector(sha256_array);
    keys[sha256] = base::ToVector(key_span);
  }
  return std::move(keys);
}

void WriteByteArrayConstant(std::string_view name,
                            base::span<const uint8_t> data,
                            std::string* string_to_write) {
  base::StringAppendF(string_to_write, "constexpr uint8_t %s[] = {",
                      name.data());

  for (auto c : data) {
    base::StringAppendF(string_to_write, "0x%02xu,", c);
  }

  *string_to_write += "};\n";
}

bool WriteCppFile(
    const SignerSet& signer_set,
    const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& keys,
    const base::FilePath& cpp_path) {
  std::vector<uint8_t> serialized(signer_set.ByteSizeLong());
  if (!signer_set.SerializeToArray(serialized.data(), serialized.size())) {
    LOG(ERROR) << "Error serializing SignerSet proto";
    return false;
  }

  std::string string_to_write =
      "// This file is auto-generated, DO NOT EDIT.\n\n";

  WriteByteArrayConstant("kSignerSetProtoBytes", serialized, &string_to_write);
  string_to_write +=
      "\nconstexpr base::span<const uint8_t> kSignerSetProto = "
      "kSignerSetProtoBytes;\n\n";

  int index = 0;
  std::vector<std::string> key_names;
  std::vector<std::string> hash_names;
  for (const auto& [hash, key] : keys) {
    std::string key_name = base::StringPrintf("kKey_%d", index);
    std::string hash_name = base::StringPrintf("kHash_%d", index);
    WriteByteArrayConstant(key_name, key, &string_to_write);
    WriteByteArrayConstant(hash_name, hash, &string_to_write);
    key_names.push_back(key_name);
    hash_names.push_back(hash_name);
    index++;
  }
  string_to_write += "\n";

  string_to_write +=
      "constexpr auto kSignerKeys = base::MakeFixedFlatMap<base::span<const "
      "uint8_t>, base::span<const uint8_t>>({\n";

  for (size_t i = 0; i < keys.size(); ++i) {
    string_to_write += "  {";
    string_to_write += hash_names[i];
    string_to_write += ", ";
    string_to_write += key_names[i];
    string_to_write += "},\n";
  }
  string_to_write += "});\n";

  if (!base::WriteFile(cpp_path, string_to_write)) {
    PLOG(ERROR) << "Error writing C++ include file";
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
  base::FilePath signer_set_path =
      command_line.GetSwitchValuePath("signer-set");
  base::FilePath keys_path = command_line.GetSwitchValuePath("keys");
  base::FilePath cpp_path = command_line.GetSwitchValuePath("write-cpp");

  if (signer_set_path.empty() || keys_path.empty() || cpp_path.empty() ||
      command_line.HasSwitch("help")) {
    std::cerr << "Usage: signer_set_tool "
              << "--signer-set=TEXTPROTO_FILE "
              << "--keys=PEM_FILE "
              << "--write-cpp=CPP_FILE" << std::endl;
    return 1;
  }

  std::string signer_set_text;
  if (!base::ReadFileToString(base::MakeAbsoluteFilePath(signer_set_path),
                              &signer_set_text)) {
    LOG(ERROR) << "Could not read " << signer_set_path;
    return 1;
  }

  SignerSet signer_set;
  if (!google::protobuf::TextFormat::ParseFromString(signer_set_text,
                                                     &signer_set)) {
    LOG(ERROR) << "Could not parse " << signer_set_path;
    return 1;
  }

  std::string keys_data;
  if (!base::ReadFileToString(base::MakeAbsoluteFilePath(keys_path),
                              &keys_data)) {
    LOG(ERROR) << "Could not read " << keys_path;
    return 1;
  }

  auto keys_opt = DecodeKeys(keys_data);
  if (!keys_opt) {
    LOG(ERROR) << "Could not decode " << keys_path;
    return 1;
  }
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> keys =
      std::move(*keys_opt);

  if (!WriteCppFile(signer_set, keys, cpp_path)) {
    return 1;
  }

  return 0;
}
