// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/stack_unwinder_arm64_android.h"

#include "base/profiler/register_context.h"
#include "base/profiler/stack_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

constexpr size_t kMaxFrameCount = 5;
constexpr size_t kFrameSize = 40;
constexpr size_t kStackSize = 1024;
constexpr uintptr_t kFirstSamplePc = 0x12345678;

void SetValue(uintptr_t address, uintptr_t value) {
  uintptr_t* ptr = reinterpret_cast<uintptr_t*>(address);
  *ptr = value;
}

void FillFrames(uintptr_t fp, size_t frame_count) {
  for (size_t i = 0; i < frame_count; ++i) {
    SetValue(fp, fp + kFrameSize);
    SetValue(fp + sizeof(uintptr_t), kFirstSamplePc + i);
    fp = fp + kFrameSize;
  }
}

}  // namespace

TEST(UnwinderArm64Test, UnwindValidStack) {
  base::RegisterContext register_context{};
  base::ModuleCache module_cache;
  base::StackBuffer buffer(kStackSize);
  memset(buffer.buffer(), 0, kStackSize);

  const uintptr_t fp = reinterpret_cast<uintptr_t>(buffer.buffer());
  const size_t stack_top = fp + kStackSize;
  register_context.regs[29] = fp;
  register_context.sp = fp;
  FillFrames(fp, kMaxFrameCount);

  UnwinderArm64 unwinder;
  unwinder.Initialize(&module_cache);
  std::vector<base::Frame> stack;
  EXPECT_EQ(base::UnwindResult::COMPLETED,
            unwinder.TryUnwind(&register_context, stack_top, &stack));

  ASSERT_EQ(kMaxFrameCount, stack.size());
  for (size_t i = 0; i < kMaxFrameCount; ++i) {
    EXPECT_EQ(kFirstSamplePc + i, stack[i].instruction_pointer);
    EXPECT_EQ(nullptr, stack[i].module);
  }
}

TEST(UnwinderArm64Test, UnwindInvalidFirstFrame) {
  base::RegisterContext register_context{};
  base::ModuleCache module_cache;
  base::StackBuffer buffer(kStackSize);
  memset(buffer.buffer(), 0, kStackSize);

  uintptr_t fp = reinterpret_cast<uintptr_t>(buffer.buffer());
  const size_t stack_top = fp + kStackSize;
  register_context.regs[29] = fp;
  register_context.sp = fp;
  // FP from register context points bad frame within stack.
  fp += 80;
  FillFrames(fp, kMaxFrameCount);

  UnwinderArm64 unwinder;
  unwinder.Initialize(&module_cache);
  std::vector<base::Frame> stack;
  EXPECT_EQ(base::UnwindResult::COMPLETED,
            unwinder.TryUnwind(&register_context, stack_top, &stack));

  // One extra frame is added when scanning starts.
  ASSERT_EQ(kMaxFrameCount + 1, stack.size());
  for (size_t i = 0; i < kMaxFrameCount; ++i) {
    EXPECT_EQ(kFirstSamplePc + i, stack[i + 1].instruction_pointer);
    EXPECT_EQ(nullptr, stack[i + 1].module);
  }
}

TEST(UnwinderArm64Test, UnwindInvalidFp) {
  base::RegisterContext register_context{};
  base::ModuleCache module_cache;
  base::StackBuffer buffer(kStackSize);
  memset(buffer.buffer(), 0, kStackSize);

  uintptr_t fp = reinterpret_cast<uintptr_t>(buffer.buffer());
  const size_t stack_top = fp + kStackSize;

  // FP points to a bad value. SP will be used to start unwinding. SP points to
  // some point in end of stack, but the first frame starts after 20 bytes.
  register_context.regs[29] = 50;
  register_context.sp = fp;
  fp += 80;

  FillFrames(fp, kMaxFrameCount);

  UnwinderArm64 unwinder;
  unwinder.Initialize(&module_cache);
  std::vector<base::Frame> stack;
  EXPECT_EQ(base::UnwindResult::COMPLETED,
            unwinder.TryUnwind(&register_context, stack_top, &stack));

  // One extra frame is added when scanning starts.
  ASSERT_EQ(kMaxFrameCount + 1, stack.size());
  for (size_t i = 0; i < kMaxFrameCount; ++i) {
    EXPECT_EQ(kFirstSamplePc + i, stack[i + 1].instruction_pointer);
    EXPECT_EQ(nullptr, stack[i + 1].module);
  }
}

}  // namespace tracing
