// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mock_texture_owner.h"

namespace gpu {

using testing::_;
using testing::Return;

MockTextureOwner::MockTextureOwner() {
  ON_CALL(*this, UpdateTexImage(_)).WillByDefault(Return(true));
  ON_CALL(*this, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });
}

MockTextureOwner::~MockTextureOwner() {}

}  // namespace gpu
