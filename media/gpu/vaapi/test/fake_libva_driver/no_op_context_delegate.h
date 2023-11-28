// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_NO_OP_CONTEXT_DELEGATE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_NO_OP_CONTEXT_DELEGATE_H_

#include "media/gpu/vaapi/test/fake_libva_driver/context_delegate.h"

namespace media::internal {

// A ContextDelegate that doesn't do anything. Intended as a fake
// ContextDelegate for unit testing purposes.
class NoOpContextDelegate : public ContextDelegate {
 public:
  NoOpContextDelegate() = default;
  NoOpContextDelegate(const NoOpContextDelegate&) = delete;
  NoOpContextDelegate& operator=(const NoOpContextDelegate&) = delete;
  ~NoOpContextDelegate() override = default;

  // ContextDelegate implementation.
  void SetRenderTarget(const FakeSurface& surface) override;
  void EnqueueWork(
      const std::vector<raw_ptr<const FakeBuffer>>& buffers) override;
  void Run() override;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_NO_OP_CONTEXT_DELEGATE_H_
