// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_

#include <optional>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
struct DOMPaintTimingInfo;
class LargestContentfulPaintManager;
class LayoutObject;
class MediaTiming;
class PaintTimingDetector;
class PropertyTreeStateOrAlias;
class StyleImage;

// `ImagePaintTimingDetector` tracks contentful paints of image contents within
// the viewport and provides timing information to clients, implementing the
// image portion of Paint Timing (https://w3c.github.io/paint-timing/). This
// supports Largest Contentful Paint (LCP) and Interaction Contentful Paint
// (ICP).
//
// For image tracking, there are three phases:
//   1. When an image is first painted with a known size, an `ImageRecord` is
//   created. Paint tracking is initialized in this phase, or the image is
//   ignored if it is not needed by any clients.
//
//   2. When an image finishes loading** and is painted again, the `ImageRecord`
//   is queued for presentation time, meaning its paint and presentation times
//   will be set when `PaintTiming` receives the presentation feedback for the
//   current frame.
//
//   3. When presentation feedback is received, the paint and presentation times
//   are set for the relevant `ImageRecord`s and `PaintTiming` clients process
//   the list of presented images, e.g. emitting LCP or ICP candidates and
//   updating metrics.
//
// **Animated images currently have a two-phase completion: metrics are updated
// when the first animated frame is complete, but LCP and ICP candidates are not
// emitted until the image finishes loading. This will be simplified when
// ReportFirstFrameTimeAsRenderTime ships.  See crbug.com/449779010.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.1k2rnrs6mdmt
class CORE_EXPORT ImagePaintTimingDetector final
    : public GarbageCollected<ImagePaintTimingDetector> {
 public:
  explicit ImagePaintTimingDetector(PaintTimingDetector*);

  // Records an image paint for <img> tags, background images, <video> poster
  // images, and first video frames. The `StyleImage` will be nullptr unless
  // there is a background image. Returns true if the image is a candidate for
  // Largest Contentful Paint, i.e. if the image is larger on screen than the
  // current LCP candidate.
  bool RecordImage(const LayoutObject&,
                   const gfx::Size& intrinsic_size,
                   const MediaTiming&,
                   const PropertyTreeStateOrAlias& current_paint_properties,
                   const StyleImage*,
                   const gfx::Rect& image_border);

  // Notifies the detector that an image has finished loading. Sets the
  // image-finished time if it's not already set.
  void NotifyImageFinished(const LayoutObject&, const MediaTiming*);

  // Notifies the detector that an image was removed. Removes the image data
  // from the relevant collections and notifies clients of the removal if the
  // image is pending.
  void NotifyImageRemoved(const LayoutObject&, const MediaTiming*);

  // Clears `images_queued_for_paint_time_` when hard LCP is stopped due to
  // input.
  //
  // TODO(crbug.com/503691215, crbug.com/454082773): This should be removed and
  // the decision should be left up to individual clients.
  void StopRecordEntries();

  OptionalPaintTimingDetectorCallback<ImageRecord> TakePaintTimingCallback();

  // Called when documentElement changes from zero to nonzero opacity. Makes the
  // largest image that was hidden due to this a Largest Contentful Paint
  // candidate.
  void ReportLargestIgnoredImage();

  // Called when the "src" attribute changes on a <video> element and the change
  // is attributable to an interaction.
  void NotifyInteractionTriggeredVideoSrcChange(const LayoutObject&);

  void Trace(Visitor*) const;

 private:
  friend class ImagePaintTimingDetectorTest;
  friend class LargestContentfulPaintCalculatorTest;

  void SendRectsToHud();

  // Returns the viewport size, initializing the cached `viewport_size_` if
  // needed.
  uint64_t ViewportSize();

  LargestContentfulPaintManager* GetLargestContentfulPaintManager() const;

  // Removes the image data associated with the `MediaRecordIdHash` from all
  // collections.
  ImageRecord* RemoveRecord(MediaRecordIdHash);

  inline ImageRecord* GetPendingImage(MediaRecordIdHash record_id_hash) {
    auto it = pending_images_.find(record_id_hash);
    return it == pending_images_.end() ? nullptr : it->value.Get();
  }

  void OnFirstAnimatedFramePainted(MediaRecordIdHash);

  void OnImageLoaded(ImageRecord*, const StyleImage*);

  void AssignPaintTimeToRegisteredQueuedRecords(
      uint32_t last_queued_frame_index,
      const base::TimeTicks&,
      const DOMPaintTimingInfo&,
      HeapVector<Member<ImageRecord>>& settled_records);

  inline void QueueToMeasurePaintTime(ImageRecord* record) {
    CHECK(record);
    record->SetFrameIndex(frame_index_);
    images_queued_for_paint_time_.push_back(record);
    added_entry_in_latest_frame_ = true;
  }

  // Used to decide which frame a record belongs to, monotonically increasing.
  uint32_t frame_index_ = 1;
  bool added_entry_in_latest_frame_ = false;

  // We cache the viewport size computation to avoid performing it on every
  // image. This value is reset when paint is finished and is computed if unset
  // when needed. 0 means that the size has not been computed.
  std::optional<uint64_t> viewport_size_;

  Member<PaintTimingDetector> paint_timing_detector_;

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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
