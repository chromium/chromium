// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "ipc/ipc_message_utils.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_metadata_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_render_pass_mojom_traits.h"
#include "services/viz/public/cpp/compositing/copy_output_request_mojom_traits.h"
#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"
#include "services/viz/public/cpp/compositing/filter_operation_mojom_traits.h"
#include "services/viz/public/cpp/compositing/filter_operations_mojom_traits.h"
#include "services/viz/public/cpp/compositing/frame_sink_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/local_surface_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"
#include "services/viz/public/cpp/compositing/returned_resource_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_quad_state_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/surface_info_mojom_traits.h"
#include "services/viz/public/cpp/compositing/transferable_resource_mojom_traits.h"
#include "services/viz/public/mojom/compositing/begin_frame_args.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame.mojom.h"
#include "services/viz/public/mojom/compositing/filter_operation.mojom.h"
#include "services/viz/public/mojom/compositing/filter_operations.mojom.h"
#include "services/viz/public/mojom/compositing/returned_resource.mojom.h"
#include "services/viz/public/mojom/compositing/selection.mojom.h"
#include "services/viz/public/mojom/compositing/surface_info.mojom.h"
#include "services/viz/public/mojom/compositing/surface_range.mojom.h"
#include "services/viz/public/mojom/compositing/transferable_resource.mojom.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "skia/public/mojom/tile_mode_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkString.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

namespace viz {

namespace {

using StructTraitsTest = testing::Test;

}  // namespace

TEST_F(StructTraitsTest, BeginFrameArgs) {
  const base::TimeTicks frame_time = base::TimeTicks::Now();
  const base::TimeTicks deadline = base::TimeTicks::Now();
  const base::TimeDelta interval = base::Milliseconds(1337);
  const BeginFrameArgs::BeginFrameArgsType type = BeginFrameArgs::NORMAL;
  const bool on_critical_path = true;
  const uint64_t source_id = 5;
  const uint64_t sequence_number = 10;
  const uint64_t frames_throttled_since_last = 20;
  const bool animate_only = true;
  BeginFrameArgs input;
  input.frame_id = BeginFrameId(source_id, sequence_number);
  input.frames_throttled_since_last = frames_throttled_since_last;
  input.frame_time = frame_time;
  input.deadline = deadline;
  input.interval = interval;
  input.type = type;
  input.on_critical_path = on_critical_path;
  input.animate_only = animate_only;

  BeginFrameArgs output;
  mojo::test::SerializeAndDeserialize<mojom::BeginFrameArgs>(input, output);

  EXPECT_EQ(source_id, output.frame_id.source_id);
  EXPECT_EQ(sequence_number, output.frame_id.sequence_number);
  EXPECT_EQ(frames_throttled_since_last, output.frames_throttled_since_last);
  EXPECT_EQ(frame_time, output.frame_time);
  EXPECT_EQ(deadline, output.deadline);
  EXPECT_EQ(interval, output.interval);
  EXPECT_EQ(type, output.type);
  EXPECT_EQ(on_critical_path, output.on_critical_path);
  EXPECT_EQ(animate_only, output.animate_only);
}

TEST_F(StructTraitsTest, BeginFrameAck) {
  const uint64_t source_id = 5;
  const uint64_t sequence_number = 10;
  const bool has_damage = true;
  BeginFrameAck input;
  input.frame_id = BeginFrameId(source_id, sequence_number);
  input.has_damage = has_damage;

  BeginFrameAck output;
  mojo::test::SerializeAndDeserialize<mojom::BeginFrameAck>(input, output);

  EXPECT_EQ(source_id, output.frame_id.source_id);
  EXPECT_EQ(sequence_number, output.frame_id.sequence_number);
  EXPECT_TRUE(output.has_damage);
}

namespace {

void ExpectEqual(const cc::FilterOperation& input,
                 const cc::FilterOperation& output) {
  EXPECT_EQ(input.type(), output.type());
  switch (input.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
      EXPECT_EQ(input.amount(), output.amount());
      break;
    case cc::FilterOperation::DROP_SHADOW:
      EXPECT_EQ(input.amount(), output.amount());
      EXPECT_EQ(input.offset(), output.offset());
      EXPECT_EQ(input.drop_shadow_color(), output.drop_shadow_color());
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      EXPECT_EQ(input.matrix(), output.matrix());
      break;
    case cc::FilterOperation::ZOOM:
      EXPECT_EQ(input.amount(), output.amount());
      EXPECT_EQ(input.zoom_inset(), output.zoom_inset());
      break;
    case cc::FilterOperation::REFERENCE: {
      ASSERT_EQ(!!input.image_filter(), !!output.image_filter());
      if (input.image_filter()) {
        EXPECT_TRUE(
            input.image_filter()->EqualsForTesting(*output.image_filter()));
      }
      break;
    }
    case cc::FilterOperation::ALPHA_THRESHOLD:
      NOTREACHED_IN_MIGRATION();
      break;
    case cc::FilterOperation::OFFSET:
      EXPECT_EQ(input.offset(), output.offset());
      break;
  }
}

}  // namespace

TEST_F(StructTraitsTest, FilterOperationBlur) {
  cc::FilterOperation input = cc::FilterOperation::CreateBlurFilter(20);

  cc::FilterOperation output;
  mojo::test::SerializeAndDeserialize<mojom::FilterOperation>(input, output);
  ExpectEqual(input, output);
}

TEST_F(StructTraitsTest, FilterOperationDropShadow) {
  cc::FilterOperation input = cc::FilterOperation::CreateDropShadowFilter(
      gfx::Point(4, 4), 4.0f, SkColor4f{0.15f, 0.0f, 0.0f, 1.0f});

  cc::FilterOperation output;
  mojo::test::SerializeAndDeserialize<mojom::FilterOperation>(input, output);
  ExpectEqual(input, output);
}

TEST_F(StructTraitsTest, FilterOperationReferenceFilter) {
  cc::FilterOperation input = cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::DropShadowPaintFilter>(
          SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4),
          SkIntToScalar(9), SkColors::kBlack,
          cc::DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground,
          nullptr));

  cc::FilterOperation output;
  mojo::test::SerializeAndDeserialize<mojom::FilterOperation>(input, output);
  ExpectEqual(input, output);
}

TEST_F(StructTraitsTest, FilterOperations) {
  cc::FilterOperations input;
  input.Append(cc::FilterOperation::CreateBlurFilter(0.f));
  input.Append(cc::FilterOperation::CreateSaturateFilter(4.f));
  input.Append(cc::FilterOperation::CreateZoomFilter(2.0f, 1));

  cc::FilterOperations output;
  mojo::test::SerializeAndDeserialize<mojom::FilterOperations>(input, output);

  EXPECT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i) {
    ExpectEqual(input.at(i), output.at(i));
  }
}

TEST_F(StructTraitsTest, LocalSurfaceId) {
  LocalSurfaceId input(
      42, base::UnguessableToken::CreateForTesting(0x12345678, 0x9abcdef0));

  LocalSurfaceId output;
  mojo::test::SerializeAndDeserialize<mojom::LocalSurfaceId>(input, output);

  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, CopyOutputRequest_BitmapRequest) {
  base::test::TaskEnvironment task_environment;

  const auto result_format = CopyOutputRequest::ResultFormat::RGBA;
  const auto result_destination =
      CopyOutputRequest::ResultDestination::kSystemMemory;
  const gfx::Rect area(5, 7, 44, 55);
  const auto source =
      base::UnguessableToken::CreateForTesting(0xdeadbeef, 0xdeadf00d);
  // Requesting 2:3 scale in X dimension, 5:4 in Y dimension.
  const gfx::Vector2d scale_from(2, 5);
  const gfx::Vector2d scale_to(3, 4);
  const gfx::Rect result_rect(7, 8, 132, 44);

  base::RunLoop run_loop;
  std::unique_ptr<CopyOutputRequest> input(new CopyOutputRequest(
      result_format, result_destination,
      base::BindOnce(
          [](base::OnceClosure quit_closure, const gfx::Rect& expected_rect,
             std::unique_ptr<CopyOutputResult> result) {
            EXPECT_EQ(expected_rect, result->rect());
            // Note: CopyOutputResult plumbing for bitmap requests is tested in
            // StructTraitsTest.CopyOutputResult_Bitmap.
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), result_rect)));
  input->SetScaleRatio(scale_from, scale_to);
  EXPECT_EQ(scale_from, input->scale_from());
  EXPECT_EQ(scale_to, input->scale_to());
  input->set_area(area);
  input->set_result_selection(result_rect);
  input->set_source(source);
  EXPECT_TRUE(input->is_scaled());
  std::unique_ptr<CopyOutputRequest> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputRequest>(input, output);

  EXPECT_EQ(result_format, output->result_format());
  EXPECT_EQ(result_destination, output->result_destination());
  EXPECT_TRUE(output->is_scaled());
  EXPECT_EQ(scale_from, output->scale_from());
  EXPECT_EQ(scale_to, output->scale_to());
  EXPECT_TRUE(output->has_source());
  EXPECT_EQ(source, output->source());
  EXPECT_TRUE(output->has_area());
  EXPECT_EQ(area, output->area());
  EXPECT_TRUE(output->has_result_selection());
  EXPECT_EQ(result_rect, output->result_selection());

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(
      result_rect.width(), result_rect.height(), SkColorSpace::MakeSRGB()));
  output->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_rect, std::move(bitmap)));
  // If the CopyOutputRequest callback is called, this ends. Otherwise, the test
  // will time out and fail.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputRequest_MessagePipeBroken) {
  base::test::TaskEnvironment task_environment;

  base::RunLoop run_loop;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> result) {
            EXPECT_TRUE(result->IsEmpty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  auto result_sender = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<CopyOutputRequest>>::result_sender(request);
  result_sender.reset();
  // The callback must be called with an empty CopyOutputResult. If it's never
  // called, this will never end and the test times out.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputRequest_TextureRequest) {
  base::test::TaskEnvironment task_environment;

  const auto result_format = CopyOutputRequest::ResultFormat::RGBA;
  const auto result_destination =
      CopyOutputRequest::ResultDestination::kNativeTextures;

  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 3};
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  gpu::SyncToken sync_token;
  const gfx::Rect result_rect(10, 10);

  base::RunLoop run_loop_for_result;
  std::unique_ptr<CopyOutputRequest> input(new CopyOutputRequest(
      result_format, result_destination,
      base::BindOnce(
          [](base::OnceClosure quit_closure, const gfx::Rect& expected_rect,
             std::unique_ptr<CopyOutputResult> result) {
            EXPECT_EQ(expected_rect, result->rect());
            // Note: CopyOutputResult plumbing for texture requests is tested in
            // StructTraitsTest.CopyOutputResult_Texture.
            std::move(quit_closure).Run();
          },
          run_loop_for_result.QuitClosure(), result_rect)));
  EXPECT_FALSE(input->is_scaled());
  std::unique_ptr<CopyOutputRequest> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputRequest>(input, output);

  EXPECT_EQ(output->result_format(), result_format);
  EXPECT_EQ(output->result_destination(), result_destination);
  EXPECT_FALSE(output->is_scaled());
  EXPECT_FALSE(output->has_source());
  EXPECT_FALSE(output->has_area());

  base::RunLoop run_loop_for_release;

  CopyOutputResult::ReleaseCallbacks release_callbacks;
  release_callbacks.push_back(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const gpu::SyncToken& expected_sync_token,
         const gpu::SyncToken& sync_token, bool is_lost) {
        EXPECT_EQ(expected_sync_token, sync_token);
        EXPECT_FALSE(is_lost);
        std::move(quit_closure).Run();
      },
      run_loop_for_release.QuitClosure(), sync_token));

  output->SendResult(std::make_unique<CopyOutputTextureResult>(
      result_format, result_rect,
      CopyOutputResult::TextureResult(mailbox, gfx::ColorSpace::CreateSRGB()),
      std::move(release_callbacks)));

  // Wait for the result to be delivered to the other side: The
  // CopyOutputRequest callback will be called, at which point
  // |run_loop_for_result| ends. Otherwise, the test will time out and fail.
  run_loop_for_result.Run();

  // Now, wait for the the texture release callback on this side to be run:
  // The CopyOutputResult callback will be called, at which point
  // |run_loop_for_release| ends. Otherwise, the test will time out and fail.
  run_loop_for_release.Run();
}

TEST_F(StructTraitsTest, CopyOutputRequest_CallbackRunsOnce) {
  base::test::TaskEnvironment task_environment;

  int n_called = 0;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](int* n_called, std::unique_ptr<CopyOutputResult> result) {
            ++*n_called;
          },
          base::Unretained(&n_called)));
  auto result_sender_pending_remote = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<CopyOutputRequest>>::result_sender(request);

  mojo::Remote<mojom::CopyOutputResultSender> result_sender_remote(
      std::move(result_sender_pending_remote));
  for (int i = 0; i < 10; i++)
    result_sender_remote->SendResult(std::make_unique<CopyOutputResult>(
        request->result_format(), request->result_destination(), gfx::Rect(),
        false));
  EXPECT_EQ(0, n_called);
  result_sender_remote.FlushForTesting();
  EXPECT_EQ(1, n_called);
}

TEST_F(StructTraitsTest, Selection) {
  gfx::SelectionBound start;
  start.SetEdge(gfx::PointF(1234.5f, 67891.f), gfx::PointF(5432.1f, 1987.6f));
  start.set_visible(true);
  start.set_type(gfx::SelectionBound::CENTER);
  gfx::SelectionBound end;
  end.SetEdge(gfx::PointF(1337.5f, 52124.f), gfx::PointF(1234.3f, 8765.6f));
  end.set_visible(false);
  end.set_type(gfx::SelectionBound::RIGHT);
  Selection<gfx::SelectionBound> input;
  input.start = start;
  input.end = end;
  Selection<gfx::SelectionBound> output;
  mojo::test::SerializeAndDeserialize<mojom::Selection>(input, output);
  EXPECT_EQ(start, output.start);
  EXPECT_EQ(end, output.end);
}

TEST_F(StructTraitsTest, SharedQuadState) {
  const auto quad_to_target_transform =
      gfx::Transform::RowMajor(1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f,
                               10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f);
  const gfx::Rect layer_rect(1234, 5678);
  const gfx::Rect visible_layer_rect(12, 34, 56, 78);
  const gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(1.f, 2.f, 30.f, 40.f), 5));
  const gfx::Rect clip_rect(123, 456, 789, 101112);
  bool are_contents_opaque = true;
  const float opacity = 0.9f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  const int sorting_context_id = 1337;
  bool is_fast_rounded_corner = true;
  SharedQuadState input_sqs;
  input_sqs.SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                   mask_filter_info, clip_rect, are_contents_opaque, opacity,
                   blend_mode, sorting_context_id, /*layer_id=*/0u,
                   is_fast_rounded_corner);
  SharedQuadState output_sqs;
  mojo::test::SerializeAndDeserialize<mojom::SharedQuadState>(input_sqs,
                                                              output_sqs);
  EXPECT_EQ(quad_to_target_transform, output_sqs.quad_to_target_transform);
  EXPECT_EQ(layer_rect, output_sqs.quad_layer_rect);
  EXPECT_EQ(visible_layer_rect, output_sqs.visible_quad_layer_rect);
  EXPECT_EQ(mask_filter_info.rounded_corner_bounds(),
            output_sqs.mask_filter_info.rounded_corner_bounds());
  EXPECT_EQ(clip_rect, output_sqs.clip_rect);
  EXPECT_EQ(opacity, output_sqs.opacity);
  EXPECT_EQ(blend_mode, output_sqs.blend_mode);
  EXPECT_EQ(sorting_context_id, output_sqs.sorting_context_id);
  EXPECT_EQ(is_fast_rounded_corner, output_sqs.is_fast_rounded_corner);
}

// Note that this is a fairly trivial test of CompositorFrame serialization as
// most of the heavy lifting has already been done by CompositorFrameMetadata,
// CompositorRenderPass, and QuadListBasic unit tests.
TEST_F(StructTraitsTest, CompositorFrame) {
  auto render_pass = CompositorRenderPass::Create();
  render_pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(5, 6),
                      gfx::Rect(2, 3), gfx::Transform());

  // SharedQuadState.
  const auto sqs_quad_to_target_transform =
      gfx::Transform::RowMajor(1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f,
                               10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f);
  const gfx::Rect sqs_layer_rect(1234, 5678);
  const gfx::Rect sqs_visible_layer_rect(12, 34, 56, 78);
  const gfx::MaskFilterInfo sqs_mask_filter_info(
      gfx::RRectF(gfx::RectF(3.f, 4.f, 50.f, 15.f), 3));
  const gfx::Rect sqs_clip_rect(123, 456, 789, 101112);
  bool sqs_are_contents_opaque = false;
  const float sqs_opacity = 0.9f;
  const SkBlendMode sqs_blend_mode = SkBlendMode::kSrcOver;
  const int sqs_sorting_context_id = 1337;
  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(sqs_quad_to_target_transform, sqs_layer_rect,
              sqs_visible_layer_rect, sqs_mask_filter_info, sqs_clip_rect,
              sqs_are_contents_opaque, sqs_opacity, sqs_blend_mode,
              sqs_sorting_context_id, /*layer_id=*/0u,
              /*fast_rounded_corner=*/false);

  // DebugBorderDrawQuad.
  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor4f color1 = SkColors::kRed;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  // SolidColorDrawQuad.
  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const SkColor4f color2 = SkColors::kWhite;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  // TransferableResource constants.
  const ResourceId single_plane_id(1337);
  const ResourceId multi_plane_id(1338);
  const SharedImageFormat single_plane_format = SinglePlaneFormat::kALPHA_8;
  const SharedImageFormat multi_plane_format = MultiPlaneFormat::kNV12;
  const gfx::Size tr_size(1234, 5678);
  TransferableResource single_plane_resource;
  single_plane_resource.id = single_plane_id;
  single_plane_resource.format = single_plane_format;
  single_plane_resource.size = tr_size;
  TransferableResource multi_plane_resource;
  multi_plane_resource.id = multi_plane_id;
  multi_plane_resource.format = multi_plane_format;
  multi_plane_resource.size = tr_size;

  // CompositorFrameMetadata constants.
  const float device_scale_factor = 2.6f;
  const gfx::PointF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const BeginFrameAck begin_frame_ack(5, 10, false);

  CompositorFrame input;
  input.metadata.device_scale_factor = device_scale_factor;
  input.metadata.root_scroll_offset = root_scroll_offset;
  input.metadata.page_scale_factor = page_scale_factor;
  input.metadata.scrollable_viewport_size = scrollable_viewport_size;
  input.render_pass_list.push_back(std::move(render_pass));
  input.resource_list.push_back(single_plane_resource);
  input.resource_list.push_back(multi_plane_resource);
  input.metadata.begin_frame_ack = begin_frame_ack;
  input.metadata.frame_token = 1;

  CompositorFrame output;
  mojo::test::SerializeAndDeserialize<mojom::CompositorFrame>(input, output);

  EXPECT_EQ(device_scale_factor, output.metadata.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.metadata.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.metadata.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.metadata.scrollable_viewport_size);
  EXPECT_EQ(begin_frame_ack, output.metadata.begin_frame_ack);

  ASSERT_EQ(2u, output.resource_list.size());
  TransferableResource out_resource1 = output.resource_list[0];
  EXPECT_EQ(single_plane_id, out_resource1.id);
  EXPECT_EQ(single_plane_format, out_resource1.format);
  EXPECT_EQ(tr_size, out_resource1.size);
  TransferableResource out_resource2 = output.resource_list[1];
  EXPECT_EQ(multi_plane_id, out_resource2.id);
  EXPECT_EQ(multi_plane_format, out_resource2.format);
  EXPECT_EQ(tr_size, out_resource2.size);

  EXPECT_EQ(1u, output.render_pass_list.size());
  const CompositorRenderPass* out_render_pass =
      output.render_pass_list[0].get();
  ASSERT_EQ(2u, out_render_pass->quad_list.size());
  ASSERT_EQ(1u, out_render_pass->shared_quad_state_list.size());

  const SharedQuadState* out_sqs =
      out_render_pass->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(sqs_quad_to_target_transform, out_sqs->quad_to_target_transform);
  EXPECT_EQ(sqs_layer_rect, out_sqs->quad_layer_rect);
  EXPECT_EQ(sqs_visible_layer_rect, out_sqs->visible_quad_layer_rect);
  EXPECT_EQ(sqs_mask_filter_info, out_sqs->mask_filter_info);
  EXPECT_EQ(sqs_clip_rect, out_sqs->clip_rect);
  EXPECT_EQ(sqs_are_contents_opaque, out_sqs->are_contents_opaque);
  EXPECT_EQ(sqs_opacity, out_sqs->opacity);
  EXPECT_EQ(sqs_blend_mode, out_sqs->blend_mode);
  EXPECT_EQ(sqs_sorting_context_id, out_sqs->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(
          out_render_pass->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(out_render_pass->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);
}

TEST_F(StructTraitsTest, CompositorFrameTransitionDirective) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();

  blink::ViewTransitionToken transition_token;
  CompositorFrameTransitionDirective::SharedElement element;
  element.render_pass_id = frame.render_pass_list.front()->id;
  element.view_transition_element_resource_id =
      ViewTransitionElementResourceId(transition_token, 1);
  uint32_t sequence_id = 1u;
  frame.metadata.transition_directives.push_back(
      CompositorFrameTransitionDirective::CreateSave(
          transition_token, /*maybe_cross_frame_sink=*/true, sequence_id,
          {element}, {}));

  // This ensures de-serialization succeeds if all passes are present.
  CompositorFrame output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CompositorFrame>(
      frame, output));
  EXPECT_EQ(output.metadata.transition_directives.size(), 1u);
  const auto& directive = output.metadata.transition_directives[0];
  EXPECT_EQ(directive.transition_token(), transition_token);
  EXPECT_TRUE(directive.maybe_cross_frame_sink());
  EXPECT_EQ(directive.sequence_id(), sequence_id);
  EXPECT_EQ(directive.type(), CompositorFrameTransitionDirective::Type::kSave);
  EXPECT_EQ(directive.shared_elements().size(), 1u);
  EXPECT_EQ(directive.shared_elements()[0], element);

  element.render_pass_id = CompositorRenderPassId(
      frame.render_pass_list.back()->id.GetUnsafeValue() + 1);
  frame.metadata.transition_directives.push_back(
      CompositorFrameTransitionDirective::CreateSave(
          transition_token, /*maybe_cross_frame_sink=*/true, sequence_id,
          {element}, {}));

  // This ensures de-serialization fails if a pass is missing.
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::CompositorFrame>(
      frame, output));
}

TEST_F(StructTraitsTest, ViewTransitionElementResourceId) {
  ViewTransitionElementResourceId empty_id;
  ASSERT_FALSE(empty_id.IsValid());
  ViewTransitionElementResourceId empty_output_id;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<
          mojom::ViewTransitionElementResourceId>(empty_id, empty_output_id));
  ASSERT_FALSE(empty_output_id.IsValid());

  ViewTransitionElementResourceId valid_id(blink::ViewTransitionToken(), 2u);
  ASSERT_TRUE(valid_id.IsValid());
  ViewTransitionElementResourceId valid_output_id;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<
          mojom::ViewTransitionElementResourceId>(valid_id, valid_output_id));
  ASSERT_TRUE(valid_output_id.IsValid());
  ASSERT_EQ(valid_output_id, valid_id);
}

TEST_F(StructTraitsTest, SurfaceInfo) {
  const SurfaceId surface_id(
      FrameSinkId(1234, 4321),
      LocalSurfaceId(5678,
                     base::UnguessableToken::CreateForTesting(143254, 144132)));
  constexpr float device_scale_factor = 1.234f;
  constexpr gfx::Size size(987, 123);

  SurfaceInfo input(surface_id, device_scale_factor, size);
  SurfaceInfo output;
  mojo::test::SerializeAndDeserialize<mojom::SurfaceInfo>(input, output);

  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.size_in_pixels(), output.size_in_pixels());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
}

TEST_F(StructTraitsTest, ReturnedResource) {
  const ResourceId id(1337u);
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  gpu::SyncToken sync_token(command_buffer_namespace, command_buffer_id,
                            release_count);
  sync_token.SetVerifyFlush();
  const int count = 1234;
  const bool lost = true;

  ReturnedResource input;
  input.id = id;
  input.sync_token = sync_token;
  input.count = count;
  input.lost = lost;

  ReturnedResource output;
  mojo::test::SerializeAndDeserialize<mojom::ReturnedResource>(input, output);

  EXPECT_EQ(id, output.id);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(count, output.count);
  EXPECT_EQ(lost, output.lost);
}

TEST_F(StructTraitsTest, CompositorFrameMetadata) {
  const float device_scale_factor = 2.6f;
  const gfx::PointF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const bool may_contain_video = true;
  const SkColor4f root_background_color = {0.0f, 0.02f, 0.224f, 0.0f};
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::vector<ui::LatencyInfo> latency_infos = {latency_info};
  std::vector<SurfaceRange> referenced_surfaces;
  SurfaceId id(FrameSinkId(1234, 4321),
               LocalSurfaceId(5678, base::UnguessableToken::Create()));
  referenced_surfaces.emplace_back(id);
  std::vector<SurfaceId> activation_dependencies;
  SurfaceId id2(FrameSinkId(4321, 1234),
                LocalSurfaceId(8765, base::UnguessableToken::Create()));
  activation_dependencies.push_back(id2);
  uint32_t frame_token = 0xdeadbeef;
  uint64_t begin_frame_ack_sequence_number = 0xdeadbeef;
  FrameDeadline frame_deadline(base::TimeTicks(), 4u, base::TimeDelta(), true);
  const float min_page_scale_factor = 3.5f;
  const float top_controls_visible_height = 12.f;

  CompositorFrameMetadata input;
  input.device_scale_factor = device_scale_factor;
  input.root_scroll_offset = root_scroll_offset;
  input.page_scale_factor = page_scale_factor;
  input.scrollable_viewport_size = scrollable_viewport_size;
  input.may_contain_video = may_contain_video;
  input.root_background_color = root_background_color;
  input.latency_info = latency_infos;
  input.referenced_surfaces = referenced_surfaces;
  input.activation_dependencies = activation_dependencies;
  input.deadline = frame_deadline;
  input.frame_token = frame_token;
  input.begin_frame_ack.frame_id.sequence_number =
      begin_frame_ack_sequence_number;
  input.min_page_scale_factor = min_page_scale_factor;
  input.top_controls_visible_height.emplace(top_controls_visible_height);

  CompositorFrameMetadata output;
  mojo::test::SerializeAndDeserialize<mojom::CompositorFrameMetadata>(input,
                                                                      output);
  EXPECT_EQ(device_scale_factor, output.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.scrollable_viewport_size);
  EXPECT_EQ(may_contain_video, output.may_contain_video);
  EXPECT_EQ(root_background_color, output.root_background_color);
  EXPECT_EQ(latency_infos.size(), output.latency_info.size());
  EXPECT_TRUE(output.latency_info[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_EQ(referenced_surfaces.size(), output.referenced_surfaces.size());
  for (uint32_t i = 0; i < referenced_surfaces.size(); ++i)
    EXPECT_EQ(referenced_surfaces[i], output.referenced_surfaces[i]);
  EXPECT_EQ(activation_dependencies.size(),
            output.activation_dependencies.size());
  for (uint32_t i = 0; i < activation_dependencies.size(); ++i)
    EXPECT_EQ(activation_dependencies[i], output.activation_dependencies[i]);
  EXPECT_EQ(frame_deadline, output.deadline);
  EXPECT_EQ(frame_token, output.frame_token);
  EXPECT_EQ(begin_frame_ack_sequence_number,
            output.begin_frame_ack.frame_id.sequence_number);
  EXPECT_EQ(min_page_scale_factor, output.min_page_scale_factor);
  EXPECT_EQ(*output.top_controls_visible_height, top_controls_visible_height);
}

TEST_F(StructTraitsTest, CompositorFrameMetadataBadOffsetTagDefinition) {
  CompositorFrameMetadata input;
  input.device_scale_factor = 1.0f;
  input.frame_token = 1u;
  input.begin_frame_ack.frame_id.sequence_number = 1u;

  {
    // Verify metadata serialization/deserialization is initially successful.
    CompositorFrameMetadata output;
    bool result =
        mojo::test::SerializeAndDeserialize<mojom::CompositorFrameMetadata>(
            input, output);
    EXPECT_TRUE(result);
  }

  SurfaceId surface_id(
      FrameSinkId(1337, 1234),
      LocalSurfaceId(0xfbadbeef, base::UnguessableToken::Create()));

  OffsetTagDefinition offset_tag_def;
  offset_tag_def.tag = OffsetTag(base::Token(1, 1));
  offset_tag_def.provider = SurfaceRange(surface_id);
  offset_tag_def.constraints.min_offset = {-20.4f, -89.3f};
  offset_tag_def.constraints.max_offset = {60.4f, 489.3f};

  input.offset_tag_definitions.push_back((offset_tag_def));
  {
    // There is no corresponding Surfacerange entry in `referenced_surfaces` so
    // this should fail deserialization.
    CompositorFrameMetadata output;
    bool result =
        mojo::test::SerializeAndDeserialize<mojom::CompositorFrameMetadata>(
            input, output);
    EXPECT_FALSE(result);
  }
}

TEST_F(StructTraitsTest, RenderPass) {
  // The CopyOutputRequest struct traits require a TaskRunner.
  base::test::TaskEnvironment task_environment;

  constexpr CompositorRenderPassId kRenderPassId{3u};
  constexpr gfx::Rect kOutputRect(45, 22, 120, 13);
  constexpr gfx::Transform kTransformToRoot =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  constexpr gfx::Rect kDamageRect(56, 123, 19, 43);
  const std::optional<gfx::RRectF> kBackdropFilterBounds(
      {10, 20, 130, 140, 1, 2, 3, 4, 5, 6, 7, 8});
  constexpr SubtreeCaptureId kSubtreeCaptureId(base::Token(0u, 22u));
  constexpr bool kHasTransparentBackground = true;
  constexpr bool kCacheRenderPass = true;
  constexpr bool kHasDamageFromContributingContent = true;
  constexpr bool kGenerateMipmap = true;
  constexpr bool kHasPerQuadDamage = true;

  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBlurFilter(0.f));
  filters.Append(cc::FilterOperation::CreateZoomFilter(2.0f, 1));
  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateSaturateFilter(4.f));
  backdrop_filters.Append(cc::FilterOperation::CreateZoomFilter(2.0f, 1));
  backdrop_filters.Append(cc::FilterOperation::CreateSaturateFilter(2.f));

  auto input = CompositorRenderPass::Create();
  input->SetAll(kRenderPassId, kOutputRect, kDamageRect, kTransformToRoot,
                filters, backdrop_filters, kBackdropFilterBounds,
                kSubtreeCaptureId, kOutputRect.size(),
                ViewTransitionElementResourceId(), kHasTransparentBackground,
                kCacheRenderPass, kHasDamageFromContributingContent,
                kGenerateMipmap, kHasPerQuadDamage);
  input->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());
  constexpr gfx::Rect kCopyOutputArea(24, 42, 75, 57);
  input->copy_requests.back()->set_area(kCopyOutputArea);

  SharedQuadState* shared_state_1 = input->CreateAndAppendSharedQuadState();
  shared_state_1->SetAll(
      gfx::Transform::RowMajor(16.1f, 15.3f, 14.3f, 13.7f, 12.2f, 11.4f, 10.4f,
                               9.8f, 8.1f, 7.3f, 6.3f, 5.7f, 4.8f, 3.4f, 2.4f,
                               1.2f),
      gfx::Rect(1, 2), gfx::Rect(1337, 5679, 9101112, 131415),
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(5.f, 6.f, 70.f, 89.f), 10.f)),
      gfx::Rect(1357, 2468, 121314, 1337), /*contents_opaque=*/true,
      /*opacity_f=*/2, SkBlendMode::kSrcOver, /*sorting_context=*/1,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  SharedQuadState* shared_state_2 = input->CreateAndAppendSharedQuadState();
  shared_state_2->SetAll(
      gfx::Transform::RowMajor(1.1f, 2.3f, 3.3f, 4.7f, 5.2f, 6.4f, 7.4f, 8.8f,
                               9.1f, 10.3f, 11.3f, 12.7f, 13.8f, 14.4f, 15.4f,
                               16.2f),
      gfx::Rect(1337, 1234), gfx::Rect(1234, 5678, 9101112, 13141516),
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(23.f, 45.f, 60.f, 70.f), 8.f)),
      gfx::Rect(1357, 2468, 121314, 1337), /*contents_opaque=*/true,
      /*opacity_f=*/2, SkBlendMode::kSrcOver, /*sorting_context=*/1,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  // This quad uses the first shared quad state. The next two quads use the
  // second shared quad state.
  DebugBorderDrawQuad* debug_quad =
      input->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  const gfx::Rect debug_quad_rect(12, 56, 89, 10);
  debug_quad->SetNew(shared_state_1, debug_quad_rect, debug_quad_rect,
                     SkColors::kBlue, 1337);

  SolidColorDrawQuad* color_quad =
      input->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  const gfx::Rect color_quad_rect(123, 456, 789, 101);
  color_quad->SetNew(shared_state_2, color_quad_rect, color_quad_rect,
                     SkColors::kRed, true);

  SurfaceDrawQuad* surface_quad =
      input->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  const gfx::Rect surface_quad_rect(1337, 2448, 1234, 5678);
  surface_quad->SetNew(
      shared_state_2, surface_quad_rect, surface_quad_rect,
      SurfaceRange(
          std::nullopt,
          SurfaceId(FrameSinkId(1337, 1234),
                    LocalSurfaceId(1234, base::UnguessableToken::Create()))),
      SkColors::kYellow, false);
  // Test non-default values.
  surface_quad->is_reflection = !surface_quad->is_reflection;
  surface_quad->allow_merge = !surface_quad->allow_merge;

  std::unique_ptr<CompositorRenderPass> output;
  mojo::test::SerializeAndDeserialize<mojom::CompositorRenderPass>(input,
                                                                   output);

  EXPECT_EQ(input->quad_list.size(), output->quad_list.size());
  EXPECT_EQ(input->shared_quad_state_list.size(),
            output->shared_quad_state_list.size());
  EXPECT_EQ(kRenderPassId, output->id);
  EXPECT_EQ(kOutputRect, output->output_rect);
  EXPECT_EQ(kDamageRect, output->damage_rect);
  EXPECT_EQ(kTransformToRoot, output->transform_to_root_target);
  EXPECT_EQ(kHasTransparentBackground, output->has_transparent_background);
  EXPECT_EQ(filters, output->filters);
  EXPECT_EQ(backdrop_filters, output->backdrop_filters);
  EXPECT_EQ(kBackdropFilterBounds, output->backdrop_filter_bounds);
  EXPECT_EQ(kSubtreeCaptureId, output->subtree_capture_id);
  EXPECT_EQ(kCacheRenderPass, output->cache_render_pass);
  EXPECT_EQ(kHasDamageFromContributingContent,
            output->has_damage_from_contributing_content);
  EXPECT_EQ(kHasPerQuadDamage, output->has_per_quad_damage);
  EXPECT_EQ(kGenerateMipmap, output->generate_mipmap);
  ASSERT_EQ(1u, output->copy_requests.size());
  EXPECT_EQ(kCopyOutputArea, output->copy_requests.front()->area());

  SharedQuadState* out_sqs1 = output->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(shared_state_1->quad_to_target_transform,
            out_sqs1->quad_to_target_transform);
  EXPECT_EQ(shared_state_1->quad_layer_rect, out_sqs1->quad_layer_rect);
  EXPECT_EQ(shared_state_1->visible_quad_layer_rect,
            out_sqs1->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_1->mask_filter_info, out_sqs1->mask_filter_info);
  EXPECT_EQ(shared_state_1->clip_rect, out_sqs1->clip_rect);
  EXPECT_EQ(shared_state_1->opacity, out_sqs1->opacity);
  EXPECT_EQ(shared_state_1->blend_mode, out_sqs1->blend_mode);
  EXPECT_EQ(shared_state_1->sorting_context_id, out_sqs1->sorting_context_id);

  SharedQuadState* out_sqs2 = output->shared_quad_state_list.ElementAt(1);
  EXPECT_EQ(shared_state_2->quad_to_target_transform,
            out_sqs2->quad_to_target_transform);
  EXPECT_EQ(shared_state_2->quad_layer_rect, out_sqs2->quad_layer_rect);
  EXPECT_EQ(shared_state_2->visible_quad_layer_rect,
            out_sqs2->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_2->mask_filter_info, out_sqs2->mask_filter_info);
  EXPECT_EQ(shared_state_2->clip_rect, out_sqs2->clip_rect);
  EXPECT_EQ(shared_state_2->opacity, out_sqs2->opacity);
  EXPECT_EQ(shared_state_2->blend_mode, out_sqs2->blend_mode);
  EXPECT_EQ(shared_state_2->sorting_context_id, out_sqs2->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_quad =
      DebugBorderDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
  EXPECT_EQ(out_debug_quad->shared_quad_state, out_sqs1);
  EXPECT_EQ(debug_quad->rect, out_debug_quad->rect);
  EXPECT_EQ(debug_quad->visible_rect, out_debug_quad->visible_rect);
  EXPECT_EQ(debug_quad->color, out_debug_quad->color);
  EXPECT_EQ(debug_quad->width, out_debug_quad->width);

  const SolidColorDrawQuad* out_color_quad =
      SolidColorDrawQuad::MaterialCast(output->quad_list.ElementAt(1));
  EXPECT_EQ(out_color_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(color_quad->rect, out_color_quad->rect);
  EXPECT_EQ(color_quad->visible_rect, out_color_quad->visible_rect);
  EXPECT_EQ(color_quad->color, out_color_quad->color);
  EXPECT_EQ(color_quad->force_anti_aliasing_off,
            out_color_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_surface_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(2));
  EXPECT_EQ(out_surface_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(surface_quad->rect, out_surface_quad->rect);
  EXPECT_EQ(surface_quad->visible_rect, out_surface_quad->visible_rect);
  EXPECT_EQ(surface_quad->surface_range, out_surface_quad->surface_range);
  EXPECT_EQ(surface_quad->default_background_color,
            out_surface_quad->default_background_color);
  EXPECT_EQ(surface_quad->stretch_content_to_fill_bounds,
            out_surface_quad->stretch_content_to_fill_bounds);
  EXPECT_EQ(surface_quad->allow_merge, out_surface_quad->allow_merge);
  EXPECT_EQ(surface_quad->is_reflection, out_surface_quad->is_reflection);
}

TEST_F(StructTraitsTest, RenderPassWithEmptySharedQuadStateList) {
  constexpr CompositorRenderPassId kRenderPassId{3u};
  constexpr gfx::Rect kOutputRect(45, 22, 120, 13);
  constexpr gfx::Rect kDamageRect(56, 123, 19, 43);
  constexpr gfx::Transform kTransformToRoot =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  const std::optional<gfx::RRectF> kBackdropFilterBounds;
  constexpr SubtreeCaptureId kEmptySubtreeCaptureId;
  constexpr bool kHasTransparentBackground = true;
  constexpr bool kCacheRenderPass = false;
  constexpr bool kHasDamageFromContributingContent = false;
  constexpr bool kGenerateMipmap = false;
  constexpr bool kHasPerQuadDamage = false;

  auto input = CompositorRenderPass::Create();
  input->SetAll(kRenderPassId, kOutputRect, kDamageRect, kTransformToRoot,
                cc::FilterOperations(), cc::FilterOperations(),
                kBackdropFilterBounds, kEmptySubtreeCaptureId,
                kOutputRect.size(), ViewTransitionElementResourceId(),
                kHasTransparentBackground, kCacheRenderPass,
                kHasDamageFromContributingContent, kGenerateMipmap,
                kHasPerQuadDamage);

  // Unlike the previous test, don't add any quads to the list; we need to
  // verify that the serialization code can deal with that.
  std::unique_ptr<CompositorRenderPass> output;
  mojo::test::SerializeAndDeserialize<mojom::CompositorRenderPass>(input,
                                                                   output);

  EXPECT_EQ(input->quad_list.size(), output->quad_list.size());
  EXPECT_EQ(input->shared_quad_state_list.size(),
            output->shared_quad_state_list.size());
  EXPECT_EQ(kRenderPassId, output->id);
  EXPECT_EQ(kOutputRect, output->output_rect);
  EXPECT_EQ(kDamageRect, output->damage_rect);
  EXPECT_EQ(kTransformToRoot, output->transform_to_root_target);
  EXPECT_EQ(kBackdropFilterBounds, output->backdrop_filter_bounds);
  EXPECT_EQ(kEmptySubtreeCaptureId, output->subtree_capture_id);
  EXPECT_EQ(kOutputRect.size(), output->subtree_size);
  EXPECT_FALSE(output->subtree_capture_id.is_valid());
  EXPECT_EQ(kHasTransparentBackground, output->has_transparent_background);
}

TEST_F(StructTraitsTest, QuadListBasic) {
  auto render_pass = CompositorRenderPass::Create();
  render_pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                      gfx::Transform());

  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();

  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor4f color1 = SkColors::kRed;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const SkColor4f color2 = SkColors::kWhite;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  const gfx::Rect rect3(1029, 3847, 5610, 2938);
  const SurfaceId primary_surface_id(
      FrameSinkId(1234, 4321),
      LocalSurfaceId(5678, base::UnguessableToken::Create()));
  const SurfaceId fallback_surface_id(
      FrameSinkId(2468, 1357),
      LocalSurfaceId(1234, base::UnguessableToken::Create()));
  SurfaceDrawQuad* primary_surface_quad =
      render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  primary_surface_quad->SetNew(
      sqs, rect3, rect3, SurfaceRange(fallback_surface_id, primary_surface_id),
      SkColors::kBlue, false);

  const gfx::Rect rect4(1234, 5678, 91012, 13141);
  const bool needs_blending = true;
  const ResourceId resource_id4(1337);
  const CompositorRenderPassId render_pass_id{1234u};
  const gfx::RectF mask_uv_rect(0, 0, 1337.1f, 1234.2f);
  const gfx::Size mask_texture_size(1234, 5678);
  gfx::Vector2dF filters_scale(1234.1f, 4321.2f);
  gfx::PointF filters_origin(8765.4f, 4567.8f);
  gfx::RectF tex_coord_rect(1.f, 1.f, 1234.f, 5678.f);
  const float backdrop_filter_quality = 1.0f;
  const bool intersects_damage_under = false;

  CompositorRenderPassDrawQuad* render_pass_quad =
      render_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  render_pass_quad->SetAll(sqs, rect4, rect4, needs_blending, render_pass_id,
                           resource_id4, mask_uv_rect, mask_texture_size,
                           filters_scale, filters_origin, tex_coord_rect,
                           force_anti_aliasing_off, backdrop_filter_quality,
                           intersects_damage_under);

  const gfx::Rect rect5(123, 567, 91011, 13141);
  const ResourceId resource_id5(1337);
  const bool premultiplied_alpha = true;

  const gfx::PointF uv_top_left(12.1f, 34.2f);
  const gfx::PointF uv_bottom_right(56.3f, 78.4f);
  const SkColor4f background_color = SkColors::kGreen;
  const bool y_flipped = true;
  const bool nearest_neighbor = true;
  const bool secure_output_only = true;
  const gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  const gfx::Size resource_size_in_pixels5(1234, 5678);
  TextureDrawQuad* texture_draw_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  texture_draw_quad->SetAll(
      sqs, rect5, rect5, needs_blending, resource_id5, resource_size_in_pixels5,
      premultiplied_alpha, uv_top_left, uv_bottom_right, background_color,
      y_flipped, nearest_neighbor, secure_output_only, protected_video_type);
  // Create a stream video TextureDrawQuad.
  const gfx::Rect rect6(321, 765, 11109, 151413);
  const bool needs_blending6 = false;
  const ResourceId resource_id6(1234);
  const gfx::Size resource_size_in_pixels6(1234, 5678);
  TextureDrawQuad* stream_video_draw_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  stream_video_draw_quad->SetAll(
      sqs, rect6, rect6, needs_blending6, resource_id6,
      resource_size_in_pixels6, false, uv_top_left, uv_bottom_right,
      SkColors::kTransparent, false, false, false, protected_video_type);
  stream_video_draw_quad->is_stream_video = true;

  // Create a TextureDrawQuad with rounded-display masks.
  const gfx::Rect rect7(421, 865, 11109, 151413);
  const bool needs_blending7 = false;
  const ResourceId resource_id7(4834);
  const gfx::Size resource_size_in_pixels7(12894, 8878);
  const int origin_rounded_display_mask_radius = 10;
  const int other_rounded_display_mask_radius = 15;
  const bool is_horizontally_positioned = false;

  TextureDrawQuad* rounded_display_mask_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  rounded_display_mask_quad->SetAll(
      sqs, rect7, rect7, needs_blending7, resource_id7,
      resource_size_in_pixels7, false, uv_top_left, uv_bottom_right,
      SkColors::kTransparent, false, false, false, protected_video_type);
  rounded_display_mask_quad->rounded_display_masks_info =
      TextureDrawQuad::RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
          origin_rounded_display_mask_radius, other_rounded_display_mask_radius,
          is_horizontally_positioned);

  std::unique_ptr<CompositorRenderPass> output;
  mojo::test::SerializeAndDeserialize<mojom::CompositorRenderPass>(render_pass,
                                                                   output);

  ASSERT_EQ(render_pass->quad_list.size(), output->quad_list.size());

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_FALSE(out_debug_border_draw_quad->needs_blending);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(output->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_FALSE(out_solid_color_draw_quad->needs_blending);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_primary_surface_draw_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(2));
  EXPECT_EQ(rect3, out_primary_surface_draw_quad->rect);
  EXPECT_EQ(rect3, out_primary_surface_draw_quad->visible_rect);
  EXPECT_TRUE(out_primary_surface_draw_quad->needs_blending);
  EXPECT_EQ(primary_surface_id,
            out_primary_surface_draw_quad->surface_range.end());
  EXPECT_EQ(SkColors::kBlue,
            out_primary_surface_draw_quad->default_background_color);
  EXPECT_EQ(fallback_surface_id,
            out_primary_surface_draw_quad->surface_range.start());

  const CompositorRenderPassDrawQuad* out_render_pass_draw_quad =
      CompositorRenderPassDrawQuad::MaterialCast(
          output->quad_list.ElementAt(3));
  EXPECT_EQ(rect4, out_render_pass_draw_quad->rect);
  EXPECT_EQ(rect4, out_render_pass_draw_quad->visible_rect);
  EXPECT_EQ(render_pass_id, out_render_pass_draw_quad->render_pass_id);
  EXPECT_EQ(resource_id4, out_render_pass_draw_quad->mask_resource_id());
  EXPECT_EQ(mask_uv_rect, out_render_pass_draw_quad->mask_uv_rect);
  EXPECT_EQ(mask_texture_size, out_render_pass_draw_quad->mask_texture_size);
  EXPECT_EQ(filters_scale, out_render_pass_draw_quad->filters_scale);
  EXPECT_EQ(filters_origin, out_render_pass_draw_quad->filters_origin);
  EXPECT_EQ(tex_coord_rect, out_render_pass_draw_quad->tex_coord_rect);
  EXPECT_EQ(force_anti_aliasing_off,
            out_render_pass_draw_quad->force_anti_aliasing_off);
  EXPECT_EQ(backdrop_filter_quality,
            out_render_pass_draw_quad->backdrop_filter_quality);
  EXPECT_EQ(intersects_damage_under,
            out_render_pass_draw_quad->intersects_damage_under);

  const TextureDrawQuad* out_texture_draw_quad =
      TextureDrawQuad::MaterialCast(output->quad_list.ElementAt(4));
  EXPECT_EQ(rect5, out_texture_draw_quad->rect);
  EXPECT_EQ(rect5, out_texture_draw_quad->visible_rect);
  EXPECT_EQ(needs_blending, out_texture_draw_quad->needs_blending);
  EXPECT_EQ(resource_id5, out_texture_draw_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels5,
            out_texture_draw_quad->resource_size_in_pixels());
  EXPECT_EQ(premultiplied_alpha, out_texture_draw_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, out_texture_draw_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, out_texture_draw_quad->uv_bottom_right);
  EXPECT_EQ(background_color, out_texture_draw_quad->background_color);
  EXPECT_EQ(y_flipped, out_texture_draw_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, out_texture_draw_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, out_texture_draw_quad->secure_output_only);

  const TextureDrawQuad* out_stream_video_draw_quad =
      TextureDrawQuad::MaterialCast(output->quad_list.ElementAt(5));
  EXPECT_TRUE(out_stream_video_draw_quad->is_stream_video);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->rect);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->visible_rect);
  EXPECT_EQ(needs_blending6, out_stream_video_draw_quad->needs_blending);
  EXPECT_EQ(resource_id6, out_stream_video_draw_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels6,
            out_stream_video_draw_quad->resource_size_in_pixels());
  EXPECT_EQ(uv_top_left, out_stream_video_draw_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, out_stream_video_draw_quad->uv_bottom_right);

  const TextureDrawQuad* out_rounded_display_mask_quad =
      TextureDrawQuad::MaterialCast(output->quad_list.ElementAt(6));
  EXPECT_FALSE(out_rounded_display_mask_quad->is_stream_video);
  EXPECT_EQ(rect7, out_rounded_display_mask_quad->rect);
  EXPECT_EQ(rect7, out_rounded_display_mask_quad->visible_rect);
  EXPECT_EQ(needs_blending7, out_rounded_display_mask_quad->needs_blending);
  EXPECT_EQ(resource_id7, out_rounded_display_mask_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels7,
            out_rounded_display_mask_quad->resource_size_in_pixels());
  EXPECT_EQ(uv_top_left, out_rounded_display_mask_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, out_rounded_display_mask_quad->uv_bottom_right);
  EXPECT_EQ(origin_rounded_display_mask_radius,
            out_rounded_display_mask_quad->rounded_display_masks_info
                .radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                           kOriginRoundedDisplayMaskIndex]);
  EXPECT_EQ(other_rounded_display_mask_radius,
            out_rounded_display_mask_quad->rounded_display_masks_info
                .radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                           kOtherRoundedDisplayMaskIndex]);
  EXPECT_EQ(is_horizontally_positioned,
            out_rounded_display_mask_quad->rounded_display_masks_info
                .is_horizontally_positioned);
}

TEST_F(StructTraitsTest, SurfaceId) {
  static constexpr FrameSinkId frame_sink_id(1337, 1234);
  static LocalSurfaceId local_surface_id(0xfbadbeef,
                                         base::UnguessableToken::Create());
  SurfaceId input(frame_sink_id, local_surface_id);
  SurfaceId output;
  mojo::test::SerializeAndDeserialize<mojom::SurfaceId>(input, output);
  EXPECT_EQ(frame_sink_id, output.frame_sink_id());
  EXPECT_EQ(local_surface_id, output.local_surface_id());
}

TEST_F(StructTraitsTest, OffsetTag) {
  constexpr OffsetTag input(base::Token(1, 1));
  OffsetTag output;

  mojo::test::SerializeAndDeserialize<mojom::OffsetTag>(input, output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, OffsetTagValue) {
  constexpr OffsetTag kTag(base::Token(1, 1));
  OffsetTagValue input = {kTag, {5.0f, 7.7f}};
  OffsetTagValue output;

  mojo::test::SerializeAndDeserialize<mojom::OffsetTagValue>(input, output);
  EXPECT_EQ(input.tag, output.tag);
  EXPECT_EQ(input.offset, output.offset);
}

TEST_F(StructTraitsTest, OffsetTagDefinition) {
  SurfaceId surface_id(
      FrameSinkId(1337, 1234),
      LocalSurfaceId(0xfbadbeef, base::UnguessableToken::Create()));

  OffsetTagDefinition input;
  input.tag = OffsetTag(base::Token(1, 1));
  input.provider = SurfaceRange(surface_id);
  input.constraints.min_offset = {-20.4f, -89.3f};
  input.constraints.max_offset = {60.4f, 489.3f};

  OffsetTagDefinition output;
  mojo::test::SerializeAndDeserialize<mojom::OffsetTagDefinition>(input,
                                                                  output);
  EXPECT_EQ(input.tag, output.tag);
  EXPECT_EQ(input.provider, output.provider);
  EXPECT_EQ(input.constraints.min_offset, output.constraints.min_offset);
  EXPECT_EQ(input.constraints.max_offset, output.constraints.max_offset);
}

TEST_F(StructTraitsTest, TransferableResource) {
  const ResourceId id(1337);
  const SharedImageFormat format = SinglePlaneFormat::kALPHA_8;
  const gfx::Size size(1234, 5678);
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const uint32_t texture_target = 1337;
  const TransferableResource::SynchronizationType sync_type =
      TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  const bool is_software = false;
  const bool is_overlay_candidate = true;

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.mailbox.SetName(mailbox_name);
  mailbox_holder.sync_token = gpu::SyncToken(command_buffer_namespace,
                                             command_buffer_id, release_count);
  mailbox_holder.sync_token.SetVerifyFlush();
  mailbox_holder.texture_target = texture_target;
  TransferableResource input;
  input.id = id;
  input.format = format;
  input.size = size;
  input.set_mailbox(mailbox_holder.mailbox);
  input.set_sync_token(mailbox_holder.sync_token);
  input.set_texture_target(mailbox_holder.texture_target);
  input.synchronization_type = sync_type;
  input.is_software = is_software;
  input.is_overlay_candidate = is_overlay_candidate;

  TransferableResource output;
  mojo::test::SerializeAndDeserialize<mojom::TransferableResource>(input,
                                                                   output);

  EXPECT_EQ(id, output.id);
  EXPECT_EQ(format, output.format);
  EXPECT_EQ(size, output.size);
  EXPECT_EQ(mailbox_holder.mailbox, output.mailbox());
  EXPECT_EQ(mailbox_holder.sync_token, output.sync_token());
  EXPECT_EQ(mailbox_holder.texture_target, output.texture_target());
  EXPECT_EQ(sync_type, output.synchronization_type);
  EXPECT_EQ(is_software, output.is_software);
  EXPECT_EQ(is_overlay_candidate, output.is_overlay_candidate);
}

TEST_F(StructTraitsTest, SharedImageFormatWithSinglePlane) {
  SharedImageFormat input = SinglePlaneFormat::kR_8;
  SharedImageFormat output;
  mojo::test::SerializeAndDeserialize<mojom::SharedImageFormat>(input, output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, SharedImageFormatWithMultiPlane) {
  SharedImageFormat input = MultiPlaneFormat::kNV12;
  SharedImageFormat output;
  mojo::test::SerializeAndDeserialize<mojom::SharedImageFormat>(input, output);
  EXPECT_EQ(input, output);
}

TEST_F(StructTraitsTest, SharedImageFormatWithUnknownPlane) {
  SharedImageFormat input = SharedImageFormat();
  SharedImageFormat output;
  EXPECT_CHECK_DEATH(
      mojo::test::SerializeAndDeserialize<mojom::SharedImageFormat>(input,
                                                                    output));
}

TEST_F(StructTraitsTest, CopyOutputResult_EmptyBitmap) {
  auto input = std::make_unique<CopyOutputResult>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory, gfx::Rect(), false);
  std::unique_ptr<CopyOutputResult> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputResult>(input, output);

  EXPECT_TRUE(output->IsEmpty());
  EXPECT_EQ(output->format(), CopyOutputResult::Format::RGBA);
  EXPECT_EQ(output->destination(),
            CopyOutputResult::Destination::kSystemMemory);
  EXPECT_TRUE(output->rect().IsEmpty());
  auto scoped_bitmap = output->ScopedAccessSkBitmap();
  auto bitmap = scoped_bitmap.bitmap();
  EXPECT_FALSE(bitmap.readyToDraw());
  EXPECT_EQ(output->GetTextureResult(), nullptr);
}

TEST_F(StructTraitsTest, CopyOutputResult_EmptyTexture) {
  base::test::TaskEnvironment task_environment;

  auto input = std::make_unique<CopyOutputResult>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kNativeTextures, gfx::Rect(),
      false);
  EXPECT_TRUE(input->IsEmpty());

  std::unique_ptr<CopyOutputResult> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputResult>(input, output);

  EXPECT_TRUE(output->IsEmpty());
  EXPECT_EQ(output->format(), CopyOutputResult::Format::RGBA);
  EXPECT_EQ(output->destination(),
            CopyOutputResult::Destination::kNativeTextures);
  EXPECT_TRUE(output->rect().IsEmpty());
  EXPECT_EQ(output->GetTextureResult(), nullptr);
}

TEST_F(StructTraitsTest, CopyOutputResult_Bitmap) {
  const gfx::Rect result_rect(42, 43, 7, 8);
  SkBitmap bitmap;
  const sk_sp<SkColorSpace> adobe_rgb =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB);
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(7, 8, adobe_rgb));
  bitmap.eraseARGB(123, 213, 77, 33);
  std::unique_ptr<CopyOutputResult> input =
      std::make_unique<CopyOutputSkBitmapResult>(result_rect,
                                                 std::move(bitmap));

  std::unique_ptr<CopyOutputResult> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputResult>(input, output);

  EXPECT_FALSE(output->IsEmpty());
  EXPECT_EQ(output->format(), CopyOutputResult::Format::RGBA);
  EXPECT_EQ(output->destination(),
            CopyOutputResult::Destination::kSystemMemory);
  EXPECT_EQ(output->rect(), result_rect);
  EXPECT_EQ(output->GetTextureResult(), nullptr);

  auto scoped_bitmap = output->ScopedAccessSkBitmap();
  auto out_bitmap = scoped_bitmap.bitmap();
  EXPECT_TRUE(out_bitmap.readyToDraw());
  EXPECT_EQ(out_bitmap.width(), result_rect.width());
  EXPECT_EQ(out_bitmap.height(), result_rect.height());

  // Check that the pixels are the same as the input and the color spaces are
  // equivalent.
  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(SkImageInfo::MakeN32Premul(7, 8, adobe_rgb));
  expected_bitmap.eraseARGB(123, 213, 77, 33);
  EXPECT_EQ(expected_bitmap.computeByteSize(), out_bitmap.computeByteSize());
  EXPECT_EQ(0, std::memcmp(expected_bitmap.getPixels(), out_bitmap.getPixels(),
                           expected_bitmap.computeByteSize()));
  EXPECT_TRUE(SkColorSpace::Equals(expected_bitmap.colorSpace(),
                                   out_bitmap.colorSpace()));
}

TEST_F(StructTraitsTest, CopyOutputResult_Texture) {
  base::test::TaskEnvironment task_environment;

  const gfx::Rect result_rect(12, 34, 56, 78);
  const gfx::ColorSpace result_color_space =
      gfx::ColorSpace::CreateDisplayP3D65();
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 3};
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x123),
                            71234838);
  sync_token.SetVerifyFlush();
  base::RunLoop run_loop;
  CopyOutputResult::ReleaseCallbacks release_callbacks;
  release_callbacks.push_back(base::BindOnce(
      [](base::OnceClosure quit_closure,
         const gpu::SyncToken& expected_sync_token,
         const gpu::SyncToken& sync_token, bool is_lost) {
        EXPECT_EQ(expected_sync_token, sync_token);
        EXPECT_TRUE(is_lost);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), sync_token));
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  std::unique_ptr<CopyOutputResult> input =
      std::make_unique<CopyOutputTextureResult>(
          CopyOutputResult::Format::RGBA, result_rect,
          CopyOutputResult::TextureResult(mailbox, result_color_space),
          std::move(release_callbacks));

  std::unique_ptr<CopyOutputResult> output;
  mojo::test::SerializeAndDeserialize<mojom::CopyOutputResult>(input, output);

  EXPECT_FALSE(output->IsEmpty());
  EXPECT_EQ(output->format(), CopyOutputResult::Format::RGBA);
  EXPECT_EQ(output->destination(),
            CopyOutputResult::Destination::kNativeTextures);
  EXPECT_EQ(output->rect(), result_rect);
  ASSERT_NE(output->GetTextureResult(), nullptr);
  EXPECT_EQ(output->GetTextureResult()->mailbox, mailbox);
  EXPECT_EQ(output->GetTextureResult()->color_space, result_color_space);

  CopyOutputResult::ReleaseCallbacks out_callbacks =
      output->TakeTextureOwnership();

  EXPECT_EQ(1u, out_callbacks.size());
  for (auto& cb : out_callbacks) {
    std::move(cb).Run(sync_token, true /* is_lost */);
  }

  // If the CopyOutputResult callback is called (which is the intended
  // behaviour), this will exit. Otherwise, this test will time out and fail.
  run_loop.Run();
}

}  // namespace viz
