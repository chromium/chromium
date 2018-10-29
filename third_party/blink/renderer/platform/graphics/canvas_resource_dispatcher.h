// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_DISPATCHER_H_

#include <memory>
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace blink {

class CanvasResource;

class CanvasResourceDispatcherClient {
 public:
  virtual void BeginFrame() = 0;
};

class PLATFORM_EXPORT CanvasResourceDispatcher
    : public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  base::WeakPtr<CanvasResourceDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  CanvasResourceDispatcherClient* Client() { return client_; }

  enum {
    kInvalidPlaceholderCanvasId = -1,
  };

  CanvasResourceDispatcher(CanvasResourceDispatcherClient*,
                           uint32_t client_id,
                           uint32_t sink_id,
                           int placeholder_canvas_id,
                           const IntSize&);

  ~CanvasResourceDispatcher() override;
  void SetNeedsBeginFrame(bool);
  void SetSuspendAnimation(bool);
  bool NeedsBeginFrame() const { return needs_begin_frame_; }
  bool IsAnimationSuspended() const { return suspend_animation_; }
  void DispatchFrame(scoped_refptr<CanvasResource>,
                     base::TimeTicks commit_start_time,
                     const SkIRect& damage_rect,
                     bool needs_vertical_flip,
                     bool is_opaque);
  void ReclaimResource(viz::ResourceId);
  void DispatchFrameSync(scoped_refptr<CanvasResource>,
                         base::TimeTicks commit_start_time,
                         const SkIRect& damage_rect,
                         bool needs_vertical_flip,
                         bool is_opaque);

  void Reshape(const IntSize&);

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) final;
  void DidPresentCompositorFrame(
      uint32_t presentation_token,
      ::gfx::mojom::blink::PresentationFeedbackPtr feedback) final;
  void OnBeginFrame(const viz::BeginFrameArgs&) final;
  void OnBeginFramePausedChanged(bool paused) final{};
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) final;

  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               ::gpu::mojom::blink::MailboxPtr id);
  void DidDeleteSharedBitmap(::gpu::mojom::blink::MailboxPtr id);

  // This enum is used in histogram, so it should be append-only.
  enum OffscreenCanvasCommitType {
    kCommitGPUCanvasGPUCompositing = 0,
    kCommitGPUCanvasSoftwareCompositing = 1,
    kCommitSoftwareCanvasGPUCompositing = 2,
    kCommitSoftwareCanvasSoftwareCompositing = 3,
    kOffscreenCanvasCommitTypeCount,
  };

 private:
  friend class CanvasResourceDispatcherTest;
  struct FrameResource;
  using ResourceMap = HashMap<unsigned, std::unique_ptr<FrameResource>>;

  bool PrepareFrame(scoped_refptr<CanvasResource>,
                    base::TimeTicks commit_start_time,
                    const SkIRect& damage_rect,
                    bool needs_vertical_flip,
                    bool is_opaque,
                    viz::CompositorFrame* frame);

  // Surface-related
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  const viz::FrameSinkId frame_sink_id_;

  IntSize size_;
  bool change_size_for_next_commit_;
  bool suspend_animation_ = false;
  bool needs_begin_frame_ = false;
  int pending_compositor_frames_ = 0;

  void SetNeedsBeginFrameInternal();

  bool VerifyImageSize(const IntSize);
  void PostImageToPlaceholderIfNotBlocked(scoped_refptr<CanvasResource>,
                                          viz::ResourceId resource_id);
  // virtual for testing
  virtual void PostImageToPlaceholder(scoped_refptr<CanvasResource>,
                                      viz::ResourceId resource_id);

  void ReclaimResourceInternal(viz::ResourceId resource_id);
  void ReclaimResourceInternal(const ResourceMap::iterator&);

  viz::mojom::blink::CompositorFrameSinkPtr sink_;
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient> binding_;
  viz::mojom::blink::CompositorFrameSinkClientPtr client_ptr_;

  int placeholder_canvas_id_;

  unsigned next_resource_id_ = 0;
  ResourceMap resources_;

  // The latest_unposted_resource_id_ always refers to the Id of the frame
  // resource used by the latest_unposted_image_.
  scoped_refptr<CanvasResource> latest_unposted_image_;
  viz::ResourceId latest_unposted_resource_id_;
  unsigned num_unreclaimed_frames_posted_;

  viz::BeginFrameAck current_begin_frame_ack_;

  CanvasResourceDispatcherClient* client_;

  base::WeakPtrFactory<CanvasResourceDispatcher> weak_ptr_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_H_
