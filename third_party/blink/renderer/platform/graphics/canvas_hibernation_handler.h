// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIBERNATION_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIBERNATION_HANDLER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class CanvasResourceProvider;

inline constexpr char kCanvasHibernationEventHistogramName[] =
    "Blink.Canvas.HibernationEvents2";

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kCanvasHibernationSnapshotZstd);

// All the fields are main-thread only. See DCheckInvariant() for invariants.
class PLATFORM_EXPORT CanvasHibernationHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual CanvasResourceProvider* GetResourceProvider() const = 0;
    virtual bool IsPageVisible() const = 0;
    virtual bool IsContextLost() const = 0;
    virtual void ResetResourceProvider() = 0;
    virtual void SetNeedsCompositingUpdate() = 0;
    virtual void ClearCanvas2DLayerTexture() {}
  };

  // The values of the enum entries must not change because they are used for
  // usage metrics histograms. New values can be added to the end.
  // LINT.IfChange(CanvasHibernationEvent)
  enum HibernationEvent {
    kHibernationScheduled = 0,
    kHibernationAbortedDueToDestructionWhileHibernatePending = 1,
    // kHibernationAbortedDueToPendingDestruction = 2, (obsolete)
    kHibernationAbortedDueToVisibilityChange = 3,
    kHibernationAbortedDueGpuContextLoss = 4,
    kHibernationAbortedDueToSwitchToUnacceleratedRendering = 5,
    // kHibernationAbortedDueToAllocationFailure = 6, (obsolete)
    kHibernationAbortedDueSnapshotFailure = 7,
    kHibernationEndedNormally = 8,
    kHibernationEndedWithSwitchToBackgroundRendering = 9,
    kHibernationEndedWithFallbackToSW = 10,
    kHibernationEndedWithTeardown = 11,
    kHibernationAbortedBecauseNoSurface = 12,
    kHibernationEndedOnReset = 13,
    kMaxValue = kHibernationEndedOnReset,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:CanvasHibernationEvent)

  static void ReportHibernationEvent(
      CanvasHibernationHandler::HibernationEvent event) {
    UMA_HISTOGRAM_ENUMERATION(kCanvasHibernationEventHistogramName, event);
  }

  explicit CanvasHibernationHandler(Delegate& delegate);
  CanvasHibernationHandler(const CanvasHibernationHandler&) = delete;
  CanvasHibernationHandler& operator=(const CanvasHibernationHandler&) = delete;

  ~CanvasHibernationHandler();
  // Semi-arbitrary threshold. Some past experiments (e.g. tile discard) have
  // shown that taking action after 5 minutes has a positive impact on memory,
  // and a minimal impact on tab switching latency (and on needless
  // compression).
  static constexpr base::TimeDelta kBeforeCompressionDelay = base::Minutes(5);

  void InitiateHibernationIfNecessary();

  void SaveForHibernation(sk_sp<SkImage>&& image,
                          std::unique_ptr<MemoryManagedPaintRecorder> recorder);
  // Returns the uncompressed image for this hibernation image. Does not
  // invalidate the hibernated image. Must call `Clear()` if invalidation is
  // required.
  sk_sp<SkImage> GetImage();
  std::unique_ptr<MemoryManagedPaintRecorder> ReleaseRecorder() {
    return std::move(recorder_);
  }
  // Invalidate the hibernated image.
  void Clear();

  bool IsHibernating() const {
    DCheckInvariant();
    return image_ != nullptr || encoded_ != nullptr;
  }
  bool is_encoded() const {
    DCheckInvariant();
    return encoded_ != nullptr;
  }
  size_t memory_size() const;
  size_t original_memory_size() const;
  int width() const { return width_; }
  int height() const { return height_; }

  void SetBackgroundTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    background_thread_task_runner_for_testing_ = task_runner;
  }

  enum class CompressionAlgorithm { kZlib, kZstd };

 private:
  struct BackgroundTaskParams final {
    BackgroundTaskParams(
        sk_sp<SkImage> image,
        uint64_t epoch,
        CompressionAlgorithm algorithm,
        base::WeakPtr<CanvasHibernationHandler> weak_instance,
        scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner)
        : image(image),
          epoch(epoch),
          algorithm(algorithm),
          weak_instance(weak_instance),
          reply_task_runner(reply_task_runner) {}

    BackgroundTaskParams(const BackgroundTaskParams&) = delete;
    BackgroundTaskParams& operator=(const BackgroundTaskParams&) = delete;
    ~BackgroundTaskParams() { DCHECK(IsMainThread()); }

    const sk_sp<SkImage> image;
    const uint64_t epoch;
    const CompressionAlgorithm algorithm;
    const base::WeakPtr<CanvasHibernationHandler> weak_instance;
    const scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner;
  };

  void DCheckInvariant() const {
    DCHECK(IsMainThread());
    DCHECK(!((image_ != nullptr) && (encoded_ != nullptr)));
  }
  void OnAfterHibernation(uint64_t initial_epoch);
  static void Encode(std::unique_ptr<BackgroundTaskParams> params);
  void OnEncoded(
      std::unique_ptr<CanvasHibernationHandler::BackgroundTaskParams> params,
      sk_sp<SkData> encoded);
  scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner() const;
  static size_t ImageMemorySize(const SkImage& image);
  static void HibernateOrLogFailure(
      base::WeakPtr<CanvasHibernationHandler> handler,
      base::TimeTicks /*idleDeadline*/);
  void Hibernate();

  // Incremented each time the canvas is hibernated.
  uint64_t epoch_ = 0;
  // Uncompressed hibernation image.
  sk_sp<SkImage> image_ = nullptr;
  // Compressed hibernation image.
  sk_sp<SkData> encoded_ = nullptr;
  CompressionAlgorithm algorithm_ = CompressionAlgorithm::kZlib;
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
  scoped_refptr<base::SingleThreadTaskRunner>
      background_thread_task_runner_for_testing_;
  int width_;
  int height_;
  int bytes_per_pixel_;

  bool hibernation_scheduled_ = false;
  const base::raw_ref<Delegate> delegate_;
  base::WeakPtrFactory<CanvasHibernationHandler> weak_ptr_factory_{this};
};

// memory-infra metrics for all hibernated canvases in this process. Main thread
// only.
class PLATFORM_EXPORT HibernatedCanvasMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static HibernatedCanvasMemoryDumpProvider& GetInstance();
  void Register(CanvasHibernationHandler* handler);
  void Unregister(CanvasHibernationHandler* handler);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class base::NoDestructor<HibernatedCanvasMemoryDumpProvider>;
  HibernatedCanvasMemoryDumpProvider();

  base::Lock lock_;
  HashSet<CanvasHibernationHandler*> handlers_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIBERNATION_HANDLER_H_
