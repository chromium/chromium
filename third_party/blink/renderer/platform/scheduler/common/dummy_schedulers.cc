// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/common/simple_main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class VirtualTimeController;

namespace scheduler {
namespace {

class DummyWidgetScheduler final : public WidgetScheduler {
 public:
  DummyWidgetScheduler() = default;
  DummyWidgetScheduler(const DummyWidgetScheduler&) = delete;
  DummyWidgetScheduler& operator=(const DummyWidgetScheduler&) = delete;
  ~DummyWidgetScheduler() override = default;

  void Shutdown() override {}
  // Returns the input task runner.
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  void WillBeginFrame(const viz::BeginFrameArgs& args) override {}
  void BeginFrameNotExpectedSoon() override {}
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void DidCommitFrameToCompositor() override {}
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override {}
  void WillPostInputEventToMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override {}
  void WillHandleInputEventOnMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution) override {}
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result,
                                       bool frame_requested) override {}
  void DidRunBeginMainFrame() override {}
  void SetHidden(bool hidden) override {}
};

class DummyFrameScheduler : public FrameScheduler {
 public:
  explicit DummyFrameScheduler(v8::Isolate* isolate)
      : page_scheduler_(CreateDummyPageScheduler(isolate)) {}
  ~DummyFrameScheduler() override = default;

  DummyFrameScheduler(const DummyFrameScheduler&) = delete;
  DummyFrameScheduler& operator=(const DummyFrameScheduler&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  PageScheduler* GetPageScheduler() const override {
    return page_scheduler_.get();
  }
  AgentGroupScheduler* GetAgentGroupScheduler() override {
    return &page_scheduler_->GetAgentGroupScheduler();
  }

  void SetPreemptedForCooperativeScheduling(Preempted) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return true; }
  void SetVisibleAreaLarge(bool) override {}
  void SetHadUserActivation(bool) override {}
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetShouldReportPostedTasksWhenDisabled(bool) override {}
  void SetCrossOriginToNearestMainFrame(bool) override {}
  void SetAgentClusterId(const base::UnguessableToken&) override {}
  bool IsCrossOriginToNearestMainFrame() const override { return false; }
  void SetIsAdFrame(bool is_ad_frame) override {}
  bool IsAdFrame() const override { return false; }
  bool IsInEmbeddedFrameTree() const override { return false; }
  void TraceUrlChange(const String&) override {}
  void AddTaskTime(base::TimeDelta) override {}
  FrameType GetFrameType() const override { return FrameType::kMainFrame; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad() override {}
  void DidCommitProvisionalLoad(bool,
                                FrameScheduler::NavigationType,
                                DidCommitProvisionalLoadParams) override {}
  void OnFirstContentfulPaintInMainFrame() override {}
  void OnFirstMeaningfulPaint(base::TimeTicks timestamp) override {}
  void OnDispatchLoadEvent() override {}
  void OnMainFrameInteractive() override {}
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override {
    return nullptr;
  }
  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingQueueType,
      WebSchedulingPriority) override {
    return nullptr;
  }
  ukm::SourceId GetUkmSourceId() override { return ukm::kInvalidSourceId; }
  void OnStartedUsingNonStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location,
      SchedulingAffectingFeatureHandle* handle) override {}
  void OnStartedUsingStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location) override {}
  void OnStoppedUsingNonStickyFeature(
      SchedulingAffectingFeatureHandle* handle) override {}
  base::WeakPtr<FrameOrWorkerScheduler> GetFrameOrWorkerSchedulerWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics() override {
    return WTF::HashSet<SchedulingPolicy::Feature>();
  }
  base::WeakPtr<FrameScheduler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void ReportActiveSchedulerTrackedFeatures() override {}
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  base::TimeDelta UnreportedTaskTime() const override {
    return base::TimeDelta();
  }

 private:
  std::unique_ptr<PageScheduler> page_scheduler_;
  base::WeakPtrFactory<FrameScheduler> weak_ptr_factory_{this};
};

class DummyPageScheduler : public PageScheduler {
 public:
  explicit DummyPageScheduler(v8::Isolate* isolate)
      : agent_group_scheduler_(CreateDummyAgentGroupScheduler(isolate)) {}
  ~DummyPageScheduler() override = default;

  DummyPageScheduler(const DummyPageScheduler&) = delete;
  DummyPageScheduler& operator=(const DummyPageScheduler&) = delete;

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      bool is_in_embedded_frame_tree,
      FrameScheduler::FrameType) override {
    return CreateDummyFrameScheduler(agent_group_scheduler_->Isolate());
  }

  void OnTitleOrFaviconUpdated() override {}
  void SetPageVisible(bool) override {}
  bool IsPageVisible() const override { return true; }
  void SetPageFrozen(bool) override {}
  void SetPageBackForwardCached(bool) override {}
  bool IsMainFrameLocal() const override { return true; }
  void SetIsMainFrameLocal(bool) override {}
  void AudioStateChanged(bool is_audio_playing) override {}
  bool IsAudioPlaying() const override { return false; }
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  bool OptedOutFromAggressiveThrottlingForTest() const override {
    return false;
  }
  bool IsInBackForwardCache() const override { return false; }
  bool RequestBeginMainFrameNotExpected(bool) override { return false; }
  AgentGroupScheduler& GetAgentGroupScheduler() override {
    return *agent_group_scheduler_;
  }
  VirtualTimeController* GetVirtualTimeController() override { return nullptr; }
  scoped_refptr<WidgetScheduler> CreateWidgetScheduler() override {
    return base::MakeRefCounted<DummyWidgetScheduler>();
  }

 private:
  Persistent<AgentGroupScheduler> agent_group_scheduler_;
};

class SimpleMainThread : public MainThread {
 public:
  // We rely on base::SingleThreadTaskRunner::CurrentDefaultHandle for tasks
  // posted on the main thread. The task runner handle may not be available on
  // Blink's startup (== on SimpleMainThread's construction), because some tests
  // like blink_platform_unittests do not set up a global task environment. In
  // those cases, a task environment is set up on a test fixture's creation, and
  // GetTaskRunner() returns the right task runner during a test.
  //
  // If GetTaskRunner() can be called from a non-main thread (including a worker
  // thread running Mojo callbacks), we need to somehow get a task runner for
  // the main thread. This is not possible with
  // SingleThreadTaskRunner::CurrentDefaultHandle. We currently deal with this
  // issue by setting the main thread task runner on the test startup and
  // clearing it on the test tear-down. This is what
  // SetMainThreadTaskRunnerForTesting() for. This function is called from
  // Platform::SetMainThreadTaskRunnerForTesting() and
  // Platform::UnsetMainThreadTaskRunnerForTesting().

  explicit SimpleMainThread(ThreadScheduler* scheduler)
      : scheduler_ptr_(scheduler) {}
  ~SimpleMainThread() override = default;

  SimpleMainThread(const SimpleMainThread&) = delete;
  SimpleMainThread& operator=(const SimpleMainThread&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      MainThreadTaskRunnerRestricted) const override {
    if (main_thread_task_runner_for_testing_) {
      return main_thread_task_runner_for_testing_;
    }
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  ThreadScheduler* Scheduler() override { return scheduler_ptr_; }

  bool IsCurrentThread() const { return WTF::IsMainThread(); }

  void SetMainThreadTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    main_thread_task_runner_for_testing_ = std::move(task_runner);
  }

 private:
  bool IsSimpleMainThread() const override { return true; }

  raw_ptr<ThreadScheduler> scheduler_ptr_;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_task_runner_for_testing_;
};

class SimpleMainThreadWithScheduler : public SimpleMainThread {
 public:
  SimpleMainThreadWithScheduler() : SimpleMainThread(nullptr) {}

  ThreadScheduler* Scheduler() override { return &scheduler_; }

 private:
  scheduler::SimpleMainThreadScheduler scheduler_;
};

class DummyWebMainThreadScheduler : public WebThreadScheduler,
                                    public MainThreadScheduler {
 public:
  DummyWebMainThreadScheduler() = default;
  ~DummyWebMainThreadScheduler() override = default;

  // WebThreadScheduler implementation:
  void Shutdown() override {}
  WebThreadScheduler* ToWebMainThreadScheduler() override { return this; }

  // ThreadScheduler implementation:
  bool ShouldYieldForHighPriorityWork() override { return false; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta delay,
                           Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  void AddRAILModeObserver(RAILModeObserver*) override {}
  void RemoveRAILModeObserver(RAILModeObserver const*) override {}
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks::Now();
  }
  void AddTaskObserver(base::TaskObserver*) override {}
  void RemoveTaskObserver(base::TaskObserver*) override {}
  blink::MainThreadScheduler* ToMainThreadScheduler() override { return this; }
  void SetV8Isolate(v8::Isolate* isolate) override { isolate_ = isolate; }

  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  std::unique_ptr<MainThread> CreateMainThread() override {
    return std::make_unique<SimpleMainThread>(this);
  }

  AgentGroupScheduler* CreateAgentGroupScheduler() override {
    return CreateDummyAgentGroupScheduler(isolate_);
  }

  std::unique_ptr<WebAgentGroupScheduler> CreateWebAgentGroupScheduler()
      override {
    return std::make_unique<WebAgentGroupScheduler>(
        CreateAgentGroupScheduler());
  }

  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  AgentGroupScheduler* GetCurrentAgentGroupScheduler() override {
    return nullptr;
  }

  std::unique_ptr<RendererPauseHandle> PauseScheduler() override {
    return nullptr;
  }

  void ExecuteAfterCurrentTaskForTesting(
      base::OnceClosure on_completion_task,
      ExecuteAfterCurrentTaskRestricted) override {}

  v8::Isolate* Isolate() override {
    return isolate_;
  }

  void StartIdlePeriodForTesting() override {}

  void ForEachMainThreadIsolate(
      base::RepeatingCallback<void(v8::Isolate* isolate)> callback) override {
    if (isolate_) {
      callback.Run(isolate_.get());
    }
  }

  void SetRendererBackgroundedForTesting(bool) override {}

 private:
  raw_ptr<v8::Isolate> isolate_ = nullptr;
};

class DummyAgentGroupScheduler : public AgentGroupScheduler {
 public:
  explicit DummyAgentGroupScheduler(v8::Isolate* isolate)
      : main_thread_scheduler_(new DummyWebMainThreadScheduler()) {
    main_thread_scheduler_->SetV8Isolate(isolate);
  }
  ~DummyAgentGroupScheduler() override = default;

  DummyAgentGroupScheduler(const DummyAgentGroupScheduler&) = delete;
  DummyAgentGroupScheduler& operator=(const DummyAgentGroupScheduler&) = delete;

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return CreateDummyPageScheduler(Isolate());
  }
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    DCHECK(WTF::IsMainThread());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  WebThreadScheduler& GetMainThreadScheduler() override {
    return *main_thread_scheduler_;
  }
  v8::Isolate* Isolate() override { return main_thread_scheduler_->Isolate(); }
  void AddAgent(Agent* agent) override {}
  void OnUrgentMessageReceived() override {}
  void OnUrgentMessageProcessed() override {}

 private:
  std::unique_ptr<DummyWebMainThreadScheduler> main_thread_scheduler_;
};

}  // namespace

std::unique_ptr<FrameScheduler> CreateDummyFrameScheduler(
    v8::Isolate* isolate) {
  DCHECK(isolate);
  return std::make_unique<DummyFrameScheduler>(isolate);
}

std::unique_ptr<PageScheduler> CreateDummyPageScheduler(v8::Isolate* isolate) {
  // TODO(crbug.com/1315595): Assert isolate is non-null.
  return std::make_unique<DummyPageScheduler>(isolate);
}

AgentGroupScheduler* CreateDummyAgentGroupScheduler(v8::Isolate* isolate) {
  // TODO(crbug.com/1315595): Assert isolate is non-null.
  return MakeGarbageCollected<DummyAgentGroupScheduler>(isolate);
}

std::unique_ptr<WebThreadScheduler> CreateDummyWebMainThreadScheduler() {
  return std::make_unique<DummyWebMainThreadScheduler>();
}

std::unique_ptr<MainThread> CreateSimpleMainThread() {
  return std::make_unique<SimpleMainThreadWithScheduler>();
}

void SetMainThreadTaskRunnerForTesting() {
  static_cast<SimpleMainThread*>(Thread::MainThread())
      ->SetMainThreadTaskRunnerForTesting(
          base::SingleThreadTaskRunner::GetCurrentDefault());
}

void UnsetMainThreadTaskRunnerForTesting() {
  static_cast<SimpleMainThread*>(Thread::MainThread())
      ->SetMainThreadTaskRunnerForTesting(nullptr);
}

}  // namespace scheduler
}  // namespace blink
