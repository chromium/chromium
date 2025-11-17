// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"

#include <list>

#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using testing::Test;

namespace {

class TestHibernationHandlerDelegate
    : public CanvasHibernationHandler::Delegate {
 public:
  explicit TestHibernationHandlerDelegate(gfx::Size size) : size_(size) {}
  ~TestHibernationHandlerDelegate() override = default;
  bool IsContextLost() const override { return false; }
  void SetNeedsCompositingUpdate() override {}
  bool IsPageVisible() const override { return page_visible_; }
  void SetIsHibernating(bool is_hibernating) {
    is_hibernating_ = is_hibernating;
  }

  CanvasResourceProvider* GetResourceProvider() const override {
    return resource_provider_.get();
  }
  void ResetResourceProvider() override { resource_provider_.reset(); }

  void CreateResourceProvider() {
    CHECK(!GetResourceProvider());
    resource_provider_ = CanvasResourceProvider::CreateSharedImageProvider(
        size_, GetN32FormatForCanvas(), kPremul_SkAlphaType,
        gfx::ColorSpace::CreateSRGB(),
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT);
  }

  void SetPageVisible(bool visible) {
    if (page_visible_ != visible) {
      page_visible_ = visible;
    }
  }

 private:
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  bool page_visible_ = true;
  bool is_hibernating_ = false;
  gfx::Size size_;
};

}  // namespace

class CanvasHibernationHandlerTest
    : public testing::TestWithParam<
          CanvasHibernationHandler::CompressionAlgorithm> {
 public:
  CanvasHibernationHandlerTest() {
    // This only enabled the feature, not necessarily compression using this
    // algorithm, since the current platform may not support it. This is the
    // correct thing to do though, as we care about code behaving well with the
    // two feature states, even on platforms that don't support ZSTD.
    CanvasHibernationHandler::CompressionAlgorithm algorithm = GetParam();
    switch (algorithm) {
      case CanvasHibernationHandler::CompressionAlgorithm::kZlib:
        scoped_feature_list_.InitWithFeatures({features::kCanvas2DHibernation},
                                              {kCanvasHibernationSnapshotZstd});
        break;
      case blink::CanvasHibernationHandler::CompressionAlgorithm::kZstd:
        scoped_feature_list_.InitWithFeatures(
            {features::kCanvas2DHibernation, kCanvasHibernationSnapshotZstd},
            {});
        break;
    }
  }

  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContextRaster(test_context_provider_.get());

    // Make sure that we can count tasks correctly in the tests.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  virtual bool NeedsMockGL() { return false; }

  void TearDown() override {
    SharedGpuContext::Reset();
    test_context_provider_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

void SetPageVisible(
    TestHibernationHandlerDelegate* delegate,
    CanvasHibernationHandler* hibernation_handler,
    ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform>& platform,
    bool page_visible) {
  delegate->SetPageVisible(page_visible);

  // TODO(crbug.com/40280152): Encapsulate the logic for starting/ending
  // hibernation in the test delegate's SetPageVisible() implementation and
  // change the tests to directly call SetPageVisible() on the delegate.
  if (!page_visible) {
    // Trigger hibernation.
    scoped_refptr<StaticBitmapImage> snapshot =
        delegate->GetResourceProvider()->Snapshot();
    hibernation_handler->SaveForHibernation(
        snapshot->PaintImageForCurrentFrame().GetSwSkImage(),
        delegate->GetResourceProvider()->ReleaseRecorder());
    EXPECT_TRUE(hibernation_handler->IsHibernating());
  } else {
    // End hibernation.
    hibernation_handler->Clear();
  }
}

std::map<std::string, uint64_t> GetEntries(
    const base::trace_event::MemoryAllocatorDump& dump) {
  std::map<std::string, uint64_t> result;
  for (const auto& entry : dump.entries()) {
    CHECK(entry.entry_type ==
          base::trace_event::MemoryAllocatorDump::Entry::kUint64);
    result.insert({entry.name, entry.value_uint64});
  }
  return result;
}

void Draw(TestHibernationHandlerDelegate& delegate) {
  if (!delegate.GetResourceProvider()) {
    delegate.CreateResourceProvider();
  }
  CanvasResourceProvider* provider = delegate.GetResourceProvider();
  provider->Canvas().drawLine(0, 0, 2, 2, cc::PaintFlags());
  provider->FlushCanvas(FlushReason::kOther);
}

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    EXPECT_TRUE(delay.is_zero()) << "Only immediate tasks";
    immediate_.push_back(std::move(task));
    return true;
  }
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return false;
  }

  // Since this is mocking a SingleThreadTaskRunner, tasks will always be run
  // in the same sequence they are posted from.
  bool RunsTasksInCurrentSequence() const override { return true; }

  size_t RunAll() {
    size_t count = 0;
    while (!immediate_.empty()) {
      std::move(immediate_.front()).Run();
      immediate_.pop_front();
      count++;
    }
    return count;
  }

  size_t PendingTasksCount() const { return immediate_.size(); }

 private:
  std::list<base::OnceClosure> immediate_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    CanvasHibernationHandlerTest,
    ::testing::Values(CanvasHibernationHandler::CompressionAlgorithm::kZlib,
                      CanvasHibernationHandler::CompressionAlgorithm::kZstd));

TEST_P(CanvasHibernationHandlerTest, SimpleTest) {
  base::HistogramTester histogram_tester;

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  SetPageVisible(&delegate, &handler, platform, false);
  EXPECT_TRUE(handler.IsHibernating());

  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  // Posted the background compression task.
  EXPECT_EQ(1u, task_runner->PendingTasksCount());

  size_t uncompressed_size = 300u * 200 * 4;
  EXPECT_EQ(handler.width(), 300);
  EXPECT_EQ(handler.height(), 200);
  EXPECT_EQ(uncompressed_size, handler.memory_size());

  // Runs the encoding task.
  EXPECT_EQ(1u, task_runner->RunAll());
  // Callback task.
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(handler.is_encoded());
  EXPECT_LT(handler.memory_size(), uncompressed_size);
  EXPECT_EQ(handler.original_memory_size(), uncompressed_size);

  histogram_tester.ExpectTotalCount(
      "Blink.Canvas.2DLayerBridge.Compression.Ratio", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.Canvas.2DLayerBridge.Compression.ThreadTime", 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.Canvas.2DLayerBridge.Compression.SnapshotSizeKb",
      uncompressed_size / 1024, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime", 0);

  // It should be possible to decompress the encoded image.
  EXPECT_TRUE(handler.GetImage());
  histogram_tester.ExpectTotalCount(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime", 1);

  SetPageVisible(&delegate, &handler, platform, true);
  EXPECT_FALSE(handler.is_encoded());

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_TRUE(delegate.GetResourceProvider()->IsValid());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundTooEarly) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);

  Draw(delegate);

  handler.SetBackgroundTaskRunnerForTesting(task_runner);
  SetPageVisible(&delegate, &handler, platform, false);

  EXPECT_TRUE(handler.IsHibernating());
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay / 2);
  SetPageVisible(&delegate, &handler, platform, true);

  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  EXPECT_EQ(0u, task_runner->PendingTasksCount());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, BackgroundForeground) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  // Background -> Foreground -> Background
  SetPageVisible(&delegate, &handler, platform, false);
  SetPageVisible(&delegate, &handler, platform, true);
  SetPageVisible(&delegate, &handler, platform, false);

  EXPECT_EQ(2u, task_environment_.GetPendingMainThreadTaskCount());
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);

  // But a single encoding task.
  EXPECT_EQ(1u, task_runner->RunAll());
  // Main thread callback.
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundAfterEncoding) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  SetPageVisible(&delegate, &handler, platform, false);
  // Wait for the encoding task to be posted.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  EXPECT_EQ(1u, task_runner->RunAll());
  // Come back to foreground after (or during) compression, but before the
  // callback.
  SetPageVisible(&delegate, &handler, platform, true);

  // The callback is still pending.
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
  task_environment_.FastForwardBy(base::TimeDelta());
  // But the encoded version is dropped.
  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundFlipForAfterEncoding) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  SetPageVisible(&delegate, &handler, platform, false);
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  // Run the compresison task.
  EXPECT_EQ(1u, task_runner->RunAll());
  // Come back to foreground after (or during) compression, but before the
  // callback.
  SetPageVisible(&delegate, &handler, platform, true);
  // And back to background.
  SetPageVisible(&delegate, &handler, platform, false);
  EXPECT_TRUE(handler.IsHibernating());

  // The callback is still pending.
  EXPECT_FALSE(handler.is_encoded());
  task_environment_.FastForwardBy(base::TimeDelta());

  // But the encoded version is dropped (epoch mismatch).
  EXPECT_FALSE(handler.is_encoded());
  // Yet we are hibernating (since the page is in the background).
  EXPECT_TRUE(handler.IsHibernating());

  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  task_runner->RunAll();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(handler.is_encoded());
  EXPECT_TRUE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundFlipForBeforeEncoding) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  SetPageVisible(&delegate, &handler, platform, false);
  // Wait for the encoding task to be posted.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);

  // Come back to foreground before compression.
  SetPageVisible(&delegate, &handler, platform, true);
  // And back to background.
  SetPageVisible(&delegate, &handler, platform, false);
  EXPECT_TRUE(handler.IsHibernating());

  // Compression happens, callback is posted and executed.
  EXPECT_EQ(1u, task_runner->RunAll());
  task_environment_.FastForwardBy(base::TimeDelta());

  // But the encoded version is dropped (epoch mismatch).
  EXPECT_FALSE(handler.is_encoded());
  // Yet we are hibernating (since the page is in the background).
  EXPECT_TRUE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ClearEndsHibernation) {
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);

  Draw(delegate);

  SetPageVisible(&delegate, &handler, platform, false);
  // Wait for the canvas to be encoded.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(handler.is_encoded());

  handler.Clear();

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, ClearWhileCompressingEndsHibernation) {
  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  CanvasHibernationHandler handler(delegate);
  handler.SetBackgroundTaskRunnerForTesting(task_runner);

  Draw(delegate);

  // Set the page to hidden to kick off hibernation.
  SetPageVisible(&delegate, &handler, platform, false);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // Run the task that kicks off compression, then run the compression task
  // itself, but *don't* run the callback for compression completing.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  task_runner->RunAll();
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // A clear while compression is in progress should end hibernation.
  handler.Clear();
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // Compression finishing should then be a no-op because the canvas is no
  // longer in hibernation.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, HibernationMemoryMetrics) {
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  TestHibernationHandlerDelegate delegate(gfx::Size(300, 200));
  auto handler = std::make_unique<CanvasHibernationHandler>(delegate);

  Draw(delegate);

  SetPageVisible(&delegate, handler.get(), platform, false);

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    auto* dump = pmd.GetAllocatorDump("canvas/hibernated/canvas_0");
    ASSERT_TRUE(dump);
    auto entries = GetEntries(*dump);
    EXPECT_EQ(entries["memory_size"], handler->memory_size());
    EXPECT_EQ(entries["original_memory_size"], handler->original_memory_size());
    EXPECT_EQ(entries.at("is_encoded"), 0u);
    EXPECT_EQ(entries["height"], 200u);
    EXPECT_EQ(entries["width"], 300u);
  }

  // Wait for the canvas to be encoded.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  EXPECT_TRUE(handler->is_encoded());

  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    auto* dump = pmd.GetAllocatorDump("canvas/hibernated/canvas_0");
    ASSERT_TRUE(dump);
    auto entries = GetEntries(*dump);
    EXPECT_EQ(entries["memory_size"], handler->memory_size());
    EXPECT_EQ(entries["original_memory_size"], handler->original_memory_size());
    EXPECT_LT(entries["memory_size"], entries["original_memory_size"]);
    EXPECT_EQ(entries["is_encoded"], 1u);
  }

  // End hibernation to be able to verify that hibernation dumps will no longer
  // occur.
  SetPageVisible(&delegate, handler.get(), platform, true);
  EXPECT_FALSE(handler->IsHibernating());

  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_FALSE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }

  SetPageVisible(&delegate, handler.get(), platform, false);
  // Wait for the canvas to be encoded.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);

  // We have an hibernated canvas.
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_TRUE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }

  // Handler gets destroyed, no more hibernated canvas.
  handler = nullptr;
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_FALSE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }
}

}  // namespace blink
