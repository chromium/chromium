// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/no_op_context_delegate.h"

namespace media::internal {

void NoOpContextDelegate::SetRenderTarget(const FakeSurface& surface) {}

void NoOpContextDelegate::EnqueueWork(
    const std::vector<raw_ptr<const FakeBuffer>>& buffers) {}

void NoOpContextDelegate::Run() {}

}  // namespace media::internal
