// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/hashed_extension_id.h"

#include "base/feature_list.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_features.h"

namespace extensions {

HashedExtensionId::HashedExtensionId() = default;

// TODO(crbug.com/455599844): Remove the dual-hash initialization and the
// fallback to `value_sha1_` once the SHA-256 rollout is 100% complete.
HashedExtensionId::HashedExtensionId(const ExtensionId& original_id)
    : value_sha1_(crx_file::id_util::HashedIdInHex(original_id)),
      value_sha256_(crx_file::id_util::HashedIdInHexSha256(original_id)),
      use_sha256_(base::FeatureList::GetInstance() &&
                  base::FeatureList::IsEnabled(
                      extensions_features::kUseSha256ForExtensionHashes)) {}

HashedExtensionId::HashedExtensionId(HashedExtensionId&& other) = default;
HashedExtensionId::HashedExtensionId(const HashedExtensionId& other) = default;
HashedExtensionId& HashedExtensionId::operator=(HashedExtensionId&& other) =
    default;
HashedExtensionId& HashedExtensionId::operator=(
    const HashedExtensionId& other) = default;

}  // namespace extensions
