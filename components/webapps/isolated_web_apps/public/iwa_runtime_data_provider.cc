// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

#include "base/base64.h"

namespace web_app {

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    PublicKeyData public_key,
    std::optional<PublicKeyData> previous_key)
    : public_key(std::move(public_key)),
      previous_key(std::move(previous_key)) {}

IwaRuntimeDataProvider::KeyRotationInfo::~KeyRotationInfo() = default;

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    const KeyRotationInfo&) = default;

base::Value IwaRuntimeDataProvider::KeyRotationInfo::AsDebugValue() const {
  auto dict =
      base::DictValue().Set("public_key", base::Base64Encode(public_key));
  if (previous_key) {
    dict.Set("previous_key", base::Base64Encode(*previous_key));
  }
  return base::Value(std::move(dict));
}

}  // namespace web_app
