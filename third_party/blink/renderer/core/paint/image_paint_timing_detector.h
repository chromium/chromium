// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class TracedValue;
class Image;

// TODO(crbug/960502): we should limit the access of these properties.
class ImageRecord : public base::SupportsWeakPtr<ImageRecord> {
 public:
  ImageRecord(DOMNodeId new_node_id,
              const ImageResourceContent* new_cached_image,
              uint64_t new_first_size,
              const IntRect& frame_visual_rect,
              const FloatRect& root_visual_rect)
      : node_id(new_node_id),
        cached_image(new_cached_image),
        first_size(new_first_size) {
    static unsigned next_insertion_index_ = 1;
    insertion_index = next_insertion_index_++;
    if (PaintTimingVisualizer::IsTracingEnabled()) {
      lcp_rect_info_ = std::make_unique<LCPRectInfo>(
          frame_visual_rect, RoundedIntRect(root_visual_rect));
    }
  }

  ImageRecord() {}

  DOMNodeId node_id = kInvalidDOMNodeId;
  WeakPersistent<const ImageResourceContent> cached_image;
  // Mind that |first_size| has to be assigned before pusing to
  // |size_ordered_set_| since it's the sorting key.
  uint64_t first_size = 0;
  unsigned frame_index = 0;
  unsigned insertion_index;
  // The time of the first paint after fully loaded. 0 means not painted yet.
  base::TimeTicks paint_time = base::TimeTicks();
  base::TimeTicks load_time = base::TimeTicks();
  bool loaded = false;
  // LCP rect information, only populated when tracing is enabled.
  std::unique_ptr<LCPRectInfo> lcp_rect_info_;
};

typedef std::pair<const LayoutObject*, const ImageResourceContent*> RecordId;

// |ImageRecordsManager| is the manager of all of the images that Largest Image
// Paint cares about. Note that an image does not necessarily correspond to a
// node; it can also be one of the background images attached to a node.
// |ImageRecordsManager| encapsulates the logic of |ImageRecord| handling,
// providing interface for the external world to handle it in the language of
// Node, LayoutObject, etc.
class CORE_EXPORT ImageRecordsManager {
  friend class ImagePaintTimingDetectorTest;
  DISALLOW_NEW();

  using NodesQueueComparator = bool (*)(const base::WeakPtr<ImageRecord>&,
                                        const base::WeakPtr<ImageRecord>&);
  using ImageRecordSet =
      std::set<base::WeakPtr<ImageRecord>, NodesQueueComparator>;

 public:
  explicit ImageRecordsManager(LocalFrameView*);
  ImageRecordsManager(const ImageRecordsManager&) = delete;
  ImageRecordsManager& operator=(const ImageRecordsManager&) = delete;
  ImageRecord* FindLargestPaintCandidate() const;

  inline void RemoveInvisibleRecordIfNeeded(const RecordId& record_id) {
    invisible_images_.erase(record_id);
  }

  inline void RemoveImageFinishedRecord(const RecordId& record_id) {
    image_finished_times_.erase(record_id);
  }

  inline void RemoveVisibleRecord(const RecordId& record_id) {
    base::WeakPtr<ImageRecord> record =
        visible_images_.find(record_id)->value->AsWeakPtr();
    if (!record->paint_time.is_null()) {
      DCHECK_GT(record->first_size, 0u);
      if (record->first_size > largest_removed_image_size_) {
        largest_removed_image_size_ = record->first_size;
        largest_removed_image_paint_time_ = record->paint_time;
      } else if (record->first_size == largest_removed_image_size_) {
        // Ensure we use the lower timestamp in the case of a tie.
        DCHECK(!largest_removed_image_paint_time_.is_null());
        largest_removed_image_paint_time_ =
            std::min(largest_removed_image_paint_time_, record->paint_time);
      }
    }
    size_ordered_set_.erase(record);
    visible_images_.erase(record_id);
    // Leave out |images_queued_for_paint_time_| intentionally because the null
    // record will be removed in |AssignPaintTimeToRegisteredQueuedRecords|.
  }

  inline void RecordInvisible(const RecordId& record_id) {
    invisible_images_.insert(record_id);
  }
  void RecordVisible(const RecordId& record_id,
                     const uint64_t& visual_size,
                     const IntRect& frame_visual_rect,
                     const FloatRect& root_visual_rect);
  bool IsRecordedVisibleImage(const RecordId& record_id) const {
    return visible_images_.Contains(record_id);
  }
  bool IsRecordedInvisibleImage(const RecordId& record_id) const {
    return invisible_images_.Contains(record_id);
  }

  void NotifyImageFinished(const RecordId& record_id) {
    // TODO(npm): Ideally NotifyImageFinished() would only be called when the
    // record has not yet been inserted in |image_finished_times_| but that's
    // not currently the case. If we plumb some information from
    // ImageResourceContent we may be able to ensure that this call does not
    // require the Contains() check, which would save time.
    if (!image_finished_times_.Contains(record_id))
      image_finished_times_.insert(record_id, base::TimeTicks::Now());
  }

  inline bool IsVisibleImageLoaded(const RecordId& record_id) const {
    DCHECK(visible_images_.Contains(record_id));
    return visible_images_.at(record_id)->loaded;
  }
  void OnImageLoaded(const RecordId&,
                     unsigned current_frame_index,
                     const StyleFetchedImage*);
  void OnImageLoadedInternal(base::WeakPtr<ImageRecord>&,
                             unsigned current_frame_index);

  // Receives a candidate image painted under opacity 0 but without nested
  // opacity. May update |largest_ignored_image_| if the new candidate has a
  // larger size.
  void MaybeUpdateLargestIgnoredImage(const RecordId&,
                                      const uint64_t& visual_size,
                                      const IntRect& frame_visual_rect,
                                      const FloatRect& root_visual_rect);
  void ReportLargestIgnoredImage(unsigned current_frame_index);

  // Compare the last frame index in queue with the last frame index that has
  // registered for assigning paint time.
  inline bool HasUnregisteredRecordsInQueue(
      unsigned last_registered_frame_index) {
    while (!images_queued_for_paint_time_.IsEmpty() &&
           !images_queued_for_paint_time_.back()) {
      images_queued_for_paint_time_.pop_back();
    }
    if (images_queued_for_paint_time_.IsEmpty())
      return false;
    return last_registered_frame_index < LastQueuedFrameIndex();
  }
  void AssignPaintTimeToRegisteredQueuedRecords(
      const base::TimeTicks&,
      unsigned last_queued_frame_index);
  inline unsigned LastQueuedFrameIndex() const {
    DCHECK(images_queued_for_paint_time_.back());
    return images_queued_for_paint_time_.back()->frame_index;
  }

  uint64_t LargestRemovedImageSize() const {
    return largest_removed_image_size_;
  }
  base::TimeTicks LargestRemovedImagePaintTime() const {
    return largest_removed_image_paint_time_;
  }

  void Trace(Visitor* visitor) const;

 private:
  // Find the image record of an visible image.
  inline base::WeakPtr<ImageRecord> FindVisibleRecord(
      const RecordId& record_id) const {
    DCHECK(visible_images_.Contains(record_id));
    return visible_images_.find(record_id)->value->AsWeakPtr();
  }
  std::unique_ptr<ImageRecord> CreateImageRecord(
      const LayoutObject& object,
      const ImageResourceContent* cached_image,
      const uint64_t& visual_size,
      const IntRect& frame_visual_rect,
      const FloatRect& root_visual_rect);
  inline void QueueToMeasurePaintTime(base::WeakPtr<ImageRecord>& record,
                                      unsigned current_frame_index) {
    images_queued_for_paint_time_.push_back(record);
    record->frame_index = current_frame_index;
  }
  inline void SetLoaded(base::WeakPtr<ImageRecord>& record) {
    record->loaded = true;
  }

  HashMap<RecordId, std::unique_ptr<ImageRecord>> visible_images_;
  HashSet<RecordId> invisible_images_;

  // This stores the image records, which are ordered by size.
  ImageRecordSet size_ordered_set_;
  // |ImageRecord|s waiting for paint time are stored in this queue
  // until they get a presentation time.
  Deque<base::WeakPtr<ImageRecord>> images_queued_for_paint_time_;
  // Map containing timestamps of when LayoutObject::ImageNotifyFinished is
  // first called.
  HashMap<RecordId, base::TimeTicks> image_finished_times_;

  Member<LocalFrameView> frame_view_;

  // We store the size and paint time of the largest removed image in order to
  // compute experimental LCP correctly.
  uint64_t largest_removed_image_size_ = 0u;
  base::TimeTicks largest_removed_image_paint_time_;

  // Image paints are ignored when they (or an ancestor) have opacity 0. This
  // can be a problem later on if the opacity changes to nonzero but this change
  // is composited. We solve this for the special case of documentElement by
  // storing a record for the largest ignored image without nested opacity. We
  // consider this an LCP candidate when the documentElement's opacity changes
  // from zero to nonzero.
  std::unique_ptr<ImageRecord> largest_ignored_image_;
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
  friend class ImagePaintTimingDetectorTest;

 public:
  ImagePaintTimingDetector(LocalFrameView*, PaintTimingCallbackManager*);
  // Record an image paint. This method covers both img and background image. In
  // the case of a normal img, the last parameter will be nullptr. This
  // parameter is needed only for the purposes of plumbing the correct loadTime
  // value to the ImageRecord.
  void RecordImage(const LayoutObject&,
                   const IntSize& intrinsic_size,
                   const ImageResourceContent&,
                   const PropertyTreeStateOrAlias& current_paint_properties,
                   const StyleFetchedImage*,
                   const IntRect& image_border);
  void NotifyImageFinished(const LayoutObject&, const ImageResourceContent*);
  void OnPaintFinished();
  void NotifyImageRemoved(const LayoutObject&, const ImageResourceContent*);
  // After the method being called, the detector stops to record new entries and
  // node removal. But it still observe the loading status. In other words, if
  // an image is recorded before stopping recording, and finish loading after
  // stopping recording, the detector can still observe the loading being
  // finished.
  void StopRecordEntries();
  inline bool IsRecording() const { return is_recording_; }
  inline bool FinishedReportingImages() const {
    return !is_recording_ && num_pending_presentation_callbacks_ == 0;
  }
  void ResetCallbackManager(PaintTimingCallbackManager* manager) {
    callback_manager_ = manager;
  }
  void ReportPresentationTime(unsigned last_queued_frame_index,
                              base::TimeTicks);

  // Return the candidate.
  ImageRecord* UpdateCandidate();

  // Called when documentElement changes from zero to nonzero opacity. Makes the
  // largest image that was hidden due to this a Largest Contentful Paint
  // candidate.
  void ReportLargestIgnoredImage();

  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;

  void PopulateTraceValue(TracedValue&, const ImageRecord& first_image_paint);
  void RegisterNotifyPresentationTime();
  void ReportCandidateToTrace(ImageRecord&);
  void ReportNoCandidateToTrace();
  // Computes the size of an image for the purpose of LargestContentfulPaint,
  // downsizing the size of images with low intrinsic size. Images that occupy
  // the full viewport are special-cased and this method returns 0 for them so
  // that they are not considered valid candidates.
  uint64_t ComputeImageRectSize(const IntRect& image_border,
                                const FloatRect& mapped_visual_rect,
                                const IntSize&,
                                const PropertyTreeStateOrAlias&,
                                const LayoutObject&,
                                const ImageResourceContent&);

  // Used to find the last candidate.
  unsigned count_candidates_ = 0;

  // Used to decide which frame a record belongs to, monotonically increasing.
  unsigned frame_index_ = 1;
  unsigned last_registered_frame_index_ = 0;

  // Used to control if we record new image entries and image removal, but has
  // no effect on recording the loading status.
  bool is_recording_ = true;

  // Used to determine how many presentation callbacks are pending. In
  // combination with |is_recording|, helps determine whether this detector can
  // be destroyed.
  int num_pending_presentation_callbacks_ = 0;

  // This need to be set whenever changes that can affect the output of
  // |FindLargestPaintCandidate| occur during the paint tree walk.
  bool need_update_timing_at_frame_end_ = false;

  bool contains_full_viewport_image_ = false;

  // We cache the viewport size computation to avoid performing it on every
  // image. This value is reset when paint is finished and is computed if unset
  // when needed. 0 means that the size has not been computed.
  absl::optional<uint64_t> viewport_size_;
  // Whether the viewport size used is the page viewport.
  bool uses_page_viewport_;

  ImageRecordsManager records_manager_;
  Member<LocalFrameView> frame_view_;
  Member<PaintTimingCallbackManager> callback_manager_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
