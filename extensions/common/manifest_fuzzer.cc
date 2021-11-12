// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extensions_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

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

struct Environment {
  Environment() { ExtensionsClient::Set(&extensions_client); }

  // Singleton objects needed for the tested code.
  base::AtExitManager at_exit;
  TestExtensionsClient extensions_client;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider fuzzed_data_provider(data, size);

  std::string extension_id = fuzzed_data_provider.ConsumeRandomLengthString();
  if (extension_id.empty())
    extension_id.resize(1);

  absl::optional<base::Value> parsed_json = base::JSONReader::Read(
      fuzzed_data_provider.ConsumeRemainingBytesAsString());
  if (!parsed_json || !parsed_json->is_dict())
    return 0;

  for (auto location : kLocations) {
    Manifest manifest(location,
                      base::DictionaryValue::From(
                          base::Value::ToUniquePtrValue(parsed_json->Clone())),
                      extension_id);

    std::string error;
    std::vector<InstallWarning> install_warning;
    manifest.ValidateManifest(&error, &install_warning);
  }

  return 0;
}

}  // namespace extensions
