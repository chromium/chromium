// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_base.h"

#include "base/check_op.h"

namespace gpu {

TextureBase::TextureBase(unsigned int service_id)
    : service_id_(service_id), target_(0) {}

TextureBase::~TextureBase() = default;

void TextureBase::SetTarget(unsigned int target) {
  DCHECK_EQ(0u, target_);  // you can only set this once.
  target_ = target;
}

TextureBase::Type TextureBase::GetType() const {
  return TextureBase::Type::kNone;
}

}  // namespace gpu
