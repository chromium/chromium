// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_pool_id.h"

namespace gpu {

PoolId::PoolId() = default;

PoolId::PoolId(const base::UnguessableToken& token) : token_(token) {}

PoolId PoolId::Create() {
  return PoolId(base::UnguessableToken::Create());
}

std::string PoolId::ToString() const {
  return token_.ToString();
}

}  // namespace gpu
