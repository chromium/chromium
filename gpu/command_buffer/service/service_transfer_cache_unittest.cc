// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/utils.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time_override.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {

constexpr int kDecoderId = 2;
constexpr auto kEntryType = cc::TransferCacheEntryType::kRawMemory;

std::unique_ptr<cc::ServiceTransferCacheEntry> CreateEntry(size_t size) {
  auto entry = std::make_unique<cc::ServiceRawMemoryTransferCacheEntry>();
  std::vector<uint8_t> data(size, 0u);
  entry->Deserialize(/*gr_context=*/nullptr, /*graphite_recorder=*/nullptr,
                     data);
  return entry;
}

TEST(ServiceTransferCacheTest, EnforcesOnReleaseMemory) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  uint32_t number_of_entry = 4u;

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.OnReleaseMemory(base::kCriticalMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);

  cache.SetCacheSizeLimitForTesting(entry_size * number_of_entry);
  // Create 4 entries, all in the cache.
  for (uint32_t i = 0; i < number_of_entry; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
    EXPECT_EQ(cache.cache_size_for_testing(), entry_size * (i + 1));
  }

  // The 5th entry creates successfully. But the 1st entry will be purged due to
  // cache limits.
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  cache.OnReleaseMemory(base::kModerateMemoryPressureThreshold);
  // Only 1/4 of cache limits remains.
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
}

TEST(ServiceTransferCacheTest, OnUpdateMemoryLimitStateful) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);

  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  uint32_t number_of_entry = 4u;

  cache.SetCacheSizeLimitForTesting(entry_size * number_of_entry);
  for (uint32_t i = 0; i < number_of_entry; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
  }
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  // Moderate memory limit (50%). OnUpdateMemoryLimit must NOT release memory,
  // so existing usage remains 4 * entry_size.
  cache.OnUpdateMemoryLimit(base::kModerateMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  // OnReleaseMemory performs the actual eviction down to 1/4 (1024 bytes).
  cache.OnReleaseMemory(base::kModerateMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);

  // Critical memory pressure (0%) purges all entries and persists limit = 0.
  cache.OnReleaseMemory(base::kCriticalMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);
  // Creating a new entry while critical pressure persists immediately evicts
  // it.
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);

  // Restore limit to 100%
  cache.OnUpdateMemoryLimit(base::kNoMemoryPressureThreshold);
  cache.OnReleaseMemory(base::kNoMemoryPressureThreshold);
  // Now we can add up to 4 entries again
  for (uint32_t i = 0; i < 4; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
  }
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);
}

TEST(ServiceTransferCache, MultipleDecoderUse) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const uint32_t entry_id = 0u;
  const size_t entry_size = 1024u;

  // Decoder 1 entry.
  int decoder1 = 1;
  auto decoder_1_entry = CreateEntry(entry_size);
  auto* decoder_1_entry_ptr = decoder_1_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder1, kEntryType, entry_id),
      std::move(decoder_1_entry));

  // Decoder 2 entry.
  int decoder2 = 2;
  auto decoder_2_entry = CreateEntry(entry_size);
  auto* decoder_2_entry_ptr = decoder_2_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder2, kEntryType, entry_id),
      std::move(decoder_2_entry));

  EXPECT_EQ(decoder_1_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder1, kEntryType, entry_id)));
  EXPECT_EQ(decoder_2_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder2, kEntryType, entry_id)));
}

TEST(ServiceTransferCache, EntryIdsBeyond32Bits) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const size_t entry_size = 1024u;

  // Two entry ids that only match in the low 32 bits address distinct
  // entries.
  const uint64_t low_entry_id = 7u;
  const uint64_t high_entry_id = low_entry_id + (uint64_t{1} << 32);

  auto low_entry = CreateEntry(entry_size);
  auto* low_entry_ptr = low_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, low_entry_id),
      std::move(low_entry));

  auto high_entry = CreateEntry(entry_size);
  auto* high_entry_ptr = high_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, high_entry_id),
      std::move(high_entry));

  EXPECT_EQ(cache.entries_count_for_testing(), 2u);
  EXPECT_EQ(low_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                               kDecoderId, kEntryType, low_entry_id)));
  EXPECT_EQ(high_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                kDecoderId, kEntryType, high_entry_id)));
}

TEST(ServiceTransferCache, DeleteEntriesForDecoder) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const size_t entry_size = 1024u;
  const size_t cache_size = 4 * entry_size;
  cache.SetCacheSizeLimitForTesting(cache_size);

  // Add 2 entries for decoder 1.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 2),
                         CreateEntry(entry_size));

  // Add 2 entries for decoder 2.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2),
                         CreateEntry(entry_size));

  // Erase all entries for decoder 1.
  EXPECT_EQ(cache.entries_count_for_testing(), 4u);
  cache.DeleteAllEntriesForDecoder(1);
  EXPECT_EQ(cache.entries_count_for_testing(), 2u);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1)),
            nullptr);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2)),
            nullptr);
}

TEST(ServiceTransferCacheTest, PurgeEntryOnTimer) {
  static base::TimeTicks now_value = base::TimeTicks::Now();
  base::subtle::ScopedTimeClockOverrides time_override(
      nullptr, []() { return now_value; }, nullptr);

  bool flush_called = false;
  ServiceTransferCache cache{
      GpuPreferences(),
      base::BindLambdaForTesting([&]() { flush_called = true; })};

  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.entries_count_for_testing(), 1u);

  now_value = now_value + base::Minutes(1);
  cache.PruneOldEntries();
  EXPECT_EQ(cache.entries_count_for_testing(), 0u);
  EXPECT_TRUE(flush_called);
}

namespace {

// A transfer cache entry that holds an unbudgeted Skia GPU texture (matching
// ServiceImageTransferCacheEntry) and verifies in its destructor that the
// expected SharedContextState and GLContext are current when the entry is
// destroyed.
class ContextCheckingTransferCacheEntry
    : public cc::ServiceTransferCacheEntryBase<
          cc::TransferCacheEntryType::kImage> {
 public:
  ContextCheckingTransferCacheEntry(SharedContextState* context_state,
                                    gl::GLContext* expected_gl_context,
                                    bool* destroyed_with_expected_context)
      : context_state_(context_state),
        expected_gl_context_(expected_gl_context),
        destroyed_with_expected_context_(destroyed_with_expected_context) {
    SkBitmap bm;
    SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
    CHECK(bm.tryAllocPixels(info));
    image_ = SkImages::TextureFromImage(
        context_state_->gr_context(), SkImages::RasterFromBitmap(bm),
        skgpu::Mipmapped::kNo, skgpu::Budgeted::kNo);
    CHECK(image_);
  }

  ~ContextCheckingTransferCacheEntry() override {
    EXPECT_TRUE(context_state_->IsCurrent(nullptr));
    EXPECT_EQ(gl::GLContext::GetCurrent(), expected_gl_context_);
    *destroyed_with_expected_context_ =
        context_state_->IsCurrent(nullptr) &&
        (gl::GLContext::GetCurrent() == expected_gl_context_);
    image_.reset();
  }

  size_t CachedSize() const override { return 400u; }

  bool Deserialize(GrDirectContext* context,
                   skgpu::graphite::Recorder* graphite_recorder,
                   base::span<const uint8_t> data) override {
    return true;
  }

 private:
  raw_ptr<SharedContextState> context_state_;
  raw_ptr<gl::GLContext> expected_gl_context_;
  raw_ptr<bool> destroyed_with_expected_context_;
  sk_sp<SkImage> image_;
};

}  // namespace

class ServiceTransferCacheContextTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPruneOldTransferCacheEntries);
    display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithStubBindings();
    ASSERT_TRUE(display_);

    scoped_refptr<gl::GLShareGroup> share_group_a = new gl::GLShareGroup();
    surface_a_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    context_a_ = gl::init::CreateGLContext(
        share_group_a.get(), surface_a_.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_a_->MakeCurrent(surface_a_.get()));

    GpuDriverBugWorkarounds workarounds;
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group_a), surface_a_, context_a_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        GrContextType::kGL);
    ASSERT_TRUE(context_state_->InitializeSkia(GpuPreferences(), workarounds));
    ASSERT_TRUE(context_state_->InitializeGL(GpuPreferences(), workarounds,
                                             GpuFeatureInfo()));

    // Create a second independent GL context (simulating e.g. a WebGL context
    // on the same thread).
    scoped_refptr<gl::GLShareGroup> share_group_b = new gl::GLShareGroup();
    surface_b_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    context_b_ = gl::init::CreateGLContext(
        share_group_b.get(), surface_b_.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_b_);
  }

  void TearDown() override {
    if (context_state_) {
      ASSERT_TRUE(context_state_->MakeCurrent(nullptr));
      context_state_.reset();
    }
    context_b_.reset();
    surface_b_.reset();
    context_a_.reset();
    surface_a_.reset();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }

  ServiceTransferCache* transfer_cache() {
    return context_state_->transfer_cache();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<gl::GLDisplay> display_ = nullptr;
  scoped_refptr<gl::GLSurface> surface_a_;
  scoped_refptr<gl::GLContext> context_a_;
  scoped_refptr<gl::GLSurface> surface_b_;
  scoped_refptr<gl::GLContext> context_b_;
  scoped_refptr<SharedContextState> context_state_;
};

TEST_F(ServiceTransferCacheContextTest, PruneOldEntriesMakesContextCurrent) {
  ASSERT_TRUE(context_state_->MakeCurrent(nullptr));

  bool destroyed_with_expected_context = false;
  transfer_cache()->CreateLocalEntry(
      ServiceTransferCache::EntryKey(
          kDecoderId, cc::TransferCacheEntryType::kImage, /*entry_id=*/1u),
      std::make_unique<ContextCheckingTransferCacheEntry>(
          context_state_.get(), context_a_.get(),
          &destroyed_with_expected_context));
  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 1u);

  // Switch the thread's current GL context to `context_b_` before the prune
  // timer fires.
  ASSERT_TRUE(context_b_->MakeCurrent(surface_b_.get()));
  context_state_->set_need_context_state_reset(false);
  EXPECT_FALSE(context_state_->IsCurrent(nullptr));
  EXPECT_EQ(gl::GLContext::GetCurrent(), context_b_.get());

  // Advance mock time so `prune_old_entries_timer_` fires `PruneOldEntries()`.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 0u);
  EXPECT_TRUE(destroyed_with_expected_context);
  EXPECT_TRUE(context_state_->IsCurrent(nullptr));
  EXPECT_TRUE(context_state_->need_context_state_reset());
}

TEST_F(ServiceTransferCacheContextTest, PurgeMemoryMakesContextCurrent) {
  ASSERT_TRUE(context_state_->MakeCurrent(nullptr));

  bool destroyed_with_expected_context = false;
  transfer_cache()->CreateLocalEntry(
      ServiceTransferCache::EntryKey(
          kDecoderId, cc::TransferCacheEntryType::kImage, /*entry_id=*/1u),
      std::make_unique<ContextCheckingTransferCacheEntry>(
          context_state_.get(), context_a_.get(),
          &destroyed_with_expected_context));
  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 1u);

  // Switch the thread's current GL context to `context_b_` before
  // `SharedContextState::PurgeMemory()` is invoked.
  ASSERT_TRUE(context_b_->MakeCurrent(surface_b_.get()));
  context_state_->set_need_context_state_reset(false);
  EXPECT_FALSE(context_state_->IsCurrent(nullptr));

  context_state_->PurgeMemory(base::kCriticalMemoryPressureThreshold);

  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 0u);
  EXPECT_TRUE(destroyed_with_expected_context);
  EXPECT_TRUE(context_state_->IsCurrent(nullptr));
  EXPECT_TRUE(context_state_->need_context_state_reset());
}

TEST_F(ServiceTransferCacheContextTest,
       DeleteEntryAndDeleteAllEntriesMarkNeedContextStateReset) {
  ASSERT_TRUE(context_state_->MakeCurrent(nullptr));

  bool destroyed_1 = false;
  bool destroyed_2 = false;
  const ServiceTransferCache::EntryKey key_1(
      kDecoderId, cc::TransferCacheEntryType::kImage, /*entry_id=*/1u);
  const ServiceTransferCache::EntryKey key_2(
      kDecoderId, cc::TransferCacheEntryType::kImage, /*entry_id=*/2u);

  transfer_cache()->CreateLocalEntry(
      key_1, std::make_unique<ContextCheckingTransferCacheEntry>(
                 context_state_.get(), context_a_.get(), &destroyed_1));
  transfer_cache()->CreateLocalEntry(
      key_2, std::make_unique<ContextCheckingTransferCacheEntry>(
                 context_state_.get(), context_a_.get(), &destroyed_2));
  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 2u);

  context_state_->set_need_context_state_reset(false);
  EXPECT_TRUE(transfer_cache()->DeleteEntry(key_1));
  EXPECT_TRUE(destroyed_1);
  EXPECT_TRUE(context_state_->IsCurrent(nullptr));
  EXPECT_TRUE(context_state_->need_context_state_reset());

  context_state_->set_need_context_state_reset(false);
  transfer_cache()->DeleteAllEntriesForDecoder(kDecoderId);
  EXPECT_TRUE(destroyed_2);
  EXPECT_EQ(transfer_cache()->entries_count_for_testing(), 0u);
  EXPECT_TRUE(context_state_->IsCurrent(nullptr));
  EXPECT_TRUE(context_state_->need_context_state_reset());
}

}  // namespace gpu
