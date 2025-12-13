// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_resource_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

constexpr gfx::Size kFrameSize(100, 50);

struct TransformTestParams {
  media::VideoTransformation transform;
  gfx::Transform expected_transform;
};

class VideoFrameResourceProviderTest
    : public testing::Test,
      public testing::WithParamInterface<TransformTestParams> {
 public:
  void SetUp() override {
    context_provider_ = viz::TestContextProvider::CreateRaster();
    ASSERT_EQ(context_provider_->BindToCurrentSequence(),
              gpu::ContextResult::kSuccess);

    provider_ = std::make_unique<VideoFrameResourceProvider>(
        cc::LayerTreeSettings(), /*use_sync_primitives=*/false);

    provider_->Initialize(context_provider_.get(),
                          /*shared_image_interface=*/nullptr);
  }

 protected:
  const viz::SharedQuadState* GetFirstSharedState(
      const viz::CompositorRenderPass& render_pass) {
    if (render_pass.shared_quad_state_list.empty()) {
      ADD_FAILURE() << "Shared quad state list is empty";
      return nullptr;
    }
    return render_pass.shared_quad_state_list.front();
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  std::unique_ptr<VideoFrameResourceProvider> provider_;
};

TEST_P(VideoFrameResourceProviderTest, AppendQuadsRotationTransform) {
  const gfx::Size kOutputSize =
      (GetParam().transform.rotation == media::VIDEO_ROTATION_90 ||
       GetParam().transform.rotation == media::VIDEO_ROTATION_270)
          ? gfx::Size(kFrameSize.height(), kFrameSize.width())
          : kFrameSize;
  const viz::CompositorRenderPassId kRenderPassId{1u};

  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, gfx::Rect(kOutputSize), gfx::Rect(),
                      gfx::Transform());

  auto frame = media::VideoFrame::CreateBlackFrame(kFrameSize);
  ASSERT_TRUE(frame);

  provider_->AppendQuads(render_pass.get(), frame, GetParam().transform,
                         /*is_opaque=*/true);

  provider_->ReleaseFrameResources();

  ASSERT_FALSE(render_pass->shared_quad_state_list.empty());
  const viz::SharedQuadState* sqs = GetFirstSharedState(*render_pass);
  ASSERT_TRUE(sqs);

  EXPECT_EQ(sqs->quad_to_target_transform, GetParam().expected_transform);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoFrameResourceProviderTest,
    testing::Values(
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_0,
                                       /*mirrored=*/false),
            gfx::Transform(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_90,
                                       /*mirrored=*/false),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutZAxis(90.0);
              expected_transform.Translate(0, -kFrameSize.height());
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_180,
                                       /*mirrored=*/false),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutZAxis(180.0);
              expected_transform.Translate(-kFrameSize.width(),
                                           -kFrameSize.height());
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_270,
                                       /*mirrored=*/false),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutZAxis(270.0);
              expected_transform.Translate(-kFrameSize.width(), 0);
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_0,
                                       /*mirrored=*/true),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutYAxis(180.0);
              expected_transform.Translate(-kFrameSize.width(), 0);
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_90,
                                       /*mirrored=*/true),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutYAxis(180.0);
              expected_transform.Translate(-kFrameSize.height(), 0);
              expected_transform.RotateAboutZAxis(90.0);
              expected_transform.Translate(0, -kFrameSize.height());
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_180,
                                       /*mirrored=*/true),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutYAxis(180.0);
              expected_transform.Translate(-kFrameSize.width(), 0);
              expected_transform.RotateAboutZAxis(180.0);
              expected_transform.Translate(-kFrameSize.width(),
                                           -kFrameSize.height());
              return expected_transform;
            }(),
        },
        TransformTestParams{
            media::VideoTransformation(media::VIDEO_ROTATION_270,
                                       /*mirrored=*/true),
            [] {
              gfx::Transform expected_transform;
              expected_transform.RotateAboutYAxis(180.0);
              expected_transform.Translate(-kFrameSize.height(), 0);
              expected_transform.RotateAboutZAxis(270.0);
              expected_transform.Translate(-kFrameSize.width(), 0);
              return expected_transform;
            }(),
        }));

}  // namespace blink
