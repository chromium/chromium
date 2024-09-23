// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extensions_client.h"

namespace extensions {

namespace {

// Bail out on larger inputs to prevent out-of-memory failures.
constexpr int kMaxInputSizeBytes = 200 * 1024;

const mojom::ManifestLocation kLocations[] = {
    mojom::ManifestLocation::kInternal,
    mojom::ManifestLocation::kExternalPref,
    mojom::ManifestLocation::kExternalRegistry,
    mojom::ManifestLocation::kUnpacked,
    mojom::ManifestLocation::kComponent,
    mojom::ManifestLocation::kExternalPrefDownload,
    mojom::ManifestLocation::kExternalPolicyDownload,
    mojom::ManifestLocation::kCommandLine,
    mojom::ManifestLocation::kExternalPolicy,
    mojom::ManifestLocation::kExternalComponent,
};

// Holds state shared across all fuzzer calls.
struct Environment {
  Environment() { ExtensionsClient::Set(&extensions_client); }

  // Singleton objects needed for the tested code.
  base::AtExitManager at_exit;
  TestExtensionsClient extensions_client;
};

bool InitFuzzedCommandLine(FuzzedDataProvider& fuzzed_data_provider) {
  constexpr int kMaxArgvItems = 100;
  const int argc =
      fuzzed_data_provider.ConsumeIntegralInRange<int>(0, kMaxArgvItems);
  std::vector<std::string> argv;
  argv.reserve(argc);
  std::vector<const char*> argv_chars;
  argv_chars.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    argv.push_back(fuzzed_data_provider.ConsumeRandomLengthString());
    argv_chars.push_back(argv.back().c_str());
  }
  return base::CommandLine::Init(argc, argv_chars.data());
}

// Holds state during a single fuzzer call.
struct PerInputEnvironment {
  explicit PerInputEnvironment(FuzzedDataProvider& fuzzed_data_provider) {
    CHECK(InitFuzzedCommandLine(fuzzed_data_provider));
  }

  ~PerInputEnvironment() { base::CommandLine::Reset(); }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  if (size > kMaxInputSizeBytes) {
    return 0;
  }
  FuzzedDataProvider fuzzed_data_provider(data, size);
  PerInputEnvironment per_input_env(fuzzed_data_provider);

  std::string extension_id = fuzzed_data_provider.ConsumeRandomLengthString();
  if (extension_id.empty())
    extension_id.resize(1);

  std::optional<base::Value> parsed_json = base::JSONReader::Read(
      fuzzed_data_provider.ConsumeRemainingBytesAsString());
  if (!parsed_json || !parsed_json->is_dict())
    return 0;

  for (auto location : kLocations) {
    Manifest manifest(location, parsed_json->GetDict().Clone(), extension_id);

    std::vector<InstallWarning> install_warning;
    manifest.ValidateManifest(&install_warning);
  }

  return 0;
}

}  // namespace extensions
