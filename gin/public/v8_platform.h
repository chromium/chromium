// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_V8_PLATFORM_H_
#define GIN_PUBLIC_V8_PLATFORM_H_

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "gin/gin_export.h"
#include "gin/time_clamper.h"
#include "gin/v8_platform_page_allocator.h"
#include "gin/v8_platform_thread_isolated_allocator.h"
#include "partition_alloc/buildflags.h"
#include "v8/include/v8-platform.h"

namespace gin {

// A v8::Platform implementation to use with gin.
class GIN_EXPORT V8Platform : public v8::Platform {
 public:
  V8Platform(const V8Platform&) = delete;
  V8Platform& operator=(const V8Platform&) = delete;

  static V8Platform* Get();

// v8::Platform implementation.
// Some configurations do not use page_allocator.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  // GetPageAllocator returns gin::PageAllocator instead of v8::PageAllocator,
  // so we can be sure that the allocator used employs security features such as
  // enabling Arm's Branch Target Instructions for executable pages. This is
  // verified in the tests for gin::PageAllocator.
  PageAllocator* GetPageAllocator() override;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  ThreadIsolatedAllocator* GetThreadIsolatedAllocator() override;
#endif
  void OnCriticalMemoryPressure() override;
#endif

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate,
      v8::TaskPriority priority) override;
  int NumberOfWorkerThreads() override;
  void PostTaskOnWorkerThreadImpl(v8::TaskPriority priority,
                                  std::unique_ptr<v8::Task> task,
                                  const v8::SourceLocation& location) override;
  void PostDelayedTaskOnWorkerThreadImpl(
      v8::TaskPriority priority,
      std::unique_ptr<v8::Task> task,
      double delay_in_seconds,
      const v8::SourceLocation& location) override;
  std::unique_ptr<v8::JobHandle> CreateJobImpl(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task,
      const v8::SourceLocation& location) override;
  std::unique_ptr<v8::ScopedBlockingCall> CreateBlockingScope(
      v8::BlockingType blocking_type) override;
  bool IdleTasksEnabled(v8::Isolate* isolate) override;
  double MonotonicallyIncreasingTime() override;
  double CurrentClockTimeMillis() override;
  int64_t CurrentClockTimeMilliseconds() override;
  double CurrentClockTimeMillisecondsHighResolution() override;
  StackTracePrinter GetStackTracePrinter() override;
  v8::TracingController* GetTracingController() override;

 private:
  friend struct base::LazyInstanceTraitsBase<V8Platform>;

  V8Platform();
  ~V8Platform() override;

  class TracingControllerImpl;
  std::unique_ptr<TracingControllerImpl> tracing_controller_;
  TimeClamper time_clamper_;
};

}  // namespace gin

#endif  // GIN_PUBLIC_V8_PLATFORM_H_
