// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

TEST(ExportedCanvasResourceTest, UnacceleratedBitmapPreservesSurfaceOrigin) {
  base::test::TaskEnvironment task_environment;
  auto sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();

  auto make_resource = [&](GrSurfaceOrigin origin) {
    auto si = sii->CreateSharedImageForSoftwareCompositor(
        {viz::SinglePlaneFormat::kBGRA_8888, gfx::Size(4, 4),
         gfx::ColorSpace::CreateSRGB(), origin, kPremul_SkAlphaType,
         gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, "Test"});
    return base::MakeRefCounted<ExportedCanvasResource>(
        std::move(si), gpu::SyncToken(),
        viz::TransferableResource::ResourceSource::kTest, gfx::HDRMetadata(),
        /*provider=*/nullptr, /*accelerated_raster=*/false,
        /*wait_sync_token=*/base::NullCallback(),
        /*release_callback=*/base::DoNothing(),
        /*use_legacy_external_canvas_resource_bitmap_path=*/false);
  };

  auto top_left_bitmap = make_resource(kTopLeft_GrSurfaceOrigin)->Bitmap();
  ASSERT_NE(top_left_bitmap, nullptr);
  EXPECT_EQ(ImageOrientationEnum::kOriginTopLeft,
            top_left_bitmap->Orientation().Orientation());

  auto bottom_left_bitmap =
      make_resource(kBottomLeft_GrSurfaceOrigin)->Bitmap();
  ASSERT_NE(bottom_left_bitmap, nullptr);
  EXPECT_EQ(ImageOrientationEnum::kOriginBottomLeft,
            bottom_left_bitmap->Orientation().Orientation());
}

}  // namespace blink
