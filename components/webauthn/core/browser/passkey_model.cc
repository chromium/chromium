// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model.h"

namespace webauthn {

PasskeyModel::UserEntity::UserEntity(std::vector<uint8_t> id,
                                     std::string name,
                                     std::string display_name)
    : id(std::move(id)),
      name(std::move(name)),
      display_name(std::move(display_name)) {}

PasskeyModel::UserEntity::~UserEntity() = default;

}  // namespace webauthn
