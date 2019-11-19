// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace ukm {
class UkmRecorder;
}

namespace blink {

class PageScheduler;
class WebSchedulingTaskQueue;

class FrameScheduler : public FrameOrWorkerScheduler {
 public:
  class PLATFORM_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    virtual ukm::UkmRecorder* GetUkmRecorder() = 0;
    virtual ukm::SourceId GetUkmSourceId() = 0;

    // Called when a frame has exceeded a total task time threshold (100ms).
    virtual void UpdateTaskTime(base::TimeDelta time) = 0;

    // Notify that the list of active features for this frame has changed.
    // See SchedulingPolicy::Feature for the list of features and the meaning
    // of individual features.
    // Note that this method is not called when the frame navigates — it is
    // the responsibility of the observer to detect this and act reset features
    // accordingly.
    virtual void UpdateActiveSchedulerTrackedFeatures(
        uint64_t features_mask) = 0;

    virtual const base::UnguessableToken& GetAgentClusterId() const = 0;
  };

  ~FrameScheduler() override = default;

  // Represents the type of frame: main (top-level) vs not.
  enum class FrameType {
    kMainFrame,
    kSubframe,
  };

  enum class NavigationType {
    kReload,
    kSameDocument,
    kOther,
  };

  // The scheduler may throttle tasks associated with offscreen frames.
  virtual void SetFrameVisible(bool) = 0;
  virtual bool IsFrameVisible() const = 0;

  // Query the page visibility state for the page associated with this frame.
  // The scheduler may throttle tasks associated with pages that are not
  // visible.
  // TODO(altimin): Remove this method.
  virtual bool IsPageVisible() const = 0;

  // Set whether this frame is suspended. Only unthrottledTaskRunner tasks are
  // allowed to run on a suspended frame.
  virtual void SetPaused(bool) = 0;

  // Sets whether or not this frame should report (via tracing) tasks that are
  // posted to it.
  virtual void SetShouldReportPostedTasksWhenDisabled(bool) = 0;

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void SetCrossOrigin(bool) = 0;
  virtual bool IsCrossOrigin() const = 0;

  virtual void SetIsAdFrame() = 0;
  virtual bool IsAdFrame() const = 0;

  virtual void TraceUrlChange(const String&) = 0;

  // Keep track of the amount of time spent running tasks for the frame.
  // Forwards this tally to PageLoadMetrics and resets it each time it reaches
  // 100ms.  The FrameScheduler will get this information primarily from the
  // MainThreadTaskScheduler, but for tasks that are unattributable to a single
  // frame (e.g. requestAnimationFrame), this method must be called explicitly.
  virtual void AddTaskTime(base::TimeDelta time) = 0;

  // Returns the frame type, which currently determines whether this frame is
  // the top level frame, i.e. a main frame.
  virtual FrameType GetFrameType() const = 0;

  // Returns a task runner that is suitable with the given task type.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  // Returns a WebResourceLoadingTaskRunnerHandle which is intended to be used
  // by the loading stack to post resource loading tasks to the renderer's main
  // thread and to notify the main thread of any change in the resource's fetch
  // (net) priority.
  virtual std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() = 0;

  virtual std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingPriority) = 0;

  // Returns the parent PageScheduler.
  virtual PageScheduler* GetPageScheduler() const = 0;

  // Returns a WebScopedVirtualTimePauser which can be used to vote for pausing
  // virtual time. Virtual time will be paused if any WebScopedVirtualTimePauser
  // votes to pause it, and only unpaused only if all
  // WebScopedVirtualTimePausers are either destroyed or vote to unpause.  Note
  // the WebScopedVirtualTimePauser returned by this method is initially
  // unpaused.
  virtual WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) = 0;

  // Tells the scheduler that a provisional load has started, the scheduler may
  // reset the task cost estimators and the UserModel. Must be called from the
  // main thread.
  virtual void DidStartProvisionalLoad(bool is_main_frame) = 0;

  // Tells the scheduler that a provisional load has committed, the scheduler
  // may reset the task cost estimators and the UserModel. Must be called from
  // the main thread.
  virtual void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                        NavigationType navigation_type) = 0;

  // Tells the scheduler that the first contentful paint has occurred for this
  // frame.
  virtual void OnFirstContentfulPaint() = 0;

  // Tells the scheduler that the first meaningful paint has occurred for this
  // frame.
  virtual void OnFirstMeaningfulPaint() = 0;

  // Returns true if this frame is should not throttled (e.g. due to an active
  // connection).
  // Note that this only applies to the current frame,
  // use GetPageScheduler()->IsExemptFromBudgetBasedThrottling for the status
  // of the page.
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;

  // Returns UKM source id for recording metrics associated with this frame.
  virtual ukm::SourceId GetUkmSourceId() = 0;

  FrameScheduler* ToFrameScheduler() override { return this; }

  // Returns a handle that prevents resource loading as long as the handle
  // exists.
  virtual std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() = 0;

  // Returns the list of active features which currently tracked by the
  // scheduler for back-forward cache metrics.
  virtual WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics() = 0;

  // TODO(altimin): Move FrameScheduler object to oilpan.
  virtual base::WeakPtr<FrameScheduler> GetWeakPtr() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_
