// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Internal helpers for converting between MojoSharedBufferGuid and
// base::UnguessableToken. Helpful so base::UnguessableToken can grant access
// to its internal serialization/deserialization helpers.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_INTERNAL_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_INTERNAL_H_

#include "base/unguessable_token.h"
#include "mojo/public/c/system/platform_handle.h"

namespace mojo {
namespace internal {

class PlatformHandleInternal {
 public:
  static MojoSharedBufferGuid MarshalUnguessableToken(
      const base::UnguessableToken& token) {
    return {.high = token.GetHighForSerialization(),
            .low = token.GetLowForSerialization()};
  }
  static std::optional<base::UnguessableToken> UnmarshalUnguessableToken(
      const MojoSharedBufferGuid* guid) {
    return base::UnguessableToken::Deserialize(guid->high, guid->low);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_INTERNAL_H_
