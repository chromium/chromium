// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include <algorithm>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/stack_allocated.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/thread_isolation.h"
#include "gin/v8_platform_thread_isolated_allocator.h"
#include "partition_alloc/buildflags.h"
#include "v8_platform_page_allocator.h"

namespace gin {

namespace {

base::LazyInstance<V8Platform>::Leaky g_v8_platform = LAZY_INSTANCE_INITIALIZER;

void PrintStackTrace() {
  base::debug::StackTrace trace;
  trace.Print();
}

class ConvertableToTraceFormatWrapper final
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit ConvertableToTraceFormatWrapper(
      std::unique_ptr<v8::ConvertableToTraceFormat> inner)
      : inner_(std::move(inner)) {}
  ConvertableToTraceFormatWrapper(const ConvertableToTraceFormatWrapper&) =
      delete;
  ConvertableToTraceFormatWrapper& operator=(
      const ConvertableToTraceFormatWrapper&) = delete;
  ~ConvertableToTraceFormatWrapper() override = default;
  void AppendAsTraceFormat(std::string* out) const final {
    inner_->AppendAsTraceFormat(out);
  }

 private:
  std::unique_ptr<v8::ConvertableToTraceFormat> inner_;
};


#if PA_BUILDFLAG(USE_PARTITION_ALLOC)

base::LazyInstance<gin::PageAllocator>::Leaky g_page_allocator =
    LAZY_INSTANCE_INITIALIZER;

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

base::TaskPriority ToBaseTaskPriority(v8::TaskPriority priority) {
  switch (priority) {
    case v8::TaskPriority::kBestEffort:
      return base::TaskPriority::BEST_EFFORT;
    case v8::TaskPriority::kUserVisible:
      return base::TaskPriority::USER_VISIBLE;
    case v8::TaskPriority::kUserBlocking:
      return base::TaskPriority::USER_BLOCKING;
  }
}

class JobDelegateImpl : public v8::JobDelegate {
  STACK_ALLOCATED();

 public:
  explicit JobDelegateImpl(base::JobDelegate* delegate) : delegate_(delegate) {}
  JobDelegateImpl() = default;

  JobDelegateImpl(const JobDelegateImpl&) = delete;
  JobDelegateImpl& operator=(const JobDelegateImpl&) = delete;

  // v8::JobDelegate:
  bool ShouldYield() override { return delegate_->ShouldYield(); }
  void NotifyConcurrencyIncrease() override {
    delegate_->NotifyConcurrencyIncrease();
  }
  uint8_t GetTaskId() override { return delegate_->GetTaskId(); }
  bool IsJoiningThread() const override { return delegate_->IsJoiningThread(); }

 private:
  base::JobDelegate* delegate_ = nullptr;
};

class JobHandleImpl : public v8::JobHandle {
 public:
  explicit JobHandleImpl(base::JobHandle handle) : handle_(std::move(handle)) {}
  ~JobHandleImpl() override = default;

  JobHandleImpl(const JobHandleImpl&) = delete;
  JobHandleImpl& operator=(const JobHandleImpl&) = delete;

  // v8::JobHandle:
  void NotifyConcurrencyIncrease() override {
    handle_.NotifyConcurrencyIncrease();
  }
  bool UpdatePriorityEnabled() const override { return true; }
  void UpdatePriority(v8::TaskPriority new_priority) override {
    handle_.UpdatePriority(ToBaseTaskPriority(new_priority));
  }
  void Join() override { handle_.Join(); }
  void Cancel() override { handle_.Cancel(); }
  void CancelAndDetach() override { handle_.CancelAndDetach(); }
  bool IsActive() override { return handle_.IsActive(); }
  bool IsValid() override { return !!handle_; }

 private:
  base::JobHandle handle_;
};

class ScopedBlockingCallImpl : public v8::ScopedBlockingCall {
 public:
  explicit ScopedBlockingCallImpl(v8::BlockingType blocking_type)
      : scoped_blocking_call_(ToBaseBlockingType(blocking_type),
                              base::internal::UncheckedScopedBlockingCall::
                                  BlockingCallType::kRegular) {}
  ~ScopedBlockingCallImpl() override = default;

  ScopedBlockingCallImpl(const ScopedBlockingCallImpl&) = delete;
  ScopedBlockingCallImpl& operator=(const ScopedBlockingCallImpl&) = delete;

 private:
  static base::BlockingType ToBaseBlockingType(v8::BlockingType type) {
    switch (type) {
      case v8::BlockingType::kMayBlock:
        return base::BlockingType::MAY_BLOCK;
      case v8::BlockingType::kWillBlock:
        return base::BlockingType::WILL_BLOCK;
    }
  }

  base::internal::UncheckedScopedBlockingCall scoped_blocking_call_;
};

}  // namespace

}  // namespace gin

// Allow std::unique_ptr<v8::ConvertableToTraceFormat> to be a valid
// initialization value for trace macros.
template <>
struct base::trace_event::TraceValue::Helper<
    std::unique_ptr<v8::ConvertableToTraceFormat>> {
  static constexpr unsigned char kType = TRACE_VALUE_TYPE_CONVERTABLE;
  static inline void SetValue(
      TraceValue* v,
      std::unique_ptr<v8::ConvertableToTraceFormat> value) {
    // NOTE: |as_convertable| is an owning pointer, so using new here
    // is acceptable.
    v->as_convertable =
        new gin::ConvertableToTraceFormatWrapper(std::move(value));
  }
};

namespace gin {

class V8Platform::TracingControllerImpl : public v8::TracingController {
 public:
  TracingControllerImpl() = default;
  TracingControllerImpl(const TracingControllerImpl&) = delete;
  TracingControllerImpl& operator=(const TracingControllerImpl&) = delete;
  ~TracingControllerImpl() override = default;

  // TracingController implementation.
};

// static
V8Platform* V8Platform::Get() { return g_v8_platform.Pointer(); }

V8Platform::V8Platform() : tracing_controller_(new TracingControllerImpl) {}

V8Platform::~V8Platform() = default;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
PageAllocator* V8Platform::GetPageAllocator() {
  return g_page_allocator.Pointer();
}

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
ThreadIsolatedAllocator* V8Platform::GetThreadIsolatedAllocator() {
  if (!GetThreadIsolationData().Initialized()) {
    return nullptr;
  }
  return GetThreadIsolationData().allocator.get();
}
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

void V8Platform::OnCriticalMemoryPressure() {
// We only have a reservation on 32-bit Windows systems.
// TODO(bbudge) Make the #if's in BlinkInitializer match.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_32_BITS)
  partition_alloc::ReleaseReservation();
#endif
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

std::shared_ptr<v8::TaskRunner> V8Platform::GetForegroundTaskRunner(
    v8::Isolate* isolate,
    v8::TaskPriority priority) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  switch (priority) {
    case v8::TaskPriority::kBestEffort:
      // blink::scheduler::TaskPriority::kLowPriority
      if (data->best_effort_task_runner()) {
        return data->best_effort_task_runner();
      }
      [[fallthrough]];
    case v8::TaskPriority::kUserVisible:
      // blink::scheduler::TaskPriority::kLowPriority
      if (data->user_visible_task_runner()) {
        return data->user_visible_task_runner();
      }
      [[fallthrough]];
    case v8::TaskPriority::kUserBlocking:
      // blink::scheduler::TaskPriority::kDefaultPriority
      return data->task_runner();
    default:
      NOTREACHED() << "Unsupported TaskPriority.";
  }
}

int V8Platform::NumberOfWorkerThreads() {
  // V8Platform assumes the number of workers used by the scheduler for user
  // blocking tasks is an upper bound.
  const size_t num_foreground_workers =
      base::ThreadPoolInstance::Get()
          ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
              {base::TaskPriority::USER_BLOCKING});
  DCHECK_GE(num_foreground_workers,
            base::ThreadPoolInstance::Get()
                ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                    {base::TaskPriority::USER_VISIBLE}));
  return std::max(1, static_cast<int>(num_foreground_workers));
}

void V8Platform::PostTaskOnWorkerThreadImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location) {
  base::ThreadPool::PostTask(V8ToBaseLocation(location),
                             {ToBaseTaskPriority(priority)},
                             base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8Platform::PostDelayedTaskOnWorkerThreadImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  base::ThreadPool::PostDelayedTask(
      V8ToBaseLocation(location), {ToBaseTaskPriority(priority)},
      base::BindOnce(&v8::Task::Run, std::move(task)),
      base::Seconds(delay_in_seconds));
}

std::unique_ptr<v8::JobHandle> V8Platform::CreateJobImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::JobTask> job_task,
    const v8::SourceLocation& location) {
  // Ownership of |job_task| is assumed by |worker_task|, while
  // |max_concurrency_callback| uses an unretained pointer.
  auto* job_task_ptr = job_task.get();
  auto handle = base::CreateJob(
      V8ToBaseLocation(location),
      {ToBaseTaskPriority(priority), base::ThreadPolicy::PREFER_BACKGROUND},
      base::BindRepeating(
          [](const std::unique_ptr<v8::JobTask>& job_task,
             base::JobDelegate* delegate) {
            JobDelegateImpl delegate_impl(delegate);
            job_task->Run(&delegate_impl);
          },
          std::move(job_task)),
      base::BindRepeating(
          [](v8::JobTask* job_task, size_t worker_count) {
            return job_task->GetMaxConcurrency(worker_count);
          },
          base::Unretained(job_task_ptr)));

  return std::make_unique<JobHandleImpl>(std::move(handle));
}

std::unique_ptr<v8::ScopedBlockingCall> V8Platform::CreateBlockingScope(
    v8::BlockingType blocking_type) {
  return std::make_unique<ScopedBlockingCallImpl>(blocking_type);
}

bool V8Platform::IdleTasksEnabled(v8::Isolate* isolate) {
  return PerIsolateData::From(isolate)->task_runner()->IdleTasksEnabled();
}

double V8Platform::MonotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

double V8Platform::CurrentClockTimeMillis() {
  return static_cast<double>(time_clamper_.ClampToMillis(base::Time::Now()));
}

int64_t V8Platform::CurrentClockTimeMilliseconds() {
  return time_clamper_.ClampToMillis(base::Time::Now());
}

double V8Platform::CurrentClockTimeMillisecondsHighResolution() {
  return time_clamper_.ClampToMillisHighResolution(base::Time::Now());
}

v8::TracingController* V8Platform::GetTracingController() {
  return tracing_controller_.get();
}

v8::Platform::StackTracePrinter V8Platform::GetStackTracePrinter() {
  return PrintStackTrace;
}

}  // namespace gin
