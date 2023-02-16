// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/api_resource.h"

namespace extensions {

ApiResource::ApiResource(const std::string& owner_extension_id)
    : owner_extension_id_(owner_extension_id) {

  CHECK(!owner_extension_id_.empty());
}

ApiResource::~ApiResource() = default;

bool ApiResource::IsPersistent() const {
  return true;  // backward-compatible behavior.
}

}  // namespace extensions
