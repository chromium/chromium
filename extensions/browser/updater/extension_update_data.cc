// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_update_data.h"

namespace extensions {

ExtensionUpdateData::ExtensionUpdateData() : is_corrupt_reinstall(false) {}

ExtensionUpdateData::ExtensionUpdateData(const ExtensionUpdateData& other) =
    default;

ExtensionUpdateData::~ExtensionUpdateData() = default;

ExtensionUpdateCheckParams::ExtensionUpdateCheckParams()
    : priority(BACKGROUND), install_immediately(false) {}

ExtensionUpdateCheckParams::ExtensionUpdateCheckParams(
    const ExtensionUpdateCheckParams& other) = default;

ExtensionUpdateCheckParams::~ExtensionUpdateCheckParams() = default;

}  // namespace extensions
