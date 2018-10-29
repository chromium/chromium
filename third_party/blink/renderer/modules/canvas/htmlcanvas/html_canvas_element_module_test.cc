// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"

using ::testing::_;
using ::testing::Values;

namespace blink {

namespace {

// This class allows for overriding GenerateFrameSinkId() so that the
// HTMLCanvasElement's SurfaceLayerBridge will get a syntactically correct
// FrameSinkId.
class TestingPlatformSupportWithGenerateFrameSinkId
    : public TestingPlatformSupport {
 public:
  viz::FrameSinkId GenerateFrameSinkId() override {
    // Doesn't matter what we return as long as is not zero.
    constexpr uint32_t kClientId = 2;
    constexpr uint32_t kSinkId = 1;
    return viz::FrameSinkId(kClientId, kSinkId);
  }
};

}  // unnamed namespace

class HTMLCanvasElementModuleTest : public PageTestBase,
                                    public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<body><canvas id='c'></canvas></body>");
    canvas_element_ = ToHTMLCanvasElement(GetElementById("c"));
  }

  HTMLCanvasElement& canvas_element() const { return *canvas_element_; }
  OffscreenCanvas* TransferControlToOffscreen(ExceptionState& exception_state) {
    return HTMLCanvasElementModule::TransferControlToOffscreenInternal(
        canvas_element(), exception_state);
  }

  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<CanvasRenderingContext> context_;
};

// Tests if the Canvas Id is associated correctly.
TEST_F(HTMLCanvasElementModuleTest, TransferControlToOffscreen) {
  NonThrowableExceptionState exception_state;
  const OffscreenCanvas* offscreen_canvas =
      TransferControlToOffscreen(exception_state);
  const DOMNodeId canvas_id = offscreen_canvas->PlaceholderCanvasId();
  EXPECT_EQ(canvas_id, DOMNodeIds::IdForNode(&(canvas_element())));
}

// Verifies that a lowLatency canvas has the appropriate opacity/blending
// information sent to the CompositorFrameSink.
TEST_P(HTMLCanvasElementModuleTest, LowLatencyCanvasCompositorFrameOpacity) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithGenerateFrameSinkId>
      platform;

  // To intercept SubmitCompositorFrame/SubmitCompositorFrameSync messages sent
  // by a canvas's CanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Binding<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_binding(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_binding);

  const bool context_alpha = GetParam();
  CanvasContextCreationAttributesCore attrs;
  attrs.alpha = context_alpha;
  attrs.low_latency = true;
  // |context_| creation triggers a SurfaceLayerBridge creation which connects
  // to a MockEmbeddedFrameSinkProvider to create a new CompositorFrameSink,
  // that will receive a SetNeedsBeginFrame() upon construction.
  mock_embedded_frame_sink_provider
      .set_num_expected_set_needs_begin_frame_on_sink_construction(1);
  EXPECT_CALL(mock_embedded_frame_sink_provider, CreateCompositorFrameSink_(_));
  context_ = canvas_element().GetCanvasRenderingContext(String("2d"), attrs);
  EXPECT_EQ(context_->CreationAttributes().alpha, attrs.alpha);
  EXPECT_TRUE(context_->CreationAttributes().low_latency);
  EXPECT_TRUE(canvas_element().LowLatencyEnabled());
  EXPECT_TRUE(canvas_element().SurfaceLayerBridge());
  platform->RunUntilIdle();

  // This call simulates having drawn something before FinalizeFrame().
  canvas_element().DidDraw();

  ::testing::InSequence s;
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              DidAllocateSharedBitmap(_, _));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>(
          ::testing::Invoke([context_alpha](const viz::CompositorFrame* frame) {
            ASSERT_EQ(frame->render_pass_list.size(), 1u);

            const auto& quad_list = frame->render_pass_list[0]->quad_list;
            ASSERT_EQ(quad_list.size(), 1u);
            EXPECT_EQ(quad_list.front()->needs_blending, context_alpha);

            const auto& shared_quad_state_list =
                frame->render_pass_list[0]->shared_quad_state_list;
            ASSERT_EQ(shared_quad_state_list.size(), 1u);
            EXPECT_NE(shared_quad_state_list.front()->are_contents_opaque,
                      context_alpha);
          })));
  canvas_element().FinalizeFrame();
  platform->RunUntilIdle();
}

INSTANTIATE_TEST_CASE_P(, HTMLCanvasElementModuleTest, Values(true, false));
}  // namespace blink
