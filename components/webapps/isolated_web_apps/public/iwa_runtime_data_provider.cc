// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

#include "base/base64.h"

namespace web_app {

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    std::optional<PublicKeyData> public_key)
    : public_key(std::move(public_key)) {}

IwaRuntimeDataProvider::KeyRotationInfo::~KeyRotationInfo() = default;

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    const KeyRotationInfo&) = default;

base::Value IwaRuntimeDataProvider::KeyRotationInfo::AsDebugValue() const {
  return base::Value(base::Value::Dict().Set(
      "public_key", public_key ? base::Base64Encode(*public_key) : "null"));
}

}  // namespace web_app
