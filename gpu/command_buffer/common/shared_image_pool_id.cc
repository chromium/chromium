// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_pool_id.h"

namespace gpu {

SharedImagePoolId::SharedImagePoolId() = default;

SharedImagePoolId::SharedImagePoolId(const base::UnguessableToken& token)
    : token_(token) {}

SharedImagePoolId SharedImagePoolId::Create() {
  return SharedImagePoolId(base::UnguessableToken::Create());
}

std::string SharedImagePoolId::ToString() const {
  return token_.ToString();
}

}  // namespace gpu
