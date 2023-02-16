// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/hashed_extension_id.h"

#include "components/crx_file/id_util.h"

namespace extensions {

HashedExtensionId::HashedExtensionId() = default;

HashedExtensionId::HashedExtensionId(const ExtensionId& original_id)
    : value_(crx_file::id_util::HashedIdInHex(original_id)) {}

HashedExtensionId::HashedExtensionId(HashedExtensionId&& other) = default;
HashedExtensionId::HashedExtensionId(const HashedExtensionId& other) = default;
HashedExtensionId& HashedExtensionId::operator=(HashedExtensionId&& other) =
    default;
HashedExtensionId& HashedExtensionId::operator=(
    const HashedExtensionId& other) = default;

}  // namespace extensions
