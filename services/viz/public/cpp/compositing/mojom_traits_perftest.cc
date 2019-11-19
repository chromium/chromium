// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/viz/public/cpp/compositing/compositor_frame_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_quad_state_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_frame.mojom.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

namespace viz {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kNumWarmupRuns = 20;
static const int kTimeCheckInterval = 10;

enum class UseSingleSharedQuadState { YES, NO };

constexpr char kMetricPrefixVizSerialization[] = "VizSerialization.";
constexpr char kMetricStructDeserializationTimeUs[] =
    "StructTraits_min_frame_deserialization_time";
constexpr char kMetricStructDeserializationThroughputRunsPerS[] =
    "StructTraits_deserialization_throughput";
constexpr char kMetricStructSerializationTimeUs[] =
    "StructTraits_min_frame_serialization_time";
constexpr char kMetricStructSerializationThroughputRunsPerS[] =
    "StructTraits_serialization_throughput";

perf_test::PerfResultReporter SetUpReporter(
    const std::string& story,
    UseSingleSharedQuadState single_sqs) {
  std::string story_suffix = single_sqs == UseSingleSharedQuadState::YES
                                 ? "_per_render_pass_shared_quad_state"
                                 : "_per_quad_shared_quad_state";
  perf_test::PerfResultReporter reporter(kMetricPrefixVizSerialization,
                                         story + story_suffix);
  reporter.RegisterImportantMetric(kMetricStructDeserializationTimeUs, "us");
  reporter.RegisterImportantMetric(
      kMetricStructDeserializationThroughputRunsPerS, "runs/s");
  reporter.RegisterImportantMetric(kMetricStructSerializationTimeUs, "us");
  reporter.RegisterImportantMetric(kMetricStructSerializationThroughputRunsPerS,
                                   "runs/s");
  return reporter;
}

class VizSerializationPerfTest : public testing::Test {
 protected:
  static void RunDeserializationTestStructTraits(
      const std::string& story,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    mojo::Message message = mojom::CompositorFrame::SerializeAsMessage(&frame);
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      CompositorFrame compositor_frame;
      mojom::CompositorFrame::Deserialize(
          message.payload(), message.payload_num_bytes(), &compositor_frame);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        CompositorFrame compositor_frame;
        mojom::CompositorFrame::Deserialize(
            message.payload(), message.payload_num_bytes(), &compositor_frame);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    auto reporter = SetUpReporter(story, single_sqs);
    reporter.AddResult(kMetricStructDeserializationTimeUs,
                       min_time.InMicrosecondsF() / kTimeCheckInterval);
    reporter.AddResult(kMetricStructDeserializationThroughputRunsPerS,
                       count * 1000 / kTimeLimitMillis);
  }

  static void RunSerializationTestStructTraits(
      const std::string& story,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      mojo::Message message =
          mojom::CompositorFrame::SerializeAsMessage(&frame);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        mojo::Message message =
            mojom::CompositorFrame::SerializeAsMessage(&frame);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    auto reporter = SetUpReporter(story, single_sqs);
    reporter.AddResult(kMetricStructSerializationTimeUs,
                       min_time.InMicrosecondsF() / kTimeCheckInterval);
    reporter.AddResult(kMetricStructSerializationThroughputRunsPerS,
                       count * 1000 / kTimeLimitMillis);
  }

  static void RunComplexCompositorFrameTest(const std::string& story) {
    CompositorFrame frame;
    frame.metadata.begin_frame_ack = BeginFrameAck(0, 1, true);

    std::vector<TransferableResource>& resource_list = frame.resource_list;
    for (uint32_t i = 0; i < 80; ++i) {
      TransferableResource arbitrary_resource;
      resource_list.push_back(arbitrary_resource);
    }

    auto& render_pass_list = frame.render_pass_list;

    gfx::Transform arbitrary_matrix1;
    arbitrary_matrix1.Scale(3, 3);
    arbitrary_matrix1.Translate(-5, 20);
    arbitrary_matrix1.Rotate(15);
    gfx::Transform arbitrary_matrix2;
    arbitrary_matrix2.Scale(10, -1);
    arbitrary_matrix2.Translate(20, 3);
    arbitrary_matrix2.Rotate(24);
    gfx::Rect arbitrary_rect1(-5, 9, 3, 15);
    gfx::Rect arbitrary_rect2(40, 23, 11, 7);
    gfx::Rect arbitrary_rect1_inside_rect2(44, 23, 4, 2);
    gfx::Rect arbitrary_rect3(7, -53, 22, 19);
    gfx::Rect arbitrary_rect2_inside_rect3(12, -51, 5, 12);
    gfx::Size arbitrary_size1(15, 19);
    gfx::Size arbitrary_size2(3, 99);
    gfx::RectF arbitrary_rectf1(4.2f, -922.1f, 15.6f, 29.5f);
    gfx::RRectF arbitrary_rrectf1(4.2f, -922.1f, 15.6f, 29.5f, 1.2f, 2.3f, 3.4f,
                                  4.5f, 5.6f, 6.7f, 7.8f, 8.9f);
    gfx::RRectF arbitrary_rrectf2(gfx::RectF(1.f, 2.f, 30.f, 45.f), 5.f);
    gfx::RRectF arbitrary_rrectf3(gfx::RectF(5.f, 6.f, 20.f, 35.f), 2.f, 3.f);
    gfx::PointF arbitrary_pointf1(31.4f, 15.9f);
    gfx::PointF arbitrary_pointf2(26.5f, -35.8f);
    float arbitrary_float1 = 0.7f;
    float arbitrary_float2 = 0.3f;
    float arbitrary_float3 = 0.9f;
    float arbitrary_float_array[4] = {3.5f, 6.2f, 9.3f, 12.3f};
    bool arbitrary_bool1 = true;
    bool arbitrary_bool2 = false;
    bool arbitrary_bool3 = true;
    bool arbitrary_bool4 = true;
    bool arbitrary_bool5 = false;
    bool arbitrary_bool6 = true;
    gfx::ProtectedVideoType arbitrary_protected_video_type =
        gfx::ProtectedVideoType::kClear;
    int arbitrary_context_id1 = 12;
    int arbitrary_context_id2 = 57;
    int arbitrary_context_id3 = -503;
    SkColor arbitrary_color = SkColorSetARGB(25, 36, 47, 58);
    SkBlendMode arbitrary_blend_mode1 = SkBlendMode::kScreen;
    SkBlendMode arbitrary_blend_mode2 = SkBlendMode::kLighten;
    SkBlendMode arbitrary_blend_mode3 = SkBlendMode::kOverlay;
    ResourceId arbitrary_resourceid1 = 55;
    ResourceId arbitrary_resourceid2 = 47;
    ResourceId arbitrary_resourceid3 = 23;
    ResourceId arbitrary_resourceid4 = 16;
    SkScalar arbitrary_sigma = SkFloatToScalar(2.0f);
    gfx::ColorSpace arbitrary_color_space = gfx::ColorSpace::CreateXYZD50();
    int root_id = 14;

    cc::FilterOperations arbitrary_filters1;
    arbitrary_filters1.Append(
        cc::FilterOperation::CreateGrayscaleFilter(arbitrary_float1));
    arbitrary_filters1.Append(cc::FilterOperation::CreateReferenceFilter(
        sk_make_sp<cc::BlurPaintFilter>(
            arbitrary_sigma, arbitrary_sigma,
            cc::BlurPaintFilter::TileMode::kClampToBlack_TileMode, nullptr)));

    cc::FilterOperations arbitrary_filters2;
    arbitrary_filters2.Append(
        cc::FilterOperation::CreateBrightnessFilter(arbitrary_float2));

    std::unique_ptr<RenderPass> pass_in = RenderPass::Create();
    pass_in->SetAll(root_id, arbitrary_rect1, arbitrary_rect2,
                    arbitrary_matrix1, arbitrary_filters2, arbitrary_filters1,
                    arbitrary_rrectf1, arbitrary_color_space, arbitrary_bool1,
                    arbitrary_bool1, arbitrary_bool1, arbitrary_bool1);

    // Texture quads
    for (uint32_t i = 0; i < 10; ++i) {
      SharedQuadState* shared_state1_in =
          pass_in->CreateAndAppendSharedQuadState();
      shared_state1_in->SetAll(
          arbitrary_matrix1, arbitrary_rect1, arbitrary_rect1,
          arbitrary_rrectf1, arbitrary_rect2, arbitrary_bool1, arbitrary_bool1,
          arbitrary_float1, arbitrary_blend_mode1, arbitrary_context_id1);

      auto* texture_in = pass_in->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_in->SetAll(
          shared_state1_in, arbitrary_rect2, arbitrary_rect1_inside_rect2,
          arbitrary_bool1, arbitrary_resourceid1, arbitrary_size1,
          arbitrary_bool1, arbitrary_pointf1, arbitrary_pointf2,
          arbitrary_color, arbitrary_float_array, arbitrary_bool4,
          arbitrary_bool5, arbitrary_bool6, arbitrary_protected_video_type);

      auto* texture_in2 = pass_in->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_in2->SetAll(
          shared_state1_in, arbitrary_rect2, arbitrary_rect1_inside_rect2,
          arbitrary_bool1, arbitrary_resourceid2, arbitrary_size1,
          arbitrary_bool3, arbitrary_pointf1, arbitrary_pointf2,
          arbitrary_color, arbitrary_float_array, arbitrary_bool4,
          arbitrary_bool5, arbitrary_bool6, arbitrary_protected_video_type);

      auto* texture_in3 = pass_in->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_in3->SetAll(
          shared_state1_in, arbitrary_rect2, arbitrary_rect1_inside_rect2,
          arbitrary_bool1, arbitrary_resourceid3, arbitrary_size1,
          arbitrary_bool2, arbitrary_pointf1, arbitrary_pointf2,
          arbitrary_color, arbitrary_float_array, arbitrary_bool4,
          arbitrary_bool6, arbitrary_bool6, arbitrary_protected_video_type);

      auto* texture_in4 = pass_in->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_in4->SetAll(
          shared_state1_in, arbitrary_rect2, arbitrary_rect1_inside_rect2,
          arbitrary_bool1, arbitrary_resourceid4, arbitrary_size2,
          arbitrary_bool4, arbitrary_pointf1, arbitrary_pointf2,
          arbitrary_color, arbitrary_float_array, arbitrary_bool4,
          arbitrary_bool5, arbitrary_bool6, arbitrary_protected_video_type);
    }

    // Tiled quads
    for (uint32_t i = 0; i < 10; ++i) {
      SharedQuadState* shared_state2_in =
          pass_in->CreateAndAppendSharedQuadState();
      shared_state2_in->SetAll(
          arbitrary_matrix2, arbitrary_rect2, arbitrary_rect2,
          arbitrary_rrectf2, arbitrary_rect3, arbitrary_bool1, arbitrary_bool1,
          arbitrary_float2, arbitrary_blend_mode2, arbitrary_context_id2);
      for (uint32_t j = 0; j < 6; ++j) {
        auto* tile_in = pass_in->CreateAndAppendDrawQuad<TileDrawQuad>();
        tile_in->SetAll(
            shared_state2_in, arbitrary_rect2, arbitrary_rect1_inside_rect2,
            arbitrary_bool1, arbitrary_resourceid3, arbitrary_rectf1,
            arbitrary_size1, arbitrary_bool2, arbitrary_bool3, arbitrary_bool4);
      }
    }

    // Solid color quads
    for (uint32_t i = 0; i < 5; ++i) {
      SharedQuadState* shared_state3_in =
          pass_in->CreateAndAppendSharedQuadState();
      shared_state3_in->SetAll(
          arbitrary_matrix1, arbitrary_rect3, arbitrary_rect3,
          arbitrary_rrectf3, arbitrary_rect1, arbitrary_bool1, arbitrary_bool1,
          arbitrary_float3, arbitrary_blend_mode3, arbitrary_context_id3);
      for (uint32_t j = 0; j < 5; ++j) {
        auto* solidcolor_in =
            pass_in->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
        solidcolor_in->SetAll(shared_state3_in, arbitrary_rect3,
                              arbitrary_rect2_inside_rect3, arbitrary_bool1,
                              arbitrary_color, arbitrary_bool2);
      }
    }

    render_pass_list.push_back(std::move(pass_in));
    RunTest(story, std::move(frame), UseSingleSharedQuadState::NO);
  }

  static void RunCompositorFrameTest(const std::string& story,
                                     uint32_t num_quads,
                                     uint32_t num_passes,
                                     UseSingleSharedQuadState single_sqs) {
    CompositorFrame frame = MakeEmptyCompositorFrame();

    for (uint32_t i = 0; i < num_passes; ++i) {
      std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
      render_pass->SetNew(1, gfx::Rect(20, 20), gfx::Rect(), gfx::Transform());
      for (uint32_t j = 0; j < num_quads; ++j) {
        if (j == 0 || single_sqs == UseSingleSharedQuadState::NO)
          render_pass->CreateAndAppendSharedQuadState();
        const gfx::Rect bounds(100, 100, 100, 100);
        const bool kForceAntiAliasingOff = true;
        auto* quad = render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
        quad->SetNew(render_pass->shared_quad_state_list.back(), bounds, bounds,
                     SK_ColorRED, kForceAntiAliasingOff);
      }
      frame.render_pass_list.push_back(std::move(render_pass));
    }
    RunTest(story, std::move(frame), single_sqs);
  }

  static void RunTest(const std::string& story,
                      CompositorFrame frame,
                      UseSingleSharedQuadState single_sqs) {
    RunSerializationTestStructTraits(story, frame, single_sqs);
    RunDeserializationTestStructTraits(story, frame, single_sqs);
  }
};

// Test for compositor frames with one render pass, 80 resources in resource
// list, 10 shared quad states with 4 texture quads each, 10 shared quad states
// with 6 tiled quads each, and 5 shared quad states with 5 solid color quads
// each.
TEST_F(VizSerializationPerfTest, DelegatedFrame_Complex) {
  RunComplexCompositorFrameTest("DelegatedFrame_Complex");
}

// Test for compositor frames with one render pass and 4000 quads.
TEST_F(VizSerializationPerfTest, DelegatedFrame_ManyQuads_1_4000) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_4000", 4000, 1,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_4000", 4000, 1,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with 5 render pass and each with 100 quads.
TEST_F(VizSerializationPerfTest, DelegatedFrame_ManyRenderPasses_5_100) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_5_100", 100, 5,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_5_100", 100, 5,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with 10 render pass and each with 500 quads.
TEST_F(VizSerializationPerfTest, DelegatedFrame_ManyRenderPasses_10_500) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_10_500", 500, 10,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_10_500", 500, 10,
                         UseSingleSharedQuadState::NO);
}

}  // namespace
}  // namespace viz
