// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/openssl_util.h"
#include "net/tools/root_store_tool/root_store.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

using chrome_root_store::RootStore;

namespace {

absl::optional<std::string> DecodePEM(base::StringPiece pem) {
  // TODO(https://crbug.com/1216547): net/cert/pem.h has a much nicer API, but
  // it would require some build refactoring to avoid a circular dependency.
  // This is assuming that the chrome trust store code goes in
  // net/cert/internal, which it may not.
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(pem.data(), pem.size()));
  if (!bio) {
    return absl::nullopt;
  }
  char* name;
  char* header;
  unsigned char* data;
  long len;
  if (!PEM_read_bio(bio.get(), &name, &header, &data, &len)) {
    LOG(ERROR) << "Could not find PEM block.";
    return absl::nullopt;
  }
  bssl::UniquePtr<char> scoped_name(name);
  bssl::UniquePtr<char> scoped_header(header);
  bssl::UniquePtr<unsigned char> scoped_data(data);
  if (strcmp(name, "CERTIFICATE") != 0) {
    LOG(ERROR) << "Found PEM block of type " << name
               << " instead of CERTIFICATE";
    return absl::nullopt;
  }
  return std::string(data, data + len);
}

absl::optional<RootStore> ReadTextRootStore(
    const base::FilePath& root_store_dir) {
  base::FilePath root_store_path =
      root_store_dir.AppendASCII("root_store.textproto");
  std::string root_store_text;
  if (!base::ReadFileToString(root_store_path, &root_store_text)) {
    LOG(ERROR) << "Could not read " << root_store_path;
    return absl::nullopt;
  }

  RootStore root_store;
  if (!google::protobuf::TextFormat::ParseFromString(root_store_text,
                                                     &root_store)) {
    LOG(ERROR) << "Could not parse " << root_store_path;
    return absl::nullopt;
  }

  // Replace the filenames with the actual certificate contents.
  base::FilePath certs_dir = root_store_dir.AppendASCII("certs");
  for (auto& anchor : *root_store.mutable_trust_anchors()) {
    base::FilePath pem_path = certs_dir.AppendASCII(anchor.filename());

    if (!base::PathExists(pem_path)) {
      LOG(ERROR) << "Error file does not exist: " << pem_path;
      return absl::nullopt;
    }

    if (base::DirectoryExists(pem_path)) {
      LOG(ERROR) << "Error path is a directory: " << pem_path;
      return absl::nullopt;
    }

    if (!base::PathIsReadable(pem_path)) {
      LOG(ERROR) << "Error file is not readable: " << pem_path;
      return absl::nullopt;
    }

    std::string pem;
    if (!base::ReadFileToString(pem_path, &pem)) {
      LOG(ERROR) << "Error reading " << pem_path;
      return absl::nullopt;
    }
    absl::optional<std::string> der = DecodePEM(pem);
    if (!der) {
      LOG(ERROR) << "Error decoding " << pem_path;
      return absl::nullopt;
    }
    anchor.clear_filename();
    anchor.set_der(*der);
  }
  return std::move(root_store);
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  crypto::EnsureOpenSSLInit();

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath proto_path = command_line.GetSwitchValuePath("write-proto");
  base::FilePath cpp_path = command_line.GetSwitchValuePath("write-cpp");
  if ((proto_path.empty() && cpp_path.empty()) ||
      command_line.HasSwitch("help")) {
    std::cerr << "Usage: root_store_tool "
                 "[--root-store-dir=<relative-path>]
                 "[--write-proto=PROTO_FILE] "
                 "[--write-cpp=CPP_FILE]"
              << std::endl;
    return 1;
  }

  // Find root store directory. Assumptions:
  //  - Root store directory is relative to base::DIR_SOURCE_ROOT
  //
  //  - $(ROOT_STORE_DIR)/root_store.textproto contains the textproto definition
  //    of the root store
  //
  //  - Any certificate files referenced in
  //    $(ROOT_STORE_DIR)/root_store.textproto exist in the
  //    $(ROOT_STORE_DIR)/certs/ subdirectory.
  base::FilePath root_store_dir =
      command_line.GetSwitchValuePath("root-store-dir");
  base::FilePath source_root;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
  if (root_store_dir.empty()) {
    root_store_dir = source_root.AppendASCII("net")
                         .AppendASCII("data")
                         .AppendASCII("ssl")
                         .AppendASCII("chrome_root_store")
                         .AppendASCII("base");
  } else {
    root_store_dir = source_root.Append(root_store_dir);
  }
  absl::optional<RootStore> root_store = ReadTextRootStore(root_store_dir);
  if (!root_store) {
    return 1;
  }

  // TODO(https://crbug.com/1216547): Figure out how to use the serialized
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

  if (!cpp_path.empty()) {
    // Root store should have at least one trust anchors.
    CHECK_GT(root_store->trust_anchors_size(), 0);

    std::string string_to_write =
        "// This file is auto-generated, DO NOT EDIT.\n\n"
        "const ChromeRootCertInfo kChromeRootCertList[] = {\n";

    for (auto& anchor : root_store->trust_anchors()) {
      // Every trust anchor at this point should have a DER.
      CHECK(!anchor.der().empty());
      std::string der = anchor.der();

      // Begin struct. Assumed type of ChromeRootCertInfo:
      //
      // struct {
      //   base::span<const uint8_t> der;
      // };
      string_to_write += "    {{{";

      // Convert each character to hex representation, escaped.
      for (auto c : der) {
        base::StringAppendF(&string_to_write, "0x%02xu,",
                            static_cast<uint8_t>(c));
      }

      // End struct
      string_to_write += "}}},\n";
    }
    string_to_write += "};";
    if (!base::WriteFile(cpp_path, string_to_write)) {
      PLOG(ERROR) << "Error writing cpp include file";
    }
  }

  return 0;
}
