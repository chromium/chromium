// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_AV1_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_AV1_DECODER_DELEGATE_H_

#include <va/va.h>

#include "media/gpu/vaapi/test/fake_libva_driver/context_delegate.h"

struct Dav1dContext;

namespace media::internal {

// Class used for libdav1d software decoding.
class Av1DecoderDelegate : public ContextDelegate {
 public:
  explicit Av1DecoderDelegate(VAProfile profile);
  Av1DecoderDelegate(const Av1DecoderDelegate&) = delete;
  Av1DecoderDelegate& operator=(const Av1DecoderDelegate&) = delete;
  ~Av1DecoderDelegate() override;

  // ContextDelegate implementation.
  void SetRenderTarget(const FakeSurface& surface) override;
  void EnqueueWork(
      const std::vector<raw_ptr<const FakeBuffer>>& buffers) override;
  void Run() override;

 private:
  struct Dav1dContextDeleter {
    void operator()(Dav1dContext* ptr);
  };
  raw_ptr<const FakeBuffer> encoded_data_buffer_{nullptr};
  raw_ptr<const FakeSurface> render_target_{nullptr};
  std::unique_ptr<Dav1dContext, Dav1dContextDeleter> dav1d_context_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_AV1_DECODER_DELEGATE_H_
