// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_
#define EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {

// A wrapper around a hex-encoded SHA1 hash of an extension ID. This struct is
// primarily to enforce type-safety, but also offers handy construction. The
// hashed ID of an extension is used to determine feature availability.
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

  const std::string& value() const { return value_; }

 private:
  // Not const to allow for assignment.
  std::string value_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_HASHED_EXTENSION_ID_H_
