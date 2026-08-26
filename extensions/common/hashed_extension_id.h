// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_
#define EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {

// A wrapper around a hex-encoded hash (SHA-1 or SHA-256) of an extension ID.
// This struct is primarily to enforce type-safety, but also offers handy
// construction. The hashed ID of an extension is used to determine feature
// availability.
class HashedExtensionId {
 public:
  // Default constructor to initialize with an empty value. It'd be nice to get
  // rid of this, but certain objects (like Manifest) don't have a valid ID at
  // construction.
  HashedExtensionId();

  // Initialize a HashedExtensionId, given the original.
  explicit HashedExtensionId(const ExtensionId& original_id);

  HashedExtensionId(HashedExtensionId&& other);
  HashedExtensionId(const HashedExtensionId& other);
  HashedExtensionId& operator=(HashedExtensionId&& other);
  HashedExtensionId& operator=(const HashedExtensionId& other);

  const std::string& value() const {
    return use_sha256_ ? value_sha256_ : value_sha1_;
  }
  // TODO(crbug.com/455599844): Remove `value_sha1()` and `value_sha256()` once
  // the SHA-256 rollout is 100% complete and default, making `value()` the
  // only getter.
  const std::string& value_sha1() const { return value_sha1_; }
  const std::string& value_sha256() const { return value_sha256_; }

 private:
  // Not const to allow for copy and move assignment operators, ensuring
  // the class behaves as a standard value type.
  // TODO(crbug.com/455599844): Remove `value_sha1_` and `use_sha256_` once the
  // SHA-256 rollout is 100% complete and default.
  std::string value_sha1_;
  std::string value_sha256_;
  bool use_sha256_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_
