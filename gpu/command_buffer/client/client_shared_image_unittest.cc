// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// TODO(blundell): Set up SharedImageInterface in these tests and test creation
// paths that pass in SIIHolder.
TEST(ClientSharedImageTest, ImportUnowned) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  auto client_si = ClientSharedImage::ImportUnowned(
      ExportedSharedImage(mailbox, metadata, SyncToken(), GL_TEXTURE_2D));

  // Check that the ClientSI's state matches the input parameters.
  EXPECT_TRUE(client_si->mailbox() == mailbox);
  EXPECT_EQ(client_si->format(), kFormat);
  EXPECT_EQ(client_si->size(), kSize);
  EXPECT_EQ(client_si->usage(), kUsage);
  EXPECT_EQ(client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
  EXPECT_FALSE(client_si->HasHolder());
}

}  // namespace gpu
