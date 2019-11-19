// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SUBMITTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SUBMITTER_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/viz/client/shared_bitmap_reporter.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "services/viz/public/mojom/compositing/frame_timing_details.mojom-blink.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_resource_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// This single-threaded class facilitates the communication between the media
// stack and browser renderer, providing compositor frames containing video
// frames and corresponding resources to the |compositor_frame_sink_|.
//
// This class requires and uses a viz::ContextProvider, and thus, besides
// construction, must be consistently accessed from the same thread.
class PLATFORM_EXPORT VideoFrameSubmitter
    : public WebVideoFrameSubmitter,
      public viz::ContextLostObserver,
      public viz::SharedBitmapReporter,
      public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  VideoFrameSubmitter(WebContextProviderCallback,
                      std::unique_ptr<VideoFrameResourceProvider>);
  ~VideoFrameSubmitter() override;

  // cc::VideoFrameProvider::Client implementation.
  void StopUsingProvider() override;
  void StartRendering() override;
  void StopRendering() override;
  void DidReceiveFrame() override;
  bool IsDrivingFrameUpdates() const override;

  // WebVideoFrameSubmitter implementation.
  void Initialize(cc::VideoFrameProvider*) override;
  void SetRotation(media::VideoRotation) override;
  void EnableSubmission(
      viz::SurfaceId,
      base::TimeTicks local_surface_id_allocation_time) override;
  void SetIsSurfaceVisible(bool is_visible) override;
  void SetIsPageVisible(bool is_visible) override;
  void SetForceSubmit(bool) override;

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

  // cc::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(
      const viz::BeginFrameArgs&,
      WTF::HashMap<uint32_t, ::viz::mojom::blink::FrameTimingDetailsPtr>)
      override;
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) override;

  // viz::SharedBitmapReporter implementation.
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion,
                               const viz::SharedBitmapId&) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId&) override;

 private:
  friend class VideoFrameSubmitterTest;

  // Called during Initialize() and OnContextLost() after a new ContextGL is
  // requested.
  void OnReceivedContextProvider(
      bool use_gpu_compositing,
      scoped_refptr<viz::RasterContextProvider> context_provider);

  // Starts submission and calls UpdateSubmissionState(); which may submit.
  void StartSubmitting();

  // Sets CompositorFrameSink::SetNeedsBeginFrame() state and submits a frame if
  // visible or an empty frame if not.
  void UpdateSubmissionState();

  // Will submit an empty frame to clear resource usage if it's safe.
  void SubmitEmptyFrameIfNeeded();

  // Returns whether a frame was submitted.
  bool SubmitFrame(const viz::BeginFrameAck&, scoped_refptr<media::VideoFrame>);

  // SubmitEmptyFrame() is used to force the remote CompositorFrameSink to
  // release resources for the last submission; saving a significant amount of
  // memory (~30%) when content goes off-screen. See https://crbug.com/829813.
  void SubmitEmptyFrame();

  // Pulls frame and submits it to compositor. Used in cases like
  // DidReceiveFrame(), which occurs before video rendering has started to post
  // the first frame or to submit a final frame before ending rendering.
  void SubmitSingleFrame();

  // Return whether the submitter should submit frames based on its current
  // state. It's important to only submit when this is true to save memory. See
  // comments above and in UpdateSubmissionState().
  bool ShouldSubmit() const;

  // Generates a new surface ID using using |child_local_surface_id_allocator_|.
  // Called during context loss or during a frame size change.
  void GenerateNewSurfaceId();

  // Helper method for creating viz::CompositorFrame. If |video_frame| is null
  // then the frame will be empty.
  viz::CompositorFrame CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      scoped_refptr<media::VideoFrame> video_frame);

  cc::VideoFrameProvider* video_frame_provider_ = nullptr;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> compositor_frame_sink_;
  mojo::Remote<mojom::blink::SurfaceEmbedder> surface_embedder_;
  mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient> receiver_{this};
  WebContextProviderCallback context_provider_callback_;
  std::unique_ptr<VideoFrameResourceProvider> resource_provider_;
  bool waiting_for_compositor_ack_ = false;

  // Current rendering state. Set by StartRendering() and StopRendering().
  bool is_rendering_ = false;

  // If the surface is not visible within in the current view port, we should
  // not submit. Not submitting when off-screen saves significant memory.
  bool is_surface_visible_ = false;

  // Likewise, if the entire page is not visible, we should not submit. Not
  // submitting in the background causes the VideoFrameProvider to enter a
  // background rendering mode using lower frequency artificial BeginFrames.
  bool is_page_visible_ = true;

  // Whether frames should always be submitted, even if we're not visible. Used
  // by Picture-in-Picture mode to ensure submission occurs even off-screen.
  bool force_submit_ = false;

  // Needs to be initialized in implementation because media isn't a public_dep
  // of blink/platform.
  media::VideoRotation rotation_;

  viz::FrameSinkId frame_sink_id_;

  // Size of the video frame being submitted. It is set the first time a frame
  // is submitted. Every time there is a change in the video frame size, the
  // child component of the LocalSurfaceId will be updated.
  gfx::Size frame_size_;

  // Used to updated the LocalSurfaceId when detecting a change in video frame
  // size.
  viz::ChildLocalSurfaceIdAllocator child_local_surface_id_allocator_;

  viz::FrameTokenGenerator next_frame_token_;

  // Timestamps indexed by frame token for histogram purposes.
  using FrameTokenType = decltype(*std::declval<viz::FrameTokenGenerator>());
  base::flat_map<FrameTokenType, base::TimeTicks> frame_token_to_timestamp_map_;

  base::OneShotTimer empty_frame_timer_;

  base::Optional<int> last_frame_id_;

  cc::FrameSequenceTrackerCollection frame_trackers_;

  // The BeginFrameArgs passed to the most recent call of OnBeginFrame().
  // Required for FrameSequenceTrackerCollection::NotifySubmitFrame
  viz::BeginFrameArgs last_begin_frame_args_;

  // The token of the frames that are submitted outside OnBeginFrame(). These
  // frames should be ignored by the video tracker even if they are reported as
  // presented.
  base::flat_set<uint32_t> ignorable_submitted_frames_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VideoFrameSubmitter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoFrameSubmitter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SUBMITTER_H_
