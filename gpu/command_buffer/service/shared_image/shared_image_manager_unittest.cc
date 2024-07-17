// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {
namespace {

std::unique_ptr<TestImageBacking> CreateImageBacking(size_t size_in_bytes) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;

  return std::make_unique<TestImageBacking>(mailbox, format, size, color_space,
                                            surface_origin, alpha_type, usage,
                                            size_in_bytes);
}

TEST(SharedImageManagerTest, BasicRefCounting) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;

  auto backing = std::make_unique<TestImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Taking/releasing an additional ref/representation with the same tracker
  // should have no impact.
  {
    auto gl_representation = manager.ProduceGLTexture(mailbox, tracker.get());
    EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
  }

  // Taking/releasing an additional ref/representation with a new tracker should
  // also have no impact.
  {
    auto tracker2 = std::make_unique<MemoryTypeTracker>(nullptr);
    auto gl_representation = manager.ProduceGLTexture(mailbox, tracker2.get());
    EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
    EXPECT_EQ(0u, tracker2->GetMemRepresented());
  }

  factory_ref.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
}

TEST(SharedImageManagerTest, MemoryDumps) {
  constexpr size_t kSizeBytes1 = 1000;
  constexpr size_t kSizeBytes2 = 2000;

  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);

  auto factory_ref1 =
      manager.Register(CreateImageBacking(kSizeBytes1), tracker.get());
  auto factory_ref2 =
      manager.Register(CreateImageBacking(kSizeBytes2), tracker.get());

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kBackground};
  base::trace_event::ProcessMemoryDump pmd(args);

  manager.OnMemoryDump(args, &pmd);

  auto* dump = pmd.GetAllocatorDump("gpu/shared_images");
  ASSERT_NE(nullptr, dump);
  ASSERT_EQ(dump->entries().size(), 3u);

  for (const auto& entry : dump->entries()) {
    if (entry.name == "size") {
      EXPECT_EQ(entry.name, base::trace_event::MemoryAllocatorDump::kNameSize);
      EXPECT_EQ(entry.units,
                base::trace_event::MemoryAllocatorDump::kUnitsBytes);
      EXPECT_EQ(entry.entry_type,
                base::trace_event::MemoryAllocatorDump::Entry::kUint64);
      EXPECT_EQ(entry.value_uint64, kSizeBytes1 + kSizeBytes2);
    } else if (entry.name == "purgeable_size") {
      EXPECT_EQ(entry.units,
                base::trace_event::MemoryAllocatorDump::kUnitsBytes);
      EXPECT_EQ(entry.entry_type,
                base::trace_event::MemoryAllocatorDump::Entry::kUint64);
      // Nothing is purgeable.
      EXPECT_EQ(entry.value_uint64, 0u);
    } else if (entry.name == "non_exo_size") {
      EXPECT_EQ(entry.units,
                base::trace_event::MemoryAllocatorDump::kUnitsBytes);
      EXPECT_EQ(entry.entry_type,
                base::trace_event::MemoryAllocatorDump::Entry::kUint64);
      // Nothing is purgeable.
      EXPECT_EQ(entry.value_uint64, kSizeBytes1 + kSizeBytes2);
    } else {
      FAIL() << "Unexpected memory dump entry name: " << entry.name;
    }
  }
}

TEST(SharedImageManagerTest, TransferRefSameTracker) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;

  auto backing = std::make_unique<TestImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Take an additional ref/representation.
  auto gl_representation = manager.ProduceGLTexture(mailbox, tracker.get());

  // Releasing the original ref should have no impact.
  factory_ref.reset();
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  gl_representation.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
}

TEST(SharedImageManagerTest, TransferRefNewTracker) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);
  auto tracker2 = std::make_unique<MemoryTypeTracker>(nullptr);

  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;

  auto backing = std::make_unique<TestImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      kSizeBytes);

  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());

  // Take an additional ref/representation with a new tracker. Memory should
  // stay accounted to the original tracker.
  auto gl_representation = manager.ProduceGLTexture(mailbox, tracker2.get());
  EXPECT_EQ(kSizeBytes, tracker->GetMemRepresented());
  EXPECT_EQ(0u, tracker2->GetMemRepresented());

  // Releasing the original should transfer memory to the new tracker.
  factory_ref.reset();
  EXPECT_EQ(0u, tracker->GetMemRepresented());
  EXPECT_EQ(kSizeBytes, tracker2->GetMemRepresented());

  // We can now safely destroy the original tracker.
  tracker.reset();

  gl_representation.reset();
  EXPECT_EQ(0u, tracker2->GetMemRepresented());
}

class SequenceValidatingMemoryTracker : public MemoryTracker {
 public:
  SequenceValidatingMemoryTracker()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

  ~SequenceValidatingMemoryTracker() override { EXPECT_EQ(size_, 0u); }

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  void TrackMemoryAllocatedChange(int64_t delta) override {
    EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    size_ += delta;
  }

  uint64_t GetSize() const override { return size_; }
  int ClientId() const override { return 0; }
  uint64_t ClientTracingId() const override { return 0; }
  uint64_t ContextGroupTracingId() const override { return 0; }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int64_t size_ = 0;
};

TEST(SharedImageManagerTest, TransferRefCrossThread) {
  const size_t kSizeBytes = 1024;
  SharedImageManager manager;
  SequenceValidatingMemoryTracker memory_tracker1;
  SequenceValidatingMemoryTracker memory_tracker2;

  auto memory_type_tracker1 = std::make_unique<MemoryTypeTracker>(
      &memory_tracker1, memory_tracker1.task_runner());
  auto memory_type_tracker2 = std::make_unique<MemoryTypeTracker>(
      &memory_tracker2, memory_tracker2.task_runner());

  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;

  auto backing = std::make_unique<TestImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      kSizeBytes);

  auto factory_ref =
      manager.Register(std::move(backing), memory_type_tracker1.get());
  EXPECT_EQ(kSizeBytes, memory_type_tracker1->GetMemRepresented());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(kSizeBytes, memory_tracker1.GetSize());

  // Take an additional ref/representation with a new tracker. Memory should
  // stay accounted to the original tracker.
  auto gl_representation =
      manager.ProduceGLTexture(mailbox, memory_type_tracker2.get());
  EXPECT_EQ(kSizeBytes, memory_type_tracker1->GetMemRepresented());
  EXPECT_EQ(0u, memory_type_tracker2->GetMemRepresented());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(kSizeBytes, memory_tracker1.GetSize());
  EXPECT_EQ(0u, memory_tracker2.GetSize());

  // Releasing the original should transfer memory to the new tracker.
  factory_ref.reset();
  EXPECT_EQ(0u, memory_type_tracker1->GetMemRepresented());
  EXPECT_EQ(kSizeBytes, memory_type_tracker2->GetMemRepresented());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(0u, memory_tracker1.GetSize());
  EXPECT_EQ(kSizeBytes, memory_tracker2.GetSize());

  // We can now safely destroy the original tracker.
  memory_type_tracker1.reset();

  gl_representation.reset();
  EXPECT_EQ(0u, memory_type_tracker2->GetMemRepresented());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(0u, memory_tracker2.GetSize());
}

TEST(SharedImageManagerTest, GetUsageForMailbox) {
  const size_t kSizeBytes = 1024;

  auto backing = CreateImageBacking(kSizeBytes);
  const gpu::Mailbox mailbox = backing->mailbox();
  const gpu::SharedImageUsageSet usage = backing->usage();

  SharedImageManager manager;
  EXPECT_EQ(std::nullopt, manager.GetUsageForMailbox(mailbox));

  auto tracker = std::make_unique<MemoryTypeTracker>(nullptr);
  auto factory_ref = manager.Register(std::move(backing), tracker.get());
  EXPECT_EQ(std::make_optional(usage), manager.GetUsageForMailbox(mailbox));

  factory_ref.reset();
  EXPECT_EQ(std::nullopt, manager.GetUsageForMailbox(mailbox));
}

}  // anonymous namespace
}  // namespace gpu
