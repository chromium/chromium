// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/scripts_internal/script_serialization.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/user_script.h"

using extensions::api::scripts_internal::SerializedUserScript;

namespace extensions {
namespace script_serialization {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Generate input parameters for the call.
  std::optional<base::Value> serialized_script_value =
      base::JSONReader::Read(provider.ConsumeRandomLengthString());
  if (!serialized_script_value.has_value()) {
    return 0;
  }
  std::optional<SerializedUserScript> serialized_script =
      SerializedUserScript::FromValue(std::move(*serialized_script_value));
  if (!serialized_script.has_value()) {
    return 0;
  }
  bool allowed_in_incognito = provider.ConsumeBool();
  SerializedUserScriptParseOptions parse_options;
  parse_options.can_execute_script_everywhere = provider.ConsumeBool();
  parse_options.all_urls_includes_chrome_urls = provider.ConsumeBool();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(base::Value::Dict()).Build();

  // Run code-under-test.
  std::u16string error;
  bool wants_file_access;
  std::unique_ptr<UserScript> user_script = ParseSerializedUserScript(
      *serialized_script, *extension, allowed_in_incognito, &error,
      &wants_file_access, parse_options);
  CHECK_EQ(user_script != nullptr, error.empty());

  return 0;
}

}  // namespace script_serialization
}  // namespace extensions
