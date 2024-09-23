// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/metrics/video_playback_roughness_reporter.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom-blink.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom-blink.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

namespace {

// If enabled, every VideoFrameSubmitter will share a FrameSinkBundle with every
// other VideoFrameSubmitter living on the same thread with the same parent
// FrameSinkId. This is used to aggregate Viz communication and substantially
// reduce IPC traffic when many VideoFrameSubmitters are active within a frame.
BASE_FEATURE(kUseVideoFrameSinkBundle,
             "UseVideoFrameSinkBundle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Builds a cc::FrameInfo representing a video frame, which is considered
// Compositor-only.
cc::FrameInfo CreateFrameInfo(cc::FrameInfo::FrameFinalState final_state) {
  cc::FrameInfo frame_info;
  frame_info.final_state = final_state;
  frame_info.smooth_thread = cc::FrameInfo::SmoothThread::kSmoothCompositor;
  frame_info.main_thread_response = cc::FrameInfo::MainThreadResponse::kMissing;
  return frame_info;
}

// Helper method for creating manual ack with damage and prefered frame
// interval.
viz::BeginFrameAck CreateManualAckWithDamageAndPreferredFrameInterval(
    cc::VideoFrameProvider* video_frame_provider) {
  auto begin_frame_ack = viz::BeginFrameAck::CreateManualAckWithDamage();
  begin_frame_ack.preferred_frame_interval =
      video_frame_provider ? video_frame_provider->GetPreferredRenderInterval()
                           : viz::BeginFrameArgs::MinInterval();
  return begin_frame_ack;
}

void RecordUmaPreSubmitBufferingDelay(bool is_media_stream,
                                      base::TimeDelta delay) {
  if (is_media_stream) {
    base::UmaHistogramTimes("Media.VideoFrameSubmitter.Rtc.PreSubmitBuffering",
                            delay);
  } else {
    base::UmaHistogramTimes(
        "Media.VideoFrameSubmitter.Video.PreSubmitBuffering", delay);
  }

  // TODO(crbug.com/364352012): This will be removed once expired, kept for now
  // due to internal dependencies.
  base::UmaHistogramTimes("Media.VideoFrameSubmitter.PreSubmitBuffering",
                          delay);
}

}  // namespace

// Helper CompositorFrameSink implementation which sits locally between a
// VideoFrameSubmitter and a thread-local FrameSinkBundle connection to Viz.
// This queues outgoing messages so they can be delivered in batches. With
// many active VideoFrameSubmitters in the same frame, this can significantly
// reduce Viz communication overhead.
class VideoFrameSubmitter::FrameSinkBundleProxy
    : public viz::mojom::blink::CompositorFrameSink {
 public:
  FrameSinkBundleProxy(base::WeakPtr<VideoFrameSinkBundle> bundle,
                       const viz::FrameSinkId& frame_sink_id)
      : bundle_(std::move(bundle)),
        bundle_id_(bundle_->bundle_id()),
        frame_sink_id_(frame_sink_id) {}
  FrameSinkBundleProxy(const FrameSinkBundleProxy&) = delete;
  FrameSinkBundleProxy& operator=(const FrameSinkBundleProxy&) = delete;

  ~FrameSinkBundleProxy() override {
    if (bundle_) {
      bundle_->RemoveClient(frame_sink_id_);
    }
  }

  // viz::mojom::Blink::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override {
    if (!bundle_) {
      return;
    }

    bundle_->SetNeedsBeginFrame(frame_sink_id_.sink_id(), needs_begin_frame);
  }

  void SetWantsBeginFrameAcks() override {
    if (!bundle_) {
      return;
    }

    bundle_->SetWantsBeginFrameAcks(frame_sink_id_.sink_id());
  }

  // Not used by VideoFrameSubmitter.
  void SetWantsAnimateOnlyBeginFrames() override { NOTREACHED_IN_MIGRATION(); }
  void SetAutoNeedsBeginFrame() override { NOTREACHED_IN_MIGRATION(); }

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override {
    if (!bundle_) {
      return;
    }

    bundle_->SubmitCompositorFrame(
        frame_sink_id_.sink_id(), local_surface_id, std::move(frame),
        std::move(hit_test_region_list), submit_time);
  }

  // Not used by VideoFrameSubmitter.
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override {
    if (!bundle_) {
      return;
    }
    bundle_->DidNotProduceFrame(frame_sink_id_.sink_id(), ack);
  }

  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override {
    if (!bundle_) {
      return;
    }
    bundle_->DidAllocateSharedBitmap(frame_sink_id_.sink_id(),
                                     std::move(region), id);
  }

  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override {
    if (!bundle_) {
      return;
    }
    bundle_->DidDeleteSharedBitmap(frame_sink_id_.sink_id(), id);
  }

  void InitializeCompositorFrameSinkType(
      viz::mojom::blink::CompositorFrameSinkType type) override {
    if (!bundle_) {
      return;
    }
    bundle_->InitializeCompositorFrameSinkType(frame_sink_id_.sink_id(), type);
  }

  void BindLayerContext(
      viz::mojom::blink::PendingLayerContextPtr context) override {}

#if BUILDFLAG(IS_ANDROID)
  void SetThreadIds(const WTF::Vector<int32_t>& thread_ids) override {
    bundle_->SetThreadIds(frame_sink_id_.sink_id(), thread_ids);
  }
#endif

 private:
  const base::WeakPtr<VideoFrameSinkBundle> bundle_;
  const viz::FrameSinkBundleId bundle_id_;
  const viz::FrameSinkId frame_sink_id_;
};

VideoFrameSubmitter::VideoFrameSubmitter(
    WebContextProviderCallback context_provider_callback,
    cc::VideoPlaybackRoughnessReporter::ReportingCallback
        roughness_reporting_callback,
    std::unique_ptr<VideoFrameResourceProvider> resource_provider)
    : context_provider_callback_(context_provider_callback),
      resource_provider_(std::move(resource_provider)),
      roughness_reporter_(std::make_unique<cc::VideoPlaybackRoughnessReporter>(
          std::move(roughness_reporting_callback))),
      frame_trackers_(false, nullptr),
      frame_sorter_(base::BindRepeating(
          &cc::FrameSequenceTrackerCollection::AddSortedFrame,
          base::Unretained(&frame_trackers_))) {
  DETACH_FROM_THREAD(thread_checker_);
}

VideoFrameSubmitter::~VideoFrameSubmitter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (context_provider_)
    context_provider_->RemoveObserver(this);

  if (shared_image_interface_) {
    shared_image_interface_->gpu_channel()->RemoveObserver(this);
  }

  // Release VideoFrameResourceProvider early since its destruction will make
  // calls back into this class via the viz::SharedBitmapReporter interface.
  resource_provider_.reset();
}

void VideoFrameSubmitter::StopUsingProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_rendering_)
    StopRendering();
  video_frame_provider_ = nullptr;
}

void VideoFrameSubmitter::StartRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_rendering_);
  is_rendering_ = true;

  if (compositor_frame_sink_) {
    compositor_frame_sink_->SetNeedsBeginFrame(IsDrivingFrameUpdates());
  }

  frame_trackers_.StartSequence(cc::FrameSequenceTrackerType::kVideo);
}

void VideoFrameSubmitter::StopRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_rendering_);
  DCHECK(video_frame_provider_);

  is_rendering_ = false;

  frame_trackers_.StopSequence(cc::FrameSequenceTrackerType::kVideo);
  frame_sorter_.Reset();

  UpdateSubmissionState();
}

void VideoFrameSubmitter::DidReceiveFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(video_frame_provider_);
  SubmitSingleFrame();
}

bool VideoFrameSubmitter::IsDrivingFrameUpdates() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We drive frame updates only when we believe that something is consuming
  // them.  This is different than VideoLayer, which drives updates any time
  // they're in the layer tree.
  return (is_rendering_ && ShouldSubmit()) || force_begin_frames_;
}

void VideoFrameSubmitter::Initialize(cc::VideoFrameProvider* provider,
                                     bool is_media_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!provider)
    return;

  DCHECK(!video_frame_provider_);
  video_frame_provider_ = provider;
  is_media_stream_ = is_media_stream;
  roughness_reporter_->set_is_media_stream(is_media_stream_);

  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  context_provider_callback_.Run(
      nullptr, base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                              weak_ptr_factory_.GetWeakPtr()));
}

void VideoFrameSubmitter::SetTransform(media::VideoTransformation transform) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transform_ = transform;
}

void VideoFrameSubmitter::EnableSubmission(viz::SurfaceId surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(lethalantidote): Set these fields earlier in the constructor. Will
  // need to construct VideoFrameSubmitter later in order to do this.
  frame_sink_id_ = surface_id.frame_sink_id();
  child_local_surface_id_allocator_.UpdateFromParent(
      surface_id.local_surface_id());
  if (resource_provider_->IsInitialized())
    StartSubmitting();
}

void VideoFrameSubmitter::SetIsSurfaceVisible(bool is_visible) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_surface_visible_ = is_visible;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SetIsPageVisible(bool is_visible) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_page_visible_ = is_visible;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SetForceBeginFrames(bool force_begin_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  force_begin_frames_ = force_begin_frames;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::SetForceSubmit(bool force_submit) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  force_submit_ = force_submit;
  UpdateSubmissionState();
}

void VideoFrameSubmitter::OnContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  receiver_.reset();

  if (context_provider_)
    context_provider_->RemoveObserver(this);

  if (shared_image_interface_) {
    shared_image_interface_->gpu_channel()->RemoveObserver(this);
    shared_image_interface_.reset();
  }

  waiting_for_compositor_ack_ = 0;
  last_frame_id_.reset();

  if (video_frame_provider_)
    video_frame_provider_->OnContextLost();

  resource_provider_->OnContextLost();

  // NOTE: These objects should be reset last; and if `bundle_proxy`_ is set, it
  // should be reset after `remote_frame_sink_`.
  compositor_frame_sink_ = nullptr;
  remote_frame_sink_.reset();
  bundle_proxy_.reset();

  context_provider_callback_.Run(
      context_provider_,
      base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VideoFrameSubmitter::OnGpuChannelLost() {
  // GpuChannel lost is notified on the IO thread. Forward it to the
  // VideoFrameCompositor thread.
  if (base::SingleThreadTaskRunner::GetCurrentDefault() != task_runner_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoFrameSubmitter::OnGpuChannelLost,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!shared_image_interface_) {
    return;
  }

  // The Observable removes all observers after completing GpuChannelLost
  // notification. No need to RemoveObserver(). Call RemoveObserver during
  // notification will cause deadlock.
  shared_image_interface_.reset();

  OnContextLost();
}

void VideoFrameSubmitter::DidReceiveCompositorFrameAck(
    WTF::Vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReclaimResources(std::move(resources));

  // `waiting_for_compositor_ack_` may be set to zero during SubmitEmptyFrame()
  // or upon ContextLost().
  if (waiting_for_compositor_ack_ == 0) {
    return;
  }

  --waiting_for_compositor_ack_;
}

void VideoFrameSubmitter::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const WTF::HashMap<uint32_t, viz::FrameTimingDetails>& timing_details,
    bool frame_ack,
    WTF::Vector<viz::ReturnedResource> resources) {
  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("media", "VideoFrameSubmitter::OnBeginFrame");

  last_begin_frame_args_ = args;

  WTF::Vector<uint32_t> frame_tokens;
  for (const auto& id : timing_details.Keys())
    frame_tokens.push_back(id);
  std::sort(frame_tokens.begin(), frame_tokens.end());

  for (const auto& frame_token : frame_tokens) {
    if (viz::FrameTokenGT(frame_token, *next_frame_token_))
      continue;

    auto& details = timing_details.find(frame_token)->value;
    auto& feedback = details.presentation_feedback;

#if BUILDFLAG(IS_LINUX)
    // TODO: On Linux failure flag is unreliable, and perfectly rendered frames
    // are reported as failures all the time.
    bool presentation_failure = false;
#else
    bool presentation_failure =
        feedback.flags & gfx::PresentationFeedback::kFailure;
#endif
    cc::FrameInfo::FrameFinalState final_state =
        cc::FrameInfo::FrameFinalState::kNoUpdateDesired;
    if (ignorable_submitted_frames_.contains(frame_token)) {
      ignorable_submitted_frames_.erase(frame_token);
    } else {
      if (presentation_failure) {
        final_state = cc::FrameInfo::FrameFinalState::kDropped;
      } else {
        final_state = cc::FrameInfo::FrameFinalState::kPresentedAll;

        // We assume that presentation feedback is reliable if
        // 1. (kHWCompletion) OS told us that the frame was shown at that time
        //  or
        // 2. (kVSync) at least presentation time is aligned with vsyncs
        // intervals
        uint32_t reliable_feedback_mask =
            gfx::PresentationFeedback::kHWCompletion |
            gfx::PresentationFeedback::kVSync;
        bool reliable_timestamp = feedback.flags & reliable_feedback_mask;
        roughness_reporter_->FramePresented(frame_token, feedback.timestamp,
                                            reliable_timestamp);
        // Compute the delta between the time the frame was received by the
        // compositor and when it was presented.
        auto delta_between_receive_and_present =
            details.presentation_feedback.timestamp -
            details.received_compositor_frame_timestamp;

        // We compute the average delta between frames being received at the
        // compositor to them being presented using an exponential moving
        // average with a smoothing factor
        // emea_smoothing_factor_for_average_delta_ which defaults to 0.2.
        // The exponential moving average formula for reference is as below:
        // EMEA_Delta = (Delta_New * smoothing_factor) +
        //     (EMEA_PreviousDelta * (1 - smoothing_factor)
        if (average_delta_between_receive_and_present_.is_zero()) {
          average_delta_between_receive_and_present_ =
              delta_between_receive_and_present;
        } else {
          // Smoothing factor for the exponential moving average for the delta.
          constexpr double emea_smoothing_factor_for_average_delta = 0.2;

          average_delta_between_receive_and_present_ =
              delta_between_receive_and_present *
                  emea_smoothing_factor_for_average_delta +
              average_delta_between_receive_and_present_ *
                  (1 - emea_smoothing_factor_for_average_delta);
        }
      }
      if (pending_frames_.contains(frame_token)) {
        frame_sorter_.AddFrameResult(pending_frames_[frame_token],
                                     CreateFrameInfo(final_state));
        pending_frames_.erase(frame_token);
      }
    }

    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "media", "VideoFrameSubmitter",
        TRACE_ID_WITH_SCOPE("VideoFrameSubmitter", frame_token),
        feedback.timestamp);
  }

  base::TimeTicks deadline_min = args.frame_time + args.interval;
  base::TimeTicks deadline_max = args.frame_time + 2 * args.interval;
  // The default value for the expected display time of the frame is the
  // same as the deadline_max.
  base::TimeTicks frame_expected_display_time = deadline_max;
  // The expected display time of a frame can be computed from the average delta
  // between the frame arriving at the compositor and being presented. We
  // use the average delta computed above and add it to the current time, which
  // gives us an approximate time for when we can expect the frame to actually
  // be presented.
  if (!average_delta_between_receive_and_present_.is_zero()) {
    frame_expected_display_time =
        base::TimeTicks::Now() + average_delta_between_receive_and_present_;
  }

  TRACE_EVENT_INSTANT1("media", "FrameExpectedDisplayTime",
                       TRACE_EVENT_SCOPE_THREAD, "frame_expected_display_time",
                       frame_expected_display_time);

  frame_trackers_.NotifyBeginImplFrame(args);
  frame_sorter_.AddNewFrame(args);

  absl::Cleanup end_frame = [this, &args] {
    frame_trackers_.NotifyFrameEnd(args, args);
  };
  absl::Cleanup roughness_processing = [this] {
    roughness_reporter_->ProcessFrameWindow();
  };

  // Don't call UpdateCurrentFrame() for MISSED BeginFrames. Also don't call it
  // after StopRendering() has been called (forbidden by API contract).
  viz::BeginFrameAck current_begin_frame_ack(args, false);
  current_begin_frame_ack.preferred_frame_interval =
      video_frame_provider_
          ? video_frame_provider_->GetPreferredRenderInterval()
          : viz::BeginFrameArgs::MinInterval();
  if (args.type == viz::BeginFrameArgs::MISSED || !is_rendering_) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    frame_sorter_.AddFrameResult(
        args,
        CreateFrameInfo(cc::FrameInfo::FrameFinalState::kNoUpdateDesired));
    return;
  }

  // Update the current frame, even if we haven't gotten an ack for a previous
  // frame yet. That probably signals a dropped frame, and this will let the
  // provider know that it happened, since we won't PutCurrentFrame this one.
  // Note that we should DidNotProduceFrame with or without the ack.
  if (!video_frame_provider_ ||
      !video_frame_provider_->UpdateCurrentFrame(deadline_min, deadline_max)) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    frame_sorter_.AddFrameResult(
        args,
        CreateFrameInfo(cc::FrameInfo::FrameFinalState::kNoUpdateDesired));
    return;
  }

  // We do have a new frame that we could display.  See if we're supposed to
  // actually submit a frame or not, and try to submit one.
  auto video_frame = video_frame_provider_->GetCurrentFrame();
  if (!SubmitFrame(current_begin_frame_ack, std::move(video_frame))) {
    compositor_frame_sink_->DidNotProduceFrame(current_begin_frame_ack);
    frame_sorter_.AddFrameResult(
        args,
        CreateFrameInfo(cc::FrameInfo::FrameFinalState::kNoUpdateDesired));
    return;
  }

  // We submitted a frame!

  // We still signal PutCurrentFrame here, rather than on the ack, so that it
  // lines up with the correct frame.  Otherwise, any intervening calls to
  // OnBeginFrame => UpdateCurrentFrame will cause the put to signal that the
  // later frame was displayed.
  video_frame_provider_->PutCurrentFrame();
}

void VideoFrameSubmitter::ReclaimResources(
    WTF::Vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_provider_->ReceiveReturnsFromParent(std::move(resources));
}

void VideoFrameSubmitter::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidAllocateSharedBitmap(std::move(region), id);
}

void VideoFrameSubmitter::DidDeleteSharedBitmap(const viz::SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_);
  compositor_frame_sink_->DidDeleteSharedBitmap(id);
}

void VideoFrameSubmitter::OnReceivedContextProvider(
    bool use_gpu_compositing,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!use_gpu_compositing) {
    shared_image_interface_ = std::move(shared_image_interface);
    if (shared_image_interface_) {
      shared_image_interface_->gpu_channel()->AddObserver(this);
    }
    resource_provider_->Initialize(nullptr, this, shared_image_interface_);
    if (frame_sink_id_.is_valid()) {
      StartSubmitting();
    }
    return;
  }

  if (!MaybeAcceptContextProvider(std::move(context_provider))) {
    constexpr base::TimeDelta kGetContextProviderRetryTimeout =
        base::Milliseconds(150);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            context_provider_callback_, context_provider_,
            base::BindOnce(&VideoFrameSubmitter::OnReceivedContextProvider,
                           weak_ptr_factory_.GetWeakPtr())),
        kGetContextProviderRetryTimeout);
    return;
  }

  context_provider_->AddObserver(this);
  resource_provider_->Initialize(context_provider_.get(), nullptr,
                                 /*shared_image_interface*/ nullptr);

  if (frame_sink_id_.is_valid())
    StartSubmitting();
}

bool VideoFrameSubmitter::MaybeAcceptContextProvider(
    scoped_refptr<viz::RasterContextProvider> context_provider) {
  if (!context_provider) {
    return false;
  }

  context_provider_ = std::move(context_provider);
  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    return false;
  }

  return context_provider_->RasterInterface()->GetGraphicsResetStatusKHR() ==
         GL_NO_ERROR;
}

void VideoFrameSubmitter::StartSubmitting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame_sink_id_.is_valid());

  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> provider;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      provider.BindNewPipeAndPassReceiver());
  if (base::FeatureList::IsEnabled(kUseVideoFrameSinkBundle)) {
    auto& bundle = VideoFrameSinkBundle::GetOrCreateSharedInstance(
        frame_sink_id_.client_id());
    auto weak_bundle = bundle.AddClient(frame_sink_id_, this, provider,
                                        receiver_, remote_frame_sink_);
    bundle_proxy_ = std::make_unique<FrameSinkBundleProxy>(
        std::move(weak_bundle), frame_sink_id_);
    compositor_frame_sink_ = bundle_proxy_.get();
  } else {
    provider->CreateCompositorFrameSink(
        frame_sink_id_, receiver_.BindNewPipeAndPassRemote(),
        remote_frame_sink_.BindNewPipeAndPassReceiver());
    compositor_frame_sink_ = remote_frame_sink_.get();
  }
  compositor_frame_sink_->SetWantsBeginFrameAcks();

  if (!surface_embedder_.is_bound()) {
    provider->ConnectToEmbedder(frame_sink_id_,
                                surface_embedder_.BindNewPipeAndPassReceiver());
  } else {
    GenerateNewSurfaceId();
  }

  remote_frame_sink_.set_disconnect_handler(base::BindOnce(
      &VideoFrameSubmitter::OnContextLost, base::Unretained(this)));

  compositor_frame_sink_->InitializeCompositorFrameSinkType(
      is_media_stream_ ? viz::mojom::CompositorFrameSinkType::kMediaStream
                       : viz::mojom::CompositorFrameSinkType::kVideo);

#if BUILDFLAG(IS_ANDROID)
  WTF::Vector<base::PlatformThreadId> thread_ids;
  thread_ids.push_back(base::PlatformThread::CurrentId());
  thread_ids.push_back(Platform::Current()->GetIOThreadId());
  compositor_frame_sink_->SetThreadIds(thread_ids);
#endif

  UpdateSubmissionState();
}

void VideoFrameSubmitter::UpdateSubmissionState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!compositor_frame_sink_)
    return;
  const auto is_driving_frame_updates = IsDrivingFrameUpdates();
  compositor_frame_sink_->SetNeedsBeginFrame(is_driving_frame_updates);
  // If we're not driving frame updates, then we're paused / off-screen / etc.
  // Roughness reporting should stop until we resume.  Since the current frame
  // might be on-screen for a long time, we also discard the current window.
  if (!is_driving_frame_updates) {
    roughness_reporter_->Reset();
  }

  // These two calls are very important; they are responsible for significant
  // memory savings when content is off-screen.
  //
  // While off-screen, we do not submit frames (unless |force_submit_| is true),
  // which prevents GPU resource creation and accumulation on the remote side.
  // During the transition to off-screen we further send an empty frame with the
  // intent to evict any resources held for the previous frame. Combined these
  // optimizations save 30-50% in cc:: resource memory usage.
  //
  // See https://crbug.com/829813 and https://crbug.com/829565.
  if (ShouldSubmit()) {
    // Submit even if we're rendering, otherwise we may display an empty frame
    // before the next OnBeginFrame() which can cause a visible flash.
    SubmitSingleFrame();
  } else {
    // Post a delayed task to submit an empty frame. We don't do this here,
    // since there is a race between when we're notified that the player is not
    // visible, and when auto-PiP starts. In PiP, we'll be set to force submit,
    // but we're notified after we find out that the page is hidden.  If we
    // submit an empty frame now, then there will be a flicker in the video
    // when the empty frame is displayed. By delaying the empty frame, we give
    // the auto-PiP a chance to start. Note that the empty frame isn't required
    // for visual correctness; it's just for resource cleanup. We can delay
    // resource cleanup a little.
    //
    // If there are any in-flight empty frame requests, this cancels them. We
    // want to wait until any group of state changes stabilizes.
    empty_frame_timer_.Start(
        FROM_HERE, base::Milliseconds(500),
        base::BindOnce(&VideoFrameSubmitter::SubmitEmptyFrameIfNeeded,
                       base::Unretained(this)));
  }
}

void VideoFrameSubmitter::SubmitEmptyFrameIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!compositor_frame_sink_)
    return;

  // If we are allowed to submit real frames, then don't send a blank frame
  // since the last real frame might actually be visible.
  //
  // We do not actually submit a real frame here, though; that should be done
  // (if desired) by whatever switched us to ShouldSubmit() mode.
  if (ShouldSubmit())
    return;

  // If we don't have a frame size, then we can't send a blank frame.
  if (frame_size_.IsEmpty())
    return;

  SubmitEmptyFrame();
}

bool VideoFrameSubmitter::SubmitFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(video_frame);
  TRACE_EVENT1("media", "VideoFrameSubmitter::SubmitFrame", "frame",
               video_frame->AsHumanReadableString());

  if (!compositor_frame_sink_ || !ShouldSubmit()) {
    return false;
  }

  // Not submitting a frame when waiting for a previous ack saves memory by
  // not building up unused remote side resources. See https://crbug.com/830828.
  //
  // Similarly we don't submit the same frame multiple times.
  if (last_frame_id_ == video_frame->unique_id()) {
    return false;
  }

  gfx::Size frame_size(video_frame->natural_size());

  // Prefer the frame level transform if set.
  auto transform = video_frame->metadata().transformation.value_or(transform_);
  if (transform.rotation == media::VIDEO_ROTATION_90 ||
      transform.rotation == media::VIDEO_ROTATION_270) {
    frame_size = gfx::Size(frame_size.height(), frame_size.width());
  }

  if (frame_size.IsEmpty()) {
    // We're not supposed to get 0x0 frames.  For now, just ignore it until we
    // track down where they're coming from.  Creating a CompositorFrame with an
    // empty output rectangle isn't allowed.
    // crbug.com/979564
    return false;
  }

  bool frame_size_changed = false;
  if (frame_size_ != frame_size) {
    if (!frame_size_.IsEmpty())
      GenerateNewSurfaceId();
    frame_size_ = frame_size;
    frame_size_changed = true;
  }

  // We can't delay frame size changes even if we have a pending compositor ACK
  // because a relayout signal is already in flight on the main thread.
  if (waiting_for_compositor_ack_ > 0 && !frame_size_changed) {
    return false;
  }

  last_frame_id_ = video_frame->unique_id();

  Opacity new_opacity = media::IsOpaque(video_frame->format())
                            ? Opacity::kIsOpaque
                            : Opacity::kIsNotOpaque;

  auto frame_token = ++next_frame_token_;
  auto source_id = begin_frame_ack.frame_id.source_id;
  if (source_id != viz::BeginFrameArgs::kManualSourceId) {
    // Roughness reporter only cares about true video frames.
    roughness_reporter_->FrameSubmitted(frame_token, *video_frame.get(),
                                        last_begin_frame_args_.interval);
  }
  auto compositor_frame = CreateCompositorFrame(
      frame_token, begin_frame_ack, std::move(video_frame), transform);

  WebVector<viz::ResourceId> resources;
  const auto& quad_list = compositor_frame.render_pass_list.back()->quad_list;
  if (!quad_list.empty()) {
    DCHECK_EQ(quad_list.size(), 1u);
    resources.Assign(quad_list.front()->resources);
  }

  WebVector<viz::TransferableResource> resource_list;
  resource_provider_->PrepareSendToParent(resources, &resource_list);
  compositor_frame.resource_list = resource_list.ReleaseVector();

  // We can pass nullptr for the HitTestData as the CompositorFram will not
  // contain any SurfaceDrawQuads.
  compositor_frame_sink_->SubmitCompositorFrame(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(compositor_frame), std::nullopt, 0);
  resource_provider_->ReleaseFrameResources();

  NotifyOpacityIfNeeded(new_opacity);

  ++waiting_for_compositor_ack_;
  return true;
}

void VideoFrameSubmitter::SubmitEmptyFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_ && !ShouldSubmit());
  DCHECK(!frame_size_.IsEmpty());
  TRACE_EVENT0("media", "VideoFrameSubmitter::SubmitEmptyFrame");

  // If there's nothing to submit to or we've already submitted an empty frame,
  // don't submit another one.
  if (!compositor_frame_sink_ || !last_frame_id_.has_value())
    return;

  last_frame_id_.reset();
  auto begin_frame_ack =
      CreateManualAckWithDamageAndPreferredFrameInterval(video_frame_provider_);
  auto frame_token = ++next_frame_token_;
  auto compositor_frame = CreateCompositorFrame(
      frame_token, begin_frame_ack, nullptr, media::kNoTransformation);

  compositor_frame_sink_->SubmitCompositorFrame(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(compositor_frame), std::nullopt, 0);
  NotifyOpacityIfNeeded(Opacity::kIsNotOpaque);

  // We set `waiting_for_compositor_ack_` to zero here since we want to allow a
  // subsequent real frame to replace it at any time if needed.
  waiting_for_compositor_ack_ = 0;
}

void VideoFrameSubmitter::SubmitSingleFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If we haven't gotten a valid result yet from |context_provider_callback_|
  // |resource_provider_| will remain uninitialized.
  // |video_frame_provider_| may be null if StopUsingProvider has been called,
  // which could happen if the |video_frame_provider_| is destructing while we
  // are waiting for the ContextProvider.
  if (!resource_provider_->IsInitialized() || !video_frame_provider_)
    return;

  auto video_frame = video_frame_provider_->GetCurrentFrame();
  if (!video_frame)
    return;

  if (SubmitFrame(CreateManualAckWithDamageAndPreferredFrameInterval(
                      video_frame_provider_),
                  std::move(video_frame))) {
    video_frame_provider_->PutCurrentFrame();
  }
}

bool VideoFrameSubmitter::ShouldSubmit() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return (is_surface_visible_ && is_page_visible_) || force_submit_;
}

viz::CompositorFrame VideoFrameSubmitter::CreateCompositorFrame(
    uint32_t frame_token,
    const viz::BeginFrameAck& begin_frame_ack,
    scoped_refptr<media::VideoFrame> video_frame,
    media::VideoTransformation transform) {
  DCHECK(!frame_size_.IsEmpty());

  viz::CompositorFrame compositor_frame;
  compositor_frame.metadata.begin_frame_ack = begin_frame_ack;
  compositor_frame.metadata.frame_token = frame_token;
  if (video_frame_provider_) {
    compositor_frame.metadata.frame_interval_inputs.frame_time =
        last_begin_frame_args_.frame_time;
    compositor_frame.metadata.frame_interval_inputs.content_interval_info
        .push_back({viz::ContentFrameIntervalType::kVideo,
                    video_frame_provider_->GetPreferredRenderInterval()});
    compositor_frame.metadata.frame_interval_inputs
        .has_only_content_frame_interval_updates = true;
  }

  if (video_frame && video_frame->metadata().decode_end_time.has_value()) {
    base::TimeTicks value = *video_frame->metadata().decode_end_time;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "media", "VideoFrameSubmitter",
        TRACE_ID_WITH_SCOPE("VideoFrameSubmitter", frame_token), value);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "media", "Pre-submit buffering",
        TRACE_ID_WITH_SCOPE("VideoFrameSubmitter", frame_token), value);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "media", "Pre-submit buffering",
        TRACE_ID_WITH_SCOPE("VideoFrameSubmitter", frame_token));

    if (begin_frame_ack.frame_id.source_id ==
        viz::BeginFrameArgs::kManualSourceId) {
      ignorable_submitted_frames_.insert(frame_token);
    } else {
      pending_frames_[frame_token] = last_begin_frame_args_;
    }

    RecordUmaPreSubmitBufferingDelay(is_media_stream_,
                                     base::TimeTicks::Now() - value);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "media", "VideoFrameSubmitter",
        TRACE_ID_WITH_SCOPE("VideoFrameSubmitter", frame_token),
        "empty video frame?", !video_frame);
  }

  // We don't assume that the ack is marked as having damage.  However, we're
  // definitely emitting a CompositorFrame that damages the entire surface.
  compositor_frame.metadata.begin_frame_ack.has_damage = true;
  compositor_frame.metadata.device_scale_factor = 1;
  compositor_frame.metadata.may_contain_video = true;
  // If we're submitting frames even if we're not visible, then also turn off
  // throttling.  This is for picture in picture, which can be throttled if the
  // opener window is minimized without this.
  compositor_frame.metadata.may_throttle_if_undrawn_frames = force_submit_;

  // Specify size of shared quad state and quad lists so that RenderPass doesn't
  // allocate using the defaults of 32 and 128 since we only append one quad.
  auto render_pass =
      viz::CompositorRenderPass::Create(/*shared_quad_state_list_size=*/1u,
                                        /*quad_list_size*/ 1u);
  render_pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(frame_size_),
                      gfx::Rect(frame_size_), gfx::Transform());

  if (video_frame) {
    compositor_frame.metadata.content_color_usage =
        video_frame->ColorSpace().GetContentColorUsage();
    const bool is_opaque = media::IsOpaque(video_frame->format());
    resource_provider_->AppendQuads(render_pass.get(), std::move(video_frame),
                                    transform, is_opaque);
  }

  compositor_frame.render_pass_list.emplace_back(std::move(render_pass));
  return compositor_frame;
}

void VideoFrameSubmitter::GenerateNewSurfaceId() {
  last_frame_id_.reset();

  // We need a new id in the event of context loss.
  child_local_surface_id_allocator_.GenerateId();

  surface_embedder_->SetLocalSurfaceId(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
}

void VideoFrameSubmitter::NotifyOpacityIfNeeded(Opacity new_opacity) {
  if (opacity_ == new_opacity || !surface_embedder_.is_bound()) {
    return;
  }

  opacity_ = new_opacity;
  surface_embedder_->OnOpacityChanged(new_opacity == Opacity::kIsOpaque);
}

}  // namespace blink
