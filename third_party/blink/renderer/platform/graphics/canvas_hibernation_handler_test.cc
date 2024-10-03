// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"

#include <list>

#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using testing::Test;

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
        scoped_feature_list_.InitWithFeatures({},
                                              {kCanvasHibernationSnapshotZstd});
        break;
      case blink::CanvasHibernationHandler::CompressionAlgorithm::kZstd:
        scoped_feature_list_.InitWithFeatures({kCanvasHibernationSnapshotZstd},
                                              {});
        break;
    }
  }

  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(const gfx::Size& size,
                                                  RasterModeHint raster_mode,
                                                  OpacityMode opacity_mode) {
    host_ = std::make_unique<FakeCanvasResourceHost>(size);
    host_->SetPreferred2DRasterMode(raster_mode);
    host_->SetOpacityMode(opacity_mode);
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        std::make_unique<Canvas2DLayerBridge>(host_.get());
    return bridge;
  }

  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContextGLES2(test_context_provider_.get());
  }

  virtual bool NeedsMockGL() { return false; }

  void TearDown() override {
    SharedGpuContext::Reset();
    test_context_provider_.reset();
  }

  FakeCanvasResourceHost* Host() {
    DCHECK(host_);
    return host_.get();
  }

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<FakeCanvasResourceHost> host_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

void SetPageVisible(
    FakeCanvasResourceHost* host,
    Canvas2DLayerBridge* bridge,
    ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform>& platform,
    bool page_visible) {
  host->SetPageVisible(page_visible);

  // TODO(crbug.com/40280152): Make a custom FakeCanvasResourceHost subclass
  // that encapsulates the logic for starting/ending hibernation in its
  // SetPageVisible() implementation and change the tests to directly call
  // SetPageVisible() on the host.
  CanvasHibernationHandler& hibernation_handler =
      bridge->GetHibernationHandler();
  if (!page_visible) {
    // Trigger hibernation.
    scoped_refptr<StaticBitmapImage> snapshot =
        host->ResourceProvider()->Snapshot(FlushReason::kHibernating);
    hibernation_handler.SaveForHibernation(
        snapshot->PaintImageForCurrentFrame().GetSwSkImage(),
        host->ResourceProvider()->ReleaseRecorder());
    EXPECT_TRUE(hibernation_handler.IsHibernating());
  } else {
    // End hibernation.
    hibernation_handler.Clear();
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

void DrawSomething(Canvas2DLayerBridge* bridge) {
  CanvasResourceProvider* provider = bridge->GetOrCreateResourceProvider();
  provider->Canvas().drawLine(0, 0, 2, 2, cc::PaintFlags());
  provider->FlushCanvas(FlushReason::kTesting);
}

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    if (delay.is_zero()) {
      immediate_.push_back(std::move(task));
    } else {
      delayed_.push_back(std::move(task));
    }

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

  static size_t RunAll(std::list<base::OnceClosure>& tasks) {
    size_t count = 0;
    while (!tasks.empty()) {
      std::move(tasks.front()).Run();
      tasks.pop_front();
      count++;
    }
    return count;
  }

  static bool RunOne(std::list<base::OnceClosure>& tasks) {
    if (tasks.empty()) {
      return false;
    }
    std::move(tasks.front()).Run();
    tasks.pop_front();
    return true;
  }

  std::list<base::OnceClosure>& delayed() { return delayed_; }
  std::list<base::OnceClosure>& immediate() { return immediate_; }

 private:
  std::list<base::OnceClosure> delayed_;
  std::list<base::OnceClosure> immediate_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    CompressionAlgorithm,
    CanvasHibernationHandlerTest,
    ::testing::Values(CanvasHibernationHandler::CompressionAlgorithm::kZlib,
                      CanvasHibernationHandler::CompressionAlgorithm::kZstd));

TEST_P(CanvasHibernationHandlerTest, SimpleTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});
  base::HistogramTester histogram_tester;

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 200), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);

  EXPECT_TRUE(handler.IsHibernating());
  // Triggers a delayed task for encoding.
  EXPECT_FALSE(task_runner->delayed().empty());
  EXPECT_TRUE(task_runner->immediate().empty());

  TestSingleThreadTaskRunner::RunAll(task_runner->delayed());
  // Posted the background compression task.
  EXPECT_FALSE(task_runner->immediate().empty());

  size_t uncompressed_size = 300u * 200 * 4;
  EXPECT_EQ(handler.width(), 300);
  EXPECT_EQ(handler.height(), 200);
  EXPECT_EQ(uncompressed_size, handler.memory_size());

  // Runs the encoding task, but also the callback one.
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
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
  EXPECT_TRUE(bridge->GetHibernationHandler().GetImage());
  histogram_tester.ExpectTotalCount(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime", 1);

  SetPageVisible(Host(), bridge.get(), platform, true);
  EXPECT_FALSE(handler.is_encoded());

  EXPECT_TRUE(Host()->GetRasterMode() == RasterMode::kGPU);
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_TRUE(Host()->IsResourceValid());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundTooEarly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);
  SetPageVisible(Host(), bridge.get(), platform, false);

  // Triggers a delayed task for encoding.
  EXPECT_FALSE(task_runner->delayed().empty());

  EXPECT_TRUE(handler.IsHibernating());
  SetPageVisible(Host(), bridge.get(), platform, true);

  // Nothing happens, because the page came to foreground in-between.
  TestSingleThreadTaskRunner::RunAll(task_runner->delayed());
  EXPECT_TRUE(task_runner->immediate().empty());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, BackgroundForeground) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  // Background -> Foreground -> Background
  SetPageVisible(Host(), bridge.get(), platform, false);
  SetPageVisible(Host(), bridge.get(), platform, true);
  SetPageVisible(Host(), bridge.get(), platform, false);

  // 2 delayed task that will potentially trigger encoding.
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  // But a single encoding task (plus the main thread callback).
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  EXPECT_TRUE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundAfterEncoding) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);
  // Wait for the encoding task to be posted.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_TRUE(TestSingleThreadTaskRunner::RunOne(task_runner->immediate()));
  // Come back to foreground after (or during) compression, but before the
  // callback.
  SetPageVisible(Host(), bridge.get(), platform, true);

  // The callback is still pending.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  // But the encoded version is dropped.
  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundFlipForAfterEncoding) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);
  // Wait for the encoding task to be posted.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_TRUE(TestSingleThreadTaskRunner::RunOne(task_runner->immediate()));
  // Come back to foreground after (or during) compression, but before the
  // callback.
  SetPageVisible(Host(), bridge.get(), platform, true);
  // And back to background.
  SetPageVisible(Host(), bridge.get(), platform, false);
  EXPECT_TRUE(handler.IsHibernating());

  // The callback is still pending.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  // But the encoded version is dropped (epoch mismatch).
  EXPECT_FALSE(handler.is_encoded());
  // Yet we are hibernating (since the bridge is in background).
  EXPECT_TRUE(handler.IsHibernating());

  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  EXPECT_TRUE(handler.is_encoded());
  // Yet we are hibernating (since the bridge is in background).
  EXPECT_TRUE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ForegroundFlipForBeforeEncoding) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);
  // Wait for the encoding task to be posted.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  // Come back to foreground before compression.
  SetPageVisible(Host(), bridge.get(), platform, true);
  // And back to background.
  SetPageVisible(Host(), bridge.get(), platform, false);
  EXPECT_TRUE(handler.IsHibernating());
  // Compression still happens, since it's a static task, doesn't look at the
  // epoch before compressing.
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));

  // But the encoded version is dropped (epoch mismatch).
  EXPECT_FALSE(handler.is_encoded());
  // Yet we are hibernating (since the bridge is in background).
  EXPECT_TRUE(handler.IsHibernating());
}

TEST_P(CanvasHibernationHandlerTest, ClearEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);
  // Wait for the canvas to be encoded.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(handler.is_encoded());

  handler.Clear();

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, ClearWhileCompressingEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 300), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  // Set the page to hidden to kick off hibernation.
  SetPageVisible(Host(), bridge.get(), platform, false);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // Run the task that kicks off compression, then run the compression task
  // itself, but *don't* run the callback for compression completing.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_TRUE(TestSingleThreadTaskRunner::RunOne(task_runner->immediate()));
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // A clear while compression is in progress should end hibernation.
  handler.Clear();
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());

  // Compression finishing should then be a no-op because the canvas is no
  // longer in hibernation.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasHibernationHandlerTest, HibernationMemoryMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  auto task_runner = base::MakeRefCounted<TestSingleThreadTaskRunner>();
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 200), RasterModeHint::kPreferGPU, kNonOpaque);
  DrawSomething(bridge.get());

  auto& handler = bridge->GetHibernationHandler();
  handler.SetTaskRunnersForTesting(task_runner, task_runner);

  SetPageVisible(Host(), bridge.get(), platform, false);

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    auto* dump = pmd.GetAllocatorDump("canvas/hibernated/canvas_0");
    ASSERT_TRUE(dump);
    auto entries = GetEntries(*dump);
    EXPECT_EQ(entries["memory_size"], handler.memory_size());
    EXPECT_EQ(entries["original_memory_size"], handler.original_memory_size());
    EXPECT_EQ(entries.at("is_encoded"), 0u);
    EXPECT_EQ(entries["height"], 200u);
    EXPECT_EQ(entries["width"], 300u);
  }

  // Wait for the canvas to be encoded.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));
  EXPECT_TRUE(handler.is_encoded());

  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    auto* dump = pmd.GetAllocatorDump("canvas/hibernated/canvas_0");
    ASSERT_TRUE(dump);
    auto entries = GetEntries(*dump);
    EXPECT_EQ(entries["memory_size"], handler.memory_size());
    EXPECT_EQ(entries["original_memory_size"], handler.original_memory_size());
    EXPECT_LT(entries["memory_size"], entries["original_memory_size"]);
    EXPECT_EQ(entries["is_encoded"], 1u);
  }

  // End hibernation to be able to verify that hibernation dumps will no longer
  // occur.
  SetPageVisible(Host(), bridge.get(), platform, true);
  EXPECT_FALSE(handler.IsHibernating());

  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_FALSE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }

  SetPageVisible(Host(), bridge.get(), platform, false);
  // Wait for the canvas to be encoded.
  EXPECT_EQ(1u, TestSingleThreadTaskRunner::RunAll(task_runner->delayed()));
  EXPECT_EQ(2u, TestSingleThreadTaskRunner::RunAll(task_runner->immediate()));

  // We have an hibernated canvas.
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_TRUE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }

  // Bridge gets destroyed, no more hibernated canvas.
  bridge = nullptr;
  {
    base::trace_event::ProcessMemoryDump pmd(args);
    EXPECT_TRUE(HibernatedCanvasMemoryDumpProvider::GetInstance().OnMemoryDump(
        args, &pmd));
    // No more dump, since the canvas is no longer hibernating.
    EXPECT_FALSE(pmd.GetAllocatorDump("canvas/hibernated/canvas_0"));
  }
}

}  // namespace blink
