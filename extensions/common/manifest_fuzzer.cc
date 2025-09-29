// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_test_suite_base.h"
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

class FakeContentClient : public content::ContentClient {
 public:
  FakeContentClient() = default;
  FakeContentClient(const FakeContentClient&) = delete;
  FakeContentClient& operator=(const FakeContentClient&) = delete;
  ~FakeContentClient() override = default;
};

// Holds state shared across all fuzzer calls. The base class supports
// registering URL schemes required to load manifest features.
struct Environment : public content::ContentTestSuiteBase {
  Environment() : ContentTestSuiteBase(0, nullptr) {
    RegisterContentSchemes(&content_client);
    extensions_client = std::make_unique<TestExtensionsClient>();
    ExtensionsClient::Set(extensions_client.get());
  }

  // Singleton objects needed for the tested code.
  base::AtExitManager at_exit;
  FakeContentClient content_client;
  // This must be created after content schemes are registered.
  std::unique_ptr<TestExtensionsClient> extensions_client;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  if (size > kMaxInputSizeBytes) {
    return 0;
  }
  FuzzedDataProvider fuzzed_data_provider(data, size);

  std::string extension_id = fuzzed_data_provider.ConsumeRandomLengthString();
  if (extension_id.empty())
    extension_id.resize(1);

  std::optional<base::Value> parsed_json = base::JSONReader::Read(
      fuzzed_data_provider.ConsumeRemainingBytesAsString(),
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
