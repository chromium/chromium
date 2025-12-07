// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_

#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/performance_hint_utils.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "gpu/ipc/common/mailbox.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink-forward.h"

namespace blink {

// A CompositorFrameSink for inspecting the viz::CompositorFrame sent to
// SubmitcompositorFrame() or SubmitcompositorFrameSync(); an object of this
// class may be offered by a MockEmbeddedFrameSinkProvider.
class MockCompositorFrameSink : public viz::mojom::blink::CompositorFrameSink {
 public:
  MockCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::blink::CompositorFrameSink> receiver,
      int num_expected_set_needs_begin_frame_on_construction) {
    receiver_.Bind(std::move(receiver));
    EXPECT_CALL(*this, SetNeedsBeginFrame(true))
        .Times(num_expected_set_needs_begin_frame_on_construction);
    if (!num_expected_set_needs_begin_frame_on_construction)
      EXPECT_CALL(*this, SetNeedsBeginFrame(false)).Times(testing::AtLeast(0));
  }

  MockCompositorFrameSink(const MockCompositorFrameSink&) = delete;
  MockCompositorFrameSink& operator=(const MockCompositorFrameSink&) = delete;

  // viz::mojom::blink::CompositorFrameSink implementation
  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  MOCK_METHOD1(SetParams,
               void(viz::mojom::blink::CompositorFrameSinkParamsPtr));
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId&,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t) override {
    SubmitCompositorFrame_(&frame);
  }
  MOCK_METHOD1(SubmitCompositorFrame_, void(viz::CompositorFrame*));
  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));
  MOCK_METHOD0(NotifyNewLocalSurfaceIdExpectedWhilePaused, void(void));
  MOCK_METHOD1(SetPreferredFrameInterval, void(base::TimeDelta));
  MOCK_METHOD2(BindLayerContext,
               void(viz::mojom::blink::PendingLayerContextPtr,
                    viz::mojom::blink::LayerContextSettingsPtr));
  MOCK_METHOD1(SetThreads, void(const Vector<viz::Thread>&));

 private:
  mojo::Receiver<viz::mojom::blink::CompositorFrameSink> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_COMPOSITOR_FRAME_SINK_H_
