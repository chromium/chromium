// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler_group.h"

#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_init_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_trace.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

#if BUILDFLAG(IS_WIN)
// On Windows, assume we have the coarsest possible timer.
static constexpr int kBaseSampleIntervalMs =
    base::Time::kMinLowResolutionThresholdMs;
#else
// Default to a 10ms base sampling interval on other platforms.
// TODO(acomminos): Reevaluate based on empirical overhead.
static constexpr int kBaseSampleIntervalMs = 10;
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class ProfilerGroup::ProfilingContextObserver
    : public GarbageCollected<ProfilingContextObserver>,
      public ExecutionContextLifecycleObserver {
 public:
  ProfilingContextObserver(ProfilerGroup* profiler_group,
                           ExecutionContext* context)
      : ExecutionContextLifecycleObserver(context),
        profiler_group_(profiler_group) {}

  void ContextDestroyed() override {
    DCHECK(profiler_group_);
    profiler_group_->OnProfilingContextDestroyed(this);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(profiler_group_);
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

  // Invariant: ProfilerGroup will outlive the tracked execution context, as
  // the execution context must live as long as the isolate.
  Member<ProfilerGroup> profiler_group_;
};

bool ProfilerGroup::CanProfile(LocalDOMWindow* local_window,
                               ExceptionState* exception_state,
                               ReportOptions report_options) {
  DCHECK(local_window);
  if (!local_window->IsFeatureEnabled(
          mojom::blink::DocumentPolicyFeature::kJSProfiling, report_options)) {
    if (exception_state) {
      exception_state->ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "JS profiling is disabled by Document Policy.");
    }
    return false;
  }

  return true;
}

void ProfilerGroup::InitializeIfEnabled(LocalDOMWindow* local_window) {
  if (ProfilerGroup::CanProfile(local_window)) {
    auto* profiler_group = ProfilerGroup::From(local_window->GetIsolate());
    profiler_group->OnProfilingContextAdded(local_window);
  }
}

ProfilerGroup* ProfilerGroup::From(v8::Isolate* isolate) {
  auto* isolate_data = V8PerIsolateData::From(isolate);
  auto* profiler_group =
      reinterpret_cast<ProfilerGroup*>(isolate_data->GetUserData(
          V8PerIsolateData::UserData::Key::kProfileGroup));
  if (!profiler_group) {
    profiler_group = MakeGarbageCollected<ProfilerGroup>(isolate);
    isolate_data->SetUserData(V8PerIsolateData::UserData::Key::kProfileGroup,
                              profiler_group);
  }
  return profiler_group;
}

base::TimeDelta ProfilerGroup::GetBaseSampleInterval() {
  return base::Milliseconds(kBaseSampleIntervalMs);
}

ProfilerGroup::ProfilerGroup(v8::Isolate* isolate)
    : isolate_(isolate),
      cpu_profiler_(nullptr),
      next_profiler_id_(0),
      num_active_profilers_(0) {}

void DiscardedSamplesDelegate::Notify() {
  if (profiler_group_) {
    profiler_group_->DispatchSampleBufferFullEvent(profiler_id_);
  }
}

void ProfilerGroup::OnProfilingContextAdded(ExecutionContext* context) {
  // Retain an observer for the context's lifetime. During which, keep the V8
  // profiler alive.
  auto* observer =
      MakeGarbageCollected<ProfilingContextObserver>(this, context);
  context_observers_.insert(observer);

  if (!cpu_profiler_) {
    InitV8Profiler();
    DCHECK(cpu_profiler_);
  }
}

void ProfilerGroup::DispatchSampleBufferFullEvent(String profiler_id) {
  for (const auto& profiler : profilers_) {
    if (profiler->ProfilerId() == profiler_id) {
      profiler->DispatchEvent(
          *Event::Create(event_type_names::kSamplebufferfull));
      break;
    }
  }
}

Profiler* ProfilerGroup::CreateProfiler(ScriptState* script_state,
                                        const ProfilerInitOptions& init_options,
                                        base::TimeTicks time_origin,
                                        ExceptionState& exception_state) {
  DCHECK_EQ(script_state->GetIsolate(), isolate_);
  DCHECK(init_options.hasSampleInterval());

  const base::TimeDelta sample_interval =
      base::Milliseconds(init_options.sampleInterval());
  const int64_t sample_interval_us = sample_interval.InMicroseconds();

  if (sample_interval_us < 0 ||
      sample_interval_us > std::numeric_limits<int>::max()) {
    exception_state.ThrowRangeError("Invalid sample interval");
    return nullptr;
  }

  if (!cpu_profiler_) {
    DCHECK(false);
    exception_state.ThrowTypeError("Error creating profiler");
    return nullptr;
  }

  String profiler_id = NextProfilerId();

  v8::CpuProfilingStatus status = cpu_profiler_->StartProfiling(
      V8String(isolate_, profiler_id),
      v8::CpuProfilingOptions(
          v8::kLeafNodeLineNumbers, init_options.maxBufferSize(),
          static_cast<int>(sample_interval_us), script_state->GetContext()),
      std::make_unique<DiscardedSamplesDelegate>(this, profiler_id));

  switch (status) {
    case v8::CpuProfilingStatus::kErrorTooManyProfilers: {
      exception_state.ThrowTypeError(
          "Reached maximum concurrent amount of profilers");
      return nullptr;
    }
    case v8::CpuProfilingStatus::kAlreadyStarted: {
      // Since we increment the profiler id for every invocation of
      // StartProfiling, we do not expect to hit kAlreadyStarted status
      DCHECK(false);
      return nullptr;
    }
    case v8::CpuProfilingStatus::kStarted: {
      // Limit non-crossorigin script frames to the origin that started the
      // profiler.
      auto* execution_context = ExecutionContext::From(script_state);
      scoped_refptr<const SecurityOrigin> source_origin(
          execution_context->GetSecurityOrigin());

      // The V8 CPU profiler ticks in multiples of the base sampling interval.
      // This effectively means that we gather samples at the multiple of the
      // base sampling interval that's greater than or equal to the requested
      // interval.
      int effective_sample_interval_ms =
          static_cast<int>(sample_interval.InMilliseconds());
      if (effective_sample_interval_ms % kBaseSampleIntervalMs != 0 ||
          effective_sample_interval_ms == 0) {
        effective_sample_interval_ms +=
            (kBaseSampleIntervalMs -
             effective_sample_interval_ms % kBaseSampleIntervalMs);
      }

      auto* profiler = MakeGarbageCollected<Profiler>(
          this, script_state, profiler_id, effective_sample_interval_ms,
          source_origin, time_origin);
      profilers_.insert(profiler);
      num_active_profilers_++;
      return profiler;
    }
  }
}

ProfilerGroup::~ProfilerGroup() {
  // v8::CpuProfiler should have been torn down by WillBeDestroyed.
  DCHECK(!cpu_profiler_);
}

void ProfilerGroup::WillBeDestroyed() {
  while (!profilers_.empty()) {
    Profiler* profiler = profilers_.begin()->Get();
    DCHECK(profiler);
    CancelProfiler(profiler);
    profiler->RemovedFromProfilerGroup();
    DCHECK(profiler->stopped());
    DCHECK(!profilers_.Contains(profiler));
  }

  StopDetachedProfilers();

  if (cpu_profiler_)
    TeardownV8Profiler();
}

void ProfilerGroup::Trace(Visitor* visitor) const {
  visitor->Trace(profilers_);
  visitor->Trace(context_observers_);
  V8PerIsolateData::UserData::Trace(visitor);
}

void ProfilerGroup::OnProfilingContextDestroyed(
    ProfilingContextObserver* observer) {
  context_observers_.erase(observer);
  if (context_observers_.size() == 0) {
    WillBeDestroyed();
  }
}

void ProfilerGroup::InitV8Profiler() {
  DCHECK(!cpu_profiler_);
  DCHECK_EQ(num_active_profilers_, 0);

  cpu_profiler_ =
      v8::CpuProfiler::New(isolate_, v8::kStandardNaming, v8::kEagerLogging);
#if BUILDFLAG(IS_WIN)
  // Avoid busy-waiting on Windows, clamping us to the system clock interrupt
  // interval in the worst case.
  cpu_profiler_->SetUsePreciseSampling(false);
#endif  // BUILDFLAG(IS_WIN)
  cpu_profiler_->SetSamplingInterval(kBaseSampleIntervalMs *
                                     base::Time::kMicrosecondsPerMillisecond);
}

void ProfilerGroup::TeardownV8Profiler() {
  DCHECK(cpu_profiler_);
  DCHECK_EQ(num_active_profilers_, 0);

  cpu_profiler_->Dispose();
  cpu_profiler_ = nullptr;
}

void ProfilerGroup::StopProfiler(
    ScriptState* script_state,
    Profiler* profiler,
    ScriptPromiseResolver<ProfilerTrace>* resolver) {
  DCHECK(cpu_profiler_);
  DCHECK(!profiler->stopped());

  v8::Local<v8::String> profiler_id =
      V8String(isolate_, profiler->ProfilerId());
  auto* profile = cpu_profiler_->StopProfiling(profiler_id);
  auto* trace = ProfilerTraceBuilder::FromProfile(
      script_state, profile, profiler->SourceOrigin(), profiler->TimeOrigin());
  resolver->Resolve(trace);

  if (profile)
    profile->Delete();

  profilers_.erase(profiler);
  --num_active_profilers_;
}

void ProfilerGroup::CancelProfiler(Profiler* profiler) {
  DCHECK(cpu_profiler_);
  DCHECK(!profiler->stopped());
  profilers_.erase(profiler);
  CancelProfilerImpl(profiler->ProfilerId());
}

void ProfilerGroup::CancelProfilerAsync(ScriptState* script_state,
                                        Profiler* profiler) {
  DCHECK(IsMainThread());
  DCHECK(cpu_profiler_);
  DCHECK(!profiler->stopped());
  profilers_.erase(profiler);

  // register the profiler to be cleaned up in case its associated context
  // gets destroyed before the cleanup task is executed.
  detached_profiler_ids_.push_back(profiler->ProfilerId());

  // Since it's possible for the profiler to get destructed along with its
  // associated context, dispatch a task to cleanup context-independent isolate
  // resources (rather than use the context's task runner).
  ThreadScheduler::Current()->V8TaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&ProfilerGroup::StopDetachedProfiler,
                               WrapPersistent(this), profiler->ProfilerId()));
}

void ProfilerGroup::StopDetachedProfiler(String profiler_id) {
  DCHECK(IsMainThread());

  // we use a vector instead of a map because the expected number of profiler
  // is expected to be very small
  auto it = base::ranges::find(detached_profiler_ids_, profiler_id);

  if (it == detached_profiler_ids_.end()) {
    // Profiler already stopped
    return;
  }

  CancelProfilerImpl(profiler_id);
  detached_profiler_ids_.erase(it);
}

void ProfilerGroup::StopDetachedProfilers() {
  DCHECK(IsMainThread());

  for (auto& detached_profiler_id : detached_profiler_ids_) {
    CancelProfilerImpl(detached_profiler_id);
  }
  detached_profiler_ids_.clear();
}

void ProfilerGroup::CancelProfilerImpl(String profiler_id) {
  if (!cpu_profiler_)
    return;

  v8::HandleScope scope(isolate_);
  v8::Local<v8::String> v8_profiler_id = V8String(isolate_, profiler_id);
  auto* profile = cpu_profiler_->StopProfiling(v8_profiler_id);

  profile->Delete();
  --num_active_profilers_;
}

String ProfilerGroup::NextProfilerId() {
  auto id = String::Format("blink::Profiler[%d]", next_profiler_id_);
  ++next_profiler_id_;
  return id;
}

}  // namespace blink
