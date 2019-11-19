// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/stack_unwinder_android.h"

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/bind.h"
#include "base/profiler/stack_buffer.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/trace_event/cfi_backtrace_android.h"
#include "services/tracing/jni_headers/UnwindTestHelper_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

const size_t kMaxStackFrames = 40;

class StackUnwinderTest : public testing::Test {
 public:
  StackUnwinderTest() : testing::Test() {}
  ~StackUnwinderTest() override {}

  void SetUp() override {
    unwinder_.Initialize();
    base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
        ->AllocateCacheForCurrentThread();
  }

  StackUnwinderAndroid* unwinder() { return &unwinder_; }

 private:
  StackUnwinderAndroid unwinder_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(StackUnwinderTest);
};

uintptr_t GetCurrentPC() {
  return reinterpret_cast<uintptr_t>(__builtin_return_address(0));
}

}  // namespace

TEST_F(StackUnwinderTest, UnwindCurrentThread) {
  const void* frames[kMaxStackFrames];
  size_t result = unwinder()->TraceStack(frames, kMaxStackFrames);
  EXPECT_GT(result, 0u);

  // Since we are starting from chrome library function (this), all the unwind
  // frames will be chrome frames.
  for (size_t i = 0; i < result; ++i) {
    EXPECT_TRUE(
        base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
            ->is_chrome_address(reinterpret_cast<uintptr_t>(frames[i])));
  }
}

TEST_F(StackUnwinderTest, UnwindOtherThread) {
  base::WaitableEvent unwind_finished_event;
  auto task_runner = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  auto callback = [](StackUnwinderAndroid* unwinder, base::PlatformThreadId tid,
                     base::WaitableEvent* unwind_finished_event,
                     uintptr_t test_pc) {
    const void* frames[kMaxStackFrames];
    auto stack_buffer = base::StackSampler::CreateStackBuffer();
    EXPECT_GT(stack_buffer->size(), 0u);
    size_t result =
        unwinder->TraceStack(tid, stack_buffer.get(), frames, kMaxStackFrames);
    EXPECT_GT(result, 0u);
    bool current_function_found = false;
    for (size_t i = 0; i < result; ++i) {
      uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
      if (addr != 0)
        EXPECT_TRUE(unwinder->IsAddressMapped(addr));
      if (addr >= test_pc && addr < test_pc + 100)
        current_function_found = true;
    }
    EXPECT_TRUE(current_function_found);

    unwind_finished_event->Signal();
  };

  // Post task on background thread to unwind the current thread.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(callback, base::Unretained(unwinder()),
                                       base::PlatformThread::CurrentId(),
                                       &unwind_finished_event, GetCurrentPC()));

  // While the background thread is trying to unwind make some slow framework
  // calls (malloc) so that the current thread can be stopped in framework
  // library functions on stack.
  while (true) {
    std::vector<int> temp;
    temp.reserve(kMaxStackFrames);
    usleep(100);

    if (unwind_finished_event.IsSignaled())
      break;
  }
}

TEST_F(StackUnwinderTest, UnwindOtherThreadOnJNICall) {
  auto task_runner = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  auto callback = [](StackUnwinderAndroid* unwinder, base::PlatformThreadId tid,
                     uintptr_t test_pc) {
    const void* frames[kMaxStackFrames];
    auto stack_buffer = base::StackSampler::CreateStackBuffer();
    EXPECT_GT(stack_buffer->size(), 0u);
    size_t result =
        unwinder->TraceStack(tid, stack_buffer.get(), frames, kMaxStackFrames);

    bool found_jni = false;
    uintptr_t jni_address =
        reinterpret_cast<uintptr_t>(&Java_UnwindTestHelper_blockCurrentThread);
    EXPECT_GT(result, 2u);
    for (size_t i = 0; i < result; ++i) {
      uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
      EXPECT_TRUE(unwinder->IsAddressMapped(addr));
      // Check if address is near |jni_address|.
      if (addr > jni_address && addr < jni_address + 50)
        found_jni = true;
    }
    EXPECT_TRUE(found_jni);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_UnwindTestHelper_unblockAllThreads(env);
  };

  // Post task on background thread to unwind the current thread.
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(callback, base::Unretained(unwinder()),
                     base::PlatformThread::CurrentId(), GetCurrentPC()),
      base::TimeDelta::FromMilliseconds(10));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnwindTestHelper_blockCurrentThread(env);
}

TEST_F(StackUnwinderTest, JNIMarkerConsistent) {
  jni_generator::JniJavaCallContextUnchecked marker;
  JNIEnv* env = base::android::AttachCurrentThread();
  std::atomic<jmethodID> method(nullptr);

  uintptr_t sp, pc;
  asm volatile("mov %0, sp" : "=r"(sp));
  asm volatile("mov %0, pc" : "=r"(pc));
  marker.Init<base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_tracing_UnwindTestHelper_clazz(env),
      "blockCurrentThread", "()V", &method);

  EXPECT_EQ(marker.sp, sp);
  // The |marker.pc| recorded should be close from where we recorded |pc|.
  EXPECT_NEAR(marker.pc, pc, 50);
}

}  // namespace tracing
