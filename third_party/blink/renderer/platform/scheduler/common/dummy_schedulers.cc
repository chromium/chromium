// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
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

AgentGroupScheduler* CreateDummyAgentGroupSchedulerWithIsolate(
    v8::Isolate* isolate);

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
                                       WebInputEventResult result) override {}
  void DidAnimateForInputOnCompositorThread() override {}
  void DidRunBeginMainFrame() override {}
  void SetHidden(bool hidden) override {}
  void SetHasTouchHandler(bool has_touch_handler) override {}
};

class DummyFrameScheduler : public FrameScheduler {
 public:
  DummyFrameScheduler() : page_scheduler_(CreateDummyPageScheduler()) {}
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
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetShouldReportPostedTasksWhenDisabled(bool) override {}
  void SetCrossOriginToNearestMainFrame(bool) override {}
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
  void DidCommitProvisionalLoad(bool, FrameScheduler::NavigationType) override {
  }
  void OnFirstContentfulPaintInMainFrame() override {}
  void OnFirstMeaningfulPaint() override {}
  void OnLoad() override {}
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

 private:
  std::unique_ptr<PageScheduler> page_scheduler_;
  base::WeakPtrFactory<FrameScheduler> weak_ptr_factory_{this};
};

class DummyPageScheduler : public PageScheduler {
 public:
  DummyPageScheduler()
      : agent_group_scheduler_(CreateDummyAgentGroupScheduler()) {}
  ~DummyPageScheduler() override = default;

  DummyPageScheduler(const DummyPageScheduler&) = delete;
  DummyPageScheduler& operator=(const DummyPageScheduler&) = delete;

  std::unique_ptr<FrameScheduler> CreateFrameScheduler(
      FrameScheduler::Delegate* delegate,
      bool is_in_embedded_frame_tree,
      FrameScheduler::FrameType) override {
    return CreateDummyFrameScheduler();
  }

  void OnTitleOrFaviconUpdated() override {}
  void SetPageVisible(bool) override {}
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

// TODO(altimin,yutak): Merge with SimpleThread in platform.cc.
class SimpleThread : public MainThread {
 public:
  explicit SimpleThread(ThreadScheduler* scheduler) : scheduler_(scheduler) {}
  ~SimpleThread() override {}

  SimpleThread(const SimpleThread&) = delete;
  SimpleThread& operator=(const SimpleThread&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      MainThreadTaskRunnerRestricted) const override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  ThreadScheduler* Scheduler() override { return scheduler_; }

  bool IsCurrentThread() const { return WTF::IsMainThread(); }

 private:
  ThreadScheduler* scheduler_;
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

  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
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
    return std::make_unique<SimpleThread>(this);
  }

  AgentGroupScheduler* CreateAgentGroupScheduler() override {
    return CreateDummyAgentGroupSchedulerWithIsolate(isolate_);
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

  v8::Isolate* Isolate() override {
    DCHECK(isolate_);
    return isolate_;
  }

  void StartIdlePeriodForTesting() override {}

 private:
  v8::Isolate* isolate_ = nullptr;
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
    return CreateDummyPageScheduler();
  }
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  WebThreadScheduler& GetMainThreadScheduler() override {
    return *main_thread_scheduler_;
  }
  void BindInterfaceBroker(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote_broker)
      override {}
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() override {
    return GetEmptyBrowserInterfaceBroker();
  }
  v8::Isolate* Isolate() override { return main_thread_scheduler_->Isolate(); }
  void AddAgent(Agent* agent) override {}

 private:
  std::unique_ptr<DummyWebMainThreadScheduler> main_thread_scheduler_;
};

AgentGroupScheduler* CreateDummyAgentGroupSchedulerWithIsolate(
    v8::Isolate* isolate) {
  return MakeGarbageCollected<DummyAgentGroupScheduler>(isolate);
}

}  // namespace

std::unique_ptr<FrameScheduler> CreateDummyFrameScheduler() {
  return std::make_unique<DummyFrameScheduler>();
}

std::unique_ptr<PageScheduler> CreateDummyPageScheduler() {
  return std::make_unique<DummyPageScheduler>();
}

AgentGroupScheduler* CreateDummyAgentGroupScheduler() {
  return CreateDummyAgentGroupSchedulerWithIsolate(/*isolate=*/nullptr);
}

std::unique_ptr<WebThreadScheduler> CreateDummyWebMainThreadScheduler() {
  return std::make_unique<DummyWebMainThreadScheduler>();
}

}  // namespace scheduler
}  // namespace blink
