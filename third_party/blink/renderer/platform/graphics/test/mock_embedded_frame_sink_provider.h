// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_EMBEDDED_FRAME_SINK_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_EMBEDDED_FRAME_SINK_PROVIDER_H_

#include <memory>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class MockCompositorFrameSink;

// A Provider that creates and binds a MockCompositorFrameSink when requested.
class MockEmbeddedFrameSinkProvider
    : public mojom::blink::EmbeddedFrameSinkProvider {
 public:
  // EmbeddedFrameSinkProvider implementation.
  MOCK_METHOD3(RegisterEmbeddedFrameSink,
               void(const viz::FrameSinkId&,
                    const viz::FrameSinkId&,
                    mojom::blink::EmbeddedFrameSinkClientPtr));
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::blink::CompositorFrameSinkClientPtr client,
      viz::mojom::blink::CompositorFrameSinkRequest sink) override {
    mock_compositor_frame_sink_ = std::make_unique<MockCompositorFrameSink>(
        std::move(sink),
        num_expected_set_needs_begin_frame_on_sink_construction_);
    CreateCompositorFrameSink_(frame_sink_id);
  }
  MOCK_METHOD1(CreateCompositorFrameSink_, void(const viz::FrameSinkId&));
  MOCK_METHOD5(CreateSimpleCompositorFrameSink,
               void(const viz::FrameSinkId&,
                    const viz::FrameSinkId&,
                    mojom::blink::EmbeddedFrameSinkClientPtr,
                    viz::mojom::blink::CompositorFrameSinkClientPtr,
                    viz::mojom::blink::CompositorFrameSinkRequest));

  // Utility method to create a scoped EmbeddedFrameSinkProvider override.
  std::unique_ptr<TestingPlatformSupport::ScopedOverrideMojoInterface>
  CreateScopedOverrideMojoInterface(
      mojo::Binding<mojom::blink::EmbeddedFrameSinkProvider>* binding) {
    using mojom::blink::EmbeddedFrameSinkProvider;
    using mojom::blink::EmbeddedFrameSinkProviderRequest;

    return std::make_unique<
        TestingPlatformSupport::ScopedOverrideMojoInterface>(WTF::BindRepeating(
        [](mojo::Binding<EmbeddedFrameSinkProvider>* binding,
           const char* interface_name, mojo::ScopedMessagePipeHandle pipe) {
          if (strcmp(interface_name, EmbeddedFrameSinkProvider::Name_))
            return;
          if (binding->is_bound())
            binding->Unbind();
          binding->Bind(EmbeddedFrameSinkProviderRequest(std::move(pipe)));
        },
        WTF::Unretained(binding)));
  }
  MockCompositorFrameSink& mock_compositor_frame_sink() const {
    return *mock_compositor_frame_sink_;
  }
  void set_num_expected_set_needs_begin_frame_on_sink_construction(int value) {
    num_expected_set_needs_begin_frame_on_sink_construction_ = value;
  }

 private:
  std::unique_ptr<MockCompositorFrameSink> mock_compositor_frame_sink_;

  // Amount of SetNeedsBeginFrame() calls to be expected by
  // MockCompositorFrameSink, passed on its construction.
  int num_expected_set_needs_begin_frame_on_sink_construction_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_EMBEDDED_FRAME_SINK_PROVIDER_H_
