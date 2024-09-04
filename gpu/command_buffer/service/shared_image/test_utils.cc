// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/test_utils.h"

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

std::vector<uint8_t> ReadPixels(
    Mailbox mailbox,
    gfx::Size size,
    SharedContextState* context_state,
    SharedImageRepresentationFactory* representation_factory) {
  DCHECK(context_state);
  EXPECT_TRUE(
      context_state->MakeCurrent(context_state->surface(), true /* needs_gl*/));
  auto skia_representation =
      representation_factory->ProduceSkia(mailbox, context_state);
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  EXPECT_TRUE(promise_texture);
  GrBackendTexture backend_texture = promise_texture->backendTexture();
  EXPECT_TRUE(backend_texture.isValid());
  EXPECT_EQ(size.width(), backend_texture.width());
  EXPECT_EQ(size.height(), backend_texture.height());

  // Create an Sk Image from GrBackendTexture.
  auto sk_image = SkImages::BorrowTextureFrom(
      context_state->gr_context(), promise_texture->backendTexture(),
      kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kOpaque_SkAlphaType,
      nullptr);

  SkImageInfo dst_info =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        kOpaque_SkAlphaType, nullptr);

  const int num_pixels = size.width() * size.height();
  std::vector<uint8_t> dst_pixels(num_pixels * 4);

  // Read back pixels from Sk Image.
  EXPECT_TRUE(sk_image->readPixels(dst_info, dst_pixels.data(),
                                   dst_info.minRowBytes(), 0, 0));
  scoped_read_access.reset();

  return dst_pixels;
}

}  // namespace gpu
