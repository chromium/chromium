// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_POOL_ID_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_POOL_ID_H_

#include <string>

#include "base/unguessable_token.h"
#include "gpu/command_buffer/common/gpu_command_buffer_common_export.h"

namespace gpu {

// A unique, unguessable identifier for a SharedImagePool.
class GPU_COMMAND_BUFFER_COMMON_EXPORT SharedImagePoolId {
 public:
  SharedImagePoolId();
  explicit SharedImagePoolId(const base::UnguessableToken& token);

  // Creates a new SharedImagePoolId with a cryptographically random value.
  static SharedImagePoolId Create();

  // Generates a string representation of the SharedImagePoolId.
  std::string ToString() const;

  bool operator==(const SharedImagePoolId& other) const {
    return token_ == other.token_;
  }

  std::strong_ordering operator<=>(const SharedImagePoolId& other) const {
    return token_ <=> other.token_;
  }

  template <typename H>
  friend H AbslHashValue(H h, const SharedImagePoolId& id) {
    return H::combine(std::move(h), id.token_);
  }

  bool IsValid() const { return !token_.is_empty(); }

  const base::UnguessableToken& GetToken() const { return token_; }

 private:
  // The underlying unguessable token.
  base::UnguessableToken token_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_POOL_ID_H_
