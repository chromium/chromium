// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

struct PLATFORM_EXPORT BeginFrameProviderParams final {
  viz::FrameSinkId parent_frame_sink_id;
  viz::FrameSinkId frame_sink_id;
};

class PLATFORM_EXPORT BeginFrameProviderClient : public GarbageCollectedMixin {
 public:
  virtual void BeginFrame(const viz::BeginFrameArgs&) = 0;
  virtual ~BeginFrameProviderClient() = default;
};

class PLATFORM_EXPORT BeginFrameProvider
    : public GarbageCollected<BeginFrameProvider>,
      public viz::mojom::blink::CompositorFrameSinkClient,
      public mojom::blink::EmbeddedFrameSinkClient {
 public:
  explicit BeginFrameProvider(
      const BeginFrameProviderParams& begin_frame_provider_params,
      BeginFrameProviderClient*,
      ContextLifecycleNotifier*);

  void CreateCompositorFrameSinkIfNeeded();

  void RequestBeginFrame();
  void FinishBeginFrame(const viz::BeginFrameArgs&);

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) final {
    NOTIMPLEMENTED();
  }
  void OnBeginFrame(
      const viz::BeginFrameArgs&,
      const WTF::HashMap<uint32_t, viz::FrameTimingDetails>&) final;
  void OnBeginFramePausedChanged(bool paused) final {}
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) final {
    NOTIMPLEMENTED();
  }

  // viz::mojom::blink::EmbeddedFrameSinkClient implementation.
  void BindSurfaceEmbedder(
      mojo::PendingReceiver<mojom::blink::SurfaceEmbedder> receiver) override {
    NOTIMPLEMENTED();
  }

  void ResetCompositorFrameSink();

  bool IsValidFrameProvider();

  void Trace(Visitor*) const;

  ~BeginFrameProvider() override = default;

 private:
  void OnMojoConnectionError(uint32_t custom_reason,
                             const std::string& description);

  bool needs_begin_frame_;
  bool requested_needs_begin_frame_;

  HeapMojoReceiver<viz::mojom::blink::CompositorFrameSinkClient,
                   BeginFrameProvider,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      cfs_receiver_;

  HeapMojoReceiver<mojom::blink::EmbeddedFrameSinkClient,
                   BeginFrameProvider,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      efs_receiver_;
  viz::FrameSinkId frame_sink_id_;
  viz::FrameSinkId parent_frame_sink_id_;
  HeapMojoRemote<viz::mojom::blink::CompositorFrameSink,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      compositor_frame_sink_;
  Member<BeginFrameProviderClient> begin_frame_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_
