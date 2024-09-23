// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include "base/run_loop.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

TEST(CanvasResourceTest, PrepareTransferableResource_SharedBitmap) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();
  auto shared_image_interface_provider =
      test_web_shared_image_interface_provider->GetWeakPtr();

  scoped_refptr<CanvasResource> canvas_resource =
      CanvasResourceSharedBitmap::Create(SkImageInfo::MakeN32Premul(10, 10),
                                         /*CanvasResourceProvider=*/nullptr,
                                         shared_image_interface_provider,
                                         cc::PaintFlags::FilterQuality::kLow);
  EXPECT_TRUE(!!canvas_resource);
  viz::TransferableResource resource;
  CanvasResource::ReleaseCallback release_callback;
  bool success = canvas_resource->PrepareTransferableResource(
      &resource, &release_callback, /*needs_verified_synctoken=*/false);

  EXPECT_TRUE(success);
  EXPECT_TRUE(resource.is_software);

  std::move(release_callback)
      .Run(std::move(canvas_resource), gpu::SyncToken(), false);
}

}  // namespace blink
