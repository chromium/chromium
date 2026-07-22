// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/privacy_sandbox_coordinator_test_util.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/strings/string_view_util.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

std::string CreatePrivacySandboxCoordinatorSerializedPublicKeys(
    const url::Origin& coordinator,
    base::span<const url::Origin> origins) {
  base::DictValue origin_scoped_keys;
  for (const auto& origin : origins) {
    base::DictValue key;
    key.Set("key", base::Base64Encode(kTestPrivacySandboxCoordinatorPublicKey));
    key.Set("id", kTestPrivacySandboxCoordinatorIdString);
    base::ListValue keys;
    keys.Append(std::move(key));

    base::DictValue origin_dict;
    origin_dict.Set("keys", std::move(keys));

    origin_scoped_keys.Set(origin.Serialize(), std::move(origin_dict));
  }

  base::DictValue coordinator_response;
  coordinator_response.Set("originScopedKeys", std::move(origin_scoped_keys));

  std::optional<std::string> serialized_response =
      base::WriteJson(coordinator_response);
  CHECK(serialized_response);
  return std::move(serialized_response).value();
}


std::string GetTestPrivacySandboxCoordinatorPrivateKey() {
  return std::string(
      base::as_string_view(kTestPrivacySandboxCoordinatorPrivateKey));
}

}  // namespace content
