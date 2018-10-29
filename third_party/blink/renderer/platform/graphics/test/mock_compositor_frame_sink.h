// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_

#include "components/viz/common/quads/compositor_frame.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"

namespace blink {

// A CompositorFrameSink for inspecting the viz::CompositorFrame sent to
// SubmitcompositorFrame() or SubmitcompositorFrameSync(); an object of this
// class may be offered by a MockEmbeddedFrameSinkProvider.
class MockCompositorFrameSink : public viz::mojom::blink::CompositorFrameSink {
 public:
  MockCompositorFrameSink(
      viz::mojom::blink::CompositorFrameSinkRequest request,
      int num_expected_set_needs_begin_frame_on_construction)
      : binding_(this, std::move(request)) {
    EXPECT_CALL(*this, SetNeedsBeginFrame(true))
        .Times(num_expected_set_needs_begin_frame_on_construction);
  }

  // viz::mojom::blink::CompositorFrameSink implementation
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  MOCK_METHOD0(SetWantsAnimateOnlyBeginFrames, void(void));
  void SubmitCompositorFrame(const viz::LocalSurfaceId&,
                             viz::CompositorFrame frame,
                             viz::mojom::blink::HitTestRegionListPtr,
                             uint64_t) {
    SubmitCompositorFrame_(&frame);
  }
  MOCK_METHOD1(SubmitCompositorFrame_, void(viz::CompositorFrame*));
  void SubmitCompositorFrameSync(const viz::LocalSurfaceId&,
                                 viz::CompositorFrame frame,
                                 viz::mojom::blink::HitTestRegionListPtr,
                                 uint64_t,
                                 SubmitCompositorFrameSyncCallback cb) {
    SubmitCompositorFrameSync_(&frame);
    std::move(cb).Run(WTF::Vector<viz::ReturnedResource>());
  }
  MOCK_METHOD1(SubmitCompositorFrameSync_, void(viz::CompositorFrame*));
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));
  MOCK_METHOD2(DidAllocateSharedBitmap,
               void(mojo::ScopedSharedBufferHandle,
                    gpu::mojom::blink::MailboxPtr));
  MOCK_METHOD1(DidDeleteSharedBitmap, void(gpu::mojom::blink::MailboxPtr));

 private:
  mojo::Binding<viz::mojom::blink::CompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockCompositorFrameSink);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
