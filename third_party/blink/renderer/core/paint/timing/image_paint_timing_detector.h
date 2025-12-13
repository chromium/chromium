// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_

#include <optional>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class Image;
class PaintTimingCallbackManager;
class StyleImage;
struct DOMPaintTimingInfo;
class SoftNavigationContext;

static constexpr double kMinimumEntropyForLCP = 0.05;

// |ImageRecordsManager| is the manager of all of the images that Largest
// Image Paint cares about. Note that an image does not necessarily correspond
// to a node; it can also be one of the background images attached to a node.
// |ImageRecordsManager| encapsulates the logic of |ImageRecord| handling,
// providing interface for the external world to handle it in the language of
// Node, LayoutObject, etc.
class CORE_EXPORT ImageRecordsManager {
  DISALLOW_NEW();
  friend class ImagePaintTimingDetector;
  friend class ImagePaintTimingDetectorTest;
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);

  void Trace(Visitor* visitor) const;

 private:
  explicit ImageRecordsManager(LocalFrameView*);
  ImageRecordsManager(const ImageRecordsManager&) = delete;
  ImageRecordsManager& operator=(const ImageRecordsManager&) = delete;
  ImageRecord* LargestImage() const;

  inline void RemoveRecord(MediaRecordIdHash record_id_hash) {
    recorded_images_.erase(record_id_hash);
    image_finished_times_.erase(record_id_hash);
    auto it = pending_images_.find(record_id_hash);
    if (it != pending_images_.end()) {
      if (largest_pending_image_ && (largest_pending_image_ == it->value)) {
        largest_pending_image_ = nullptr;
      }
      it->value->OnImageOrTextRemovedWhilePending();
      pending_images_.erase(it);
      // Leave out |images_queued_for_paint_time_| intentionally because the
      // null record can be removed in
      // |AssignPaintTimeToRegisteredQueuedRecords|.
    }
  }

  inline void RecordImage(MediaRecordIdHash record_id_hash) {
    recorded_images_.insert(record_id_hash);
  }

  // Always adds media record to `recorded_images_`, and might create a new
  // ImageRecord to add to `pending_images_`.
  ImageRecord* RecordFirstPaintAndMaybeCreateImageRecord(
      bool is_recording_lcp,
      const MediaRecordId& record_id,
      const uint64_t& visual_size,
      const gfx::Rect& frame_visual_rect,
      const gfx::RectF& root_visual_rect,
      double entropy_for_lcp,
      SoftNavigationContext* soft_navigation_context);

  bool IsRecordedImage(MediaRecordIdHash record_id_hash) const {
    return recorded_images_.Contains(record_id_hash);
  }

  void NotifyImageFinished(MediaRecordIdHash record_id_hash) {
    // TODO(npm): Ideally NotifyImageFinished() would only be called when the
    // record has not yet been inserted in |image_finished_times_| but that's
    // not currently the case. If we plumb some information from
    // MediaTiming we may be able to ensure that this call does not
    // require the Contains() check, which would save time.
    if (!image_finished_times_.Contains(record_id_hash)) {
      image_finished_times_.insert(record_id_hash, base::TimeTicks::Now());
    }
  }

  inline ImageRecord* GetPendingImage(MediaRecordIdHash record_id_hash) {
    auto it = pending_images_.find(record_id_hash);
    return it == pending_images_.end() ? nullptr : it->value.Get();
  }
  bool OnFirstAnimatedFramePainted(MediaRecordIdHash,
                                   uint32_t current_frame_index);
  void OnImageLoaded(MediaRecordIdHash,
                     uint32_t current_frame_index,
                     const StyleImage*);

  // Receives a candidate image painted under opacity 0 but without nested
  // opacity. May update |largest_ignored_image_| if the new candidate has a
  // larger size.
  void MaybeUpdateLargestIgnoredImage(const MediaRecordId&,
                                      uint64_t visual_size,
                                      const gfx::Rect& frame_visual_rect,
                                      const gfx::RectF& root_visual_rect,
                                      double entropy_for_lcp,
                                      bool is_recording_lcp);
  // If `largest_ignored_image_` is non-null and the corresponding node is still
  // attached to the DOM, this marks first image paint (always) and reports the
  // image as an LCP candidate (if `is_recording_lcp` is true). Returns true iff
  // the image record is considered an LCP candidate.
  bool ReportLargestIgnoredImage(uint32_t current_frame_index,
                                 bool is_recording_lcp);

  void AssignPaintTimeToRegisteredQueuedRecords(
      const base::TimeTicks&,
      const DOMPaintTimingInfo&,
      uint32_t last_queued_frame_index,
      bool is_recording_lcp);

  void AddPendingImage(ImageRecord* record, bool is_recording_lcp);
  void ClearImagesQueuedForPaintTime();

  inline void QueueToMeasurePaintTime(ImageRecord* record,
                                      uint32_t current_frame_index) {
    CHECK(record);
    record->SetFrameIndex(current_frame_index);
    images_queued_for_paint_time_.push_back(record);
  }

  ImageRecord* TakeLargestIgnoredImage() {
    return std::exchange(largest_ignored_image_, nullptr);
  }
  const ImageRecord* LargestIgnoredImage() const {
    return largest_ignored_image_;
  }

  void OnImageLoadedInternal(ImageRecord*, uint32_t current_frame_index);

  // The ImageRecord corresponding to the largest image that has been loaded and
  // painted.
  Member<ImageRecord> largest_painted_image_;

  // The ImageRecord corresponding to the largest image that has been loaded,
  // but not necessarily painted.
  Member<ImageRecord> largest_pending_image_;

  // MediaRecordId for images for which we have seen a first paint. A
  // MediaRecordId is added to this set regardless of whether the image could be
  // an LCP candidate.
  HashSet<MediaRecordIdHash> recorded_images_;

  // Map of MediaRecordId to ImageRecord for images for which the first paint
  // has been seen but which do not have the paint time set yet. This may
  // contain only images which are potential LCP candidates.
  HeapHashMap<MediaRecordIdHash, Member<ImageRecord>> pending_images_;

  // |ImageRecord|s waiting for paint time are stored in this map
  // until they get a presentation time.
  HeapDeque<Member<ImageRecord>> images_queued_for_paint_time_;

  // Map containing timestamps of when LayoutObject::ImageNotifyFinished is
  // first called.
  HashMap<MediaRecordIdHash, base::TimeTicks> image_finished_times_;

  Member<LocalFrameView> frame_view_;

  // Image paints are ignored when they (or an ancestor) have opacity 0. This
  // can be a problem later on if the opacity changes to nonzero but this change
  // is composited. We solve this for the special case of documentElement by
  // storing a record for the largest ignored image without nested opacity. We
  // consider this an LCP candidate when the documentElement's opacity changes
  // from zero to nonzero.
  Member<ImageRecord> largest_ignored_image_;
};

// ImagePaintTimingDetector contains Largest Image Paint.
//
// Largest Image Paint timing measures when the largest image element within
// viewport finishes painting. Specifically, it:
// 1. Tracks all images' first invalidation, recording their visual size, if
// this image is within viewport.
// 2. When an image finishes loading, record its paint time.
// 3. At the end of each frame, if new images are added and loaded, the
// algorithm will start an analysis.
//
// In the analysis:
// 3.1 Largest Image Paint finds the largest image by the first visual size. If
// it has finished loading, reports a candidate result as its first paint time
// since loaded.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Image Paint candidate as its final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.1k2rnrs6mdmt
class CORE_EXPORT ImagePaintTimingDetector final
    : public GarbageCollected<ImagePaintTimingDetector> {
 public:
  explicit ImagePaintTimingDetector(LocalFrameView*);
  // Record an image paint. This method covers both img and background image. In
  // the case of a normal img, the last parameter will be nullptr. This
  // parameter is needed only for the purposes of plumbing the correct loadTime
  // value to the ImageRecord. The method returns true if the image is a
  // candidate for LargestContentfulPaint. That is, if the image is larger
  // on screen than the current best candidate.
  bool RecordImage(const LayoutObject&,
                   const gfx::Size& intrinsic_size,
                   const MediaTiming&,
                   const PropertyTreeStateOrAlias& current_paint_properties,
                   const StyleImage*,
                   const gfx::Rect& image_border);
  void NotifyImageFinished(const LayoutObject&, const MediaTiming*);
  void NotifyImageRemoved(const LayoutObject&, const MediaTiming*);
  // After the method being called, the detector stops to recording new entries.
  // We manually clean up the |images_queued_for_paint_time_| since those may be
  // used in the presentation callbacks, and we do not want any new paint times
  // to be assigned after this method is called. Essentially, this class should
  // do nothing after this method is called, and is now just waiting to be
  // GarbageCollected.
  void StopRecordEntries();
  void ResetCallbackManager(PaintTimingCallbackManager* manager) {
    callback_manager_ = manager;
  }

  void ReportPresentationTime(uint32_t last_queued_frame_index,
                              base::TimeTicks);
  std::optional<base::OnceCallback<void(const base::TimeTicks&,
                                        const DOMPaintTimingInfo&)>>
  TakePaintTimingCallback();

  // Return the image LCP candidate and whether the candidate has changed.
  std::pair<ImageRecord*, bool> UpdateMetricsCandidate();

  // Called when documentElement changes from zero to nonzero opacity. Makes the
  // largest image that was hidden due to this a Largest Contentful Paint
  // candidate.
  void ReportLargestIgnoredImage();

  // Called when the "src" attribute changes on a <video> element and the change
  // is attributable to an interaction.
  void NotifyInteractionTriggeredVideoSrcChange(const LayoutObject&);

  bool IsRecordingLargestImagePaint() const {
    return recording_largest_image_paint_;
  }
  void StopRecordingLargestImagePaint() {
    recording_largest_image_paint_ = false;
  }
  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class ImagePaintTimingDetectorTest;
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);

  void RegisterNotifyPresentationTime();
  // Computes the size of an image for the purpose of LargestContentfulPaint,
  // downsizing the size of images with low intrinsic size. Images that occupy
  // the full viewport are special-cased and this method returns 0 for them so
  // that they are not considered valid candidates.
  uint64_t ComputeImageRectSize(const gfx::Rect& image_border,
                                const gfx::RectF& mapped_visual_rect,
                                const gfx::Size&,
                                const PropertyTreeStateOrAlias&,
                                const LayoutObject&,
                                const MediaTiming&);

  // Used to decide which frame a record belongs to, monotonically increasing.
  uint32_t frame_index_ = 1;
  bool added_entry_in_latest_frame_ = false;

  bool contains_full_viewport_image_ = false;

  // We cache the viewport size computation to avoid performing it on every
  // image. This value is reset when paint is finished and is computed if unset
  // when needed. 0 means that the size has not been computed.
  std::optional<uint64_t> viewport_size_;
  // Whether the viewport size used is the page viewport.
  bool uses_page_viewport_;
  // Are we recording an LCP candidate? True after a hard navigation until the
  // next user interaction.
  bool recording_largest_image_paint_ = true;

  ImageRecordsManager records_manager_;
  Member<LocalFrameView> frame_view_;
  Member<PaintTimingCallbackManager> callback_manager_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
