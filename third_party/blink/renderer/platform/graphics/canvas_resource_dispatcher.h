// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_DISPATCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/graphics/resource_id_traits.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class CanvasResource;

class CanvasResourceDispatcherClient {
 public:
  virtual bool BeginFrame() = 0;
};

class PLATFORM_EXPORT CanvasResourceDispatcher
    : public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  static constexpr unsigned kMaxPendingCompositorFrames = 2;

  // We set a limit to the number of placeholder resources that have been posted
  // to the main thread but not yet received on that thread.
  static constexpr unsigned kMaxPendingPlaceholderResources = 50;

  base::WeakPtr<CanvasResourceDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  CanvasResourceDispatcherClient* Client() { return client_; }

  enum {
    kInvalidPlaceholderCanvasId = -1,
  };

  enum class AnimationState {
    // Animation should be active, and use the real sync signal from viz.
    kActive,

    // Animation should be active, but should use a synthetic sync signal.  This
    // is useful when viz won't provide us with one.
    kActiveWithSyntheticTiming,

    // Animation should be suspended.
    kSuspended,
  };

  // `task_runner` is the task runner this object is associated with and
  // executes on. `agent_group_scheduler_compositor_task_runner` is the
  // compositor task runner for the associated canvas element.
  CanvasResourceDispatcher(
      CanvasResourceDispatcherClient*,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner>
          agent_group_scheduler_compositor_task_runner,
      uint32_t client_id,
      uint32_t sink_id,
      int placeholder_canvas_id,
      const gfx::Size&);

  ~CanvasResourceDispatcher() override;
  void SetNeedsBeginFrame(bool);
  void SetAnimationState(AnimationState animation_state);
  AnimationState GetAnimationStateForTesting() const {
    return animation_state_;
  }
  bool NeedsBeginFrame() const { return needs_begin_frame_; }
  bool IsAnimationSuspended() const {
    return animation_state_ == AnimationState::kSuspended;
  }
  void DispatchFrame(scoped_refptr<CanvasResource>&&,
                     const SkIRect& damage_rect,
                     bool is_opaque);
  // virtual for mocking
  virtual void OnMainThreadReceivedImage();
  void ReplaceBeginFrameAck(const viz::BeginFrameArgs& args) {
    current_begin_frame_ack_ = viz::BeginFrameAck(args, true);
  }
  bool HasTooManyPendingFrames() const;

  void Reshape(const gfx::Size&);

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      Vector<viz::ReturnedResource> resources) final;
  void OnBeginFrame(const viz::BeginFrameArgs&,
                    const HashMap<uint32_t, viz::FrameTimingDetails>&,
                    Vector<viz::ReturnedResource> resources) final;
  void OnBeginFramePausedChanged(bool paused) final {}
  void ReclaimResources(Vector<viz::ReturnedResource> resources) final;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) final {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) final {}

  void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality);
  void SetPlaceholderCanvasDispatcher(int placeholder_canvas_id);

 private:
  friend class OffscreenCanvasPlaceholderTest;
  friend class CanvasResourceDispatcherTest;
  struct ExportedResource;

  using ExportedResourceMap =
      HashMap<viz::ResourceId, std::unique_ptr<ExportedResource>>;

  bool PrepareFrame(scoped_refptr<CanvasResource>&&,
                    const SkIRect& damage_rect,
                    bool is_opaque,
                    viz::CompositorFrame* frame);

  // Timer callback for synthetic OnBeginFrames.
  void OnFakeFrameTimer(TimerBase* timer);

  // Surface-related
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  const viz::FrameSinkId frame_sink_id_;

  gfx::Size size_;
  bool change_size_for_next_commit_;
  AnimationState animation_state_ = AnimationState::kActive;
  bool needs_begin_frame_ = false;
  unsigned pending_compositor_frames_ = 0;

  // Make sure that we're are / are not requesting `OnBeginFrame` callbacks and
  // are / are not generating synthetic OBFs via timer based on whether we need
  // a begin frame source or not.  It's okay to call this regardless if the
  // state has actually changed or not.
  void UpdateBeginFrameSource();

  bool VerifyImageSize(const gfx::Size&);
  void PostImageToPlaceholderIfNotBlocked(scoped_refptr<CanvasResource>&&,
                                          viz::ResourceId resource_id);
  // virtual for testing
  virtual void PostImageToPlaceholder(scoped_refptr<CanvasResource>&&,
                                      viz::ResourceId resource_id);

  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink_;
  mojo::Remote<mojom::blink::SurfaceEmbedder> surface_embedder_;
  mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient> receiver_{this};

  int placeholder_canvas_id_;

  viz::ResourceIdGenerator id_generator_;

  // Stores resources that have been exported to the compositor, to be released
  // when the compositor no longer requires them (or in the limit when this
  // instance is destroyed).
  ExportedResourceMap exported_resources_;

  viz::FrameTokenGenerator next_frame_token_;

  // The latest_unposted_resource_id_ always refers to the Id of the frame
  // resource used by the latest_unposted_resource_.
  scoped_refptr<CanvasResource> latest_unposted_resource_;
  viz::ResourceId latest_unposted_resource_id_;
  unsigned num_pending_placeholder_resources_;

  viz::BeginFrameAck current_begin_frame_ack_;

  raw_ptr<CanvasResourceDispatcherClient> client_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      agent_group_scheduler_compositor_task_runner_;

  TaskRunnerTimer<CanvasResourceDispatcher> fake_frame_timer_;

  base::WeakPtrFactory<CanvasResourceDispatcher> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_DISPATCHER_H_
