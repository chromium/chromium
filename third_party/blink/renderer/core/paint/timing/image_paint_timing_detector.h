// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class TracedValue;
class Image;

// TODO(crbug/960502): we should limit the access of these properties.
// TODO(yoav): Rename all mentions of "image" to "media"
class ImageRecord : public base::SupportsWeakPtr<ImageRecord> {
  USING_FAST_MALLOC(ImageRecord);

 public:
  ImageRecord(DOMNodeId new_node_id,
              const MediaTiming* new_media_timing,
              uint64_t new_recorded_size,
              const gfx::Rect& frame_visual_rect,
              const gfx::RectF& root_visual_rect,
              bool is_loaded_after_mouseover_input)
      : node_id(new_node_id),
        media_timing(new_media_timing),
        recorded_size(new_recorded_size),
        is_loaded_after_mouseover(is_loaded_after_mouseover_input) {
    static unsigned next_insertion_index_ = 1;
    insertion_index = next_insertion_index_++;
    if (PaintTimingVisualizer::IsTracingEnabled()) {
      lcp_rect_info_ = std::make_unique<LCPRectInfo>(
          frame_visual_rect, gfx::ToRoundedRect(root_visual_rect));
    }
  }

  ImageRecord() = delete;

  // Returns the image's entropy, in encoded-bits-per-layout-pixel, as used to
  // determine whether the image is a potential LCP candidate. Will return 0.0
  // if there is no `media_timing`.
  double EntropyForLCP() const;

  // Returns the image's loading priority. Will return `absl::nullopt` if there
  // is no `media_timing`.
  absl::optional<WebURLRequest::Priority> RequestPriority() const;

  DOMNodeId node_id = kInvalidDOMNodeId;
  WeakPersistent<const MediaTiming> media_timing;
  // Mind that |recorded_size| has to be assigned before pusing to
  // |size_ordered_set_| since it's the sorting key.
  uint64_t recorded_size = 0;
  unsigned frame_index = 0;
  unsigned insertion_index;
  // The time of the first paint after fully loaded. 0 means not painted yet.
  base::TimeTicks paint_time = base::TimeTicks();
  base::TimeTicks load_time = base::TimeTicks();
  base::TimeTicks first_animated_frame_time = base::TimeTicks();
  bool loaded = false;
  // An animated frame is queued for paint timing.
  bool queue_animated_paint = false;
  // LCP rect information, only populated when tracing is enabled.
  std::unique_ptr<LCPRectInfo> lcp_rect_info_;

  // A non-style image, or a style image that comes from an origin-clean style.
  // Images that come from origin-dirty styles should have some limitations on
  // what they report.
  bool origin_clean = true;

  bool is_loaded_after_mouseover = false;
};

typedef std::pair<const LayoutObject*, const MediaTiming*> RecordId;

// |ImageRecordsManager| is the manager of all of the images that Largest
// Image Paint cares about. Note that an image does not necessarily correspond
// to a node; it can also be one of the background images attached to a node.
// |ImageRecordsManager| encapsulates the logic of |ImageRecord| handling,
// providing interface for the external world to handle it in the language of
// Node, LayoutObject, etc.
class CORE_EXPORT ImageRecordsManager {
  friend class ImagePaintTimingDetectorTest;
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);
  DISALLOW_NEW();

  using NodesQueueComparator = bool (*)(const base::WeakPtr<ImageRecord>&,
                                        const base::WeakPtr<ImageRecord>&);
  using ImageRecordSet =
      std::set<base::WeakPtr<ImageRecord>, NodesQueueComparator>;

 public:
  explicit ImageRecordsManager(LocalFrameView*);
  ImageRecordsManager(const ImageRecordsManager&) = delete;
  ImageRecordsManager& operator=(const ImageRecordsManager&) = delete;
  ImageRecord* LargestImage() const;

  inline void RemoveRecord(const RecordId& record_id) {
    recorded_images_.erase(record_id);
    image_finished_times_.erase(record_id);
    auto it = pending_images_.find(record_id);
    if (it != pending_images_.end()) {
      size_ordered_set_.erase(it->value->AsWeakPtr());
      pending_images_.erase(it);
      // Leave out |images_queued_for_paint_time_| intentionally because the
      // null record can be removed in
      // |AssignPaintTimeToRegisteredQueuedRecords|.
    }
  }
  // Returns whether an image was added to |pending_images_|.
  bool RecordFirstPaintAndReturnIsPending(const RecordId& record_id,
                                          const uint64_t& visual_size,
                                          const gfx::Rect& frame_visual_rect,
                                          const gfx::RectF& root_visual_rect,
                                          double bpp,
                                          bool is_loaded_after_mouseover);
  bool IsRecordedImage(const RecordId& record_id) const {
    return recorded_images_.Contains(record_id);
  }

  void NotifyImageFinished(const RecordId& record_id) {
    // TODO(npm): Ideally NotifyImageFinished() would only be called when the
    // record has not yet been inserted in |image_finished_times_| but that's
    // not currently the case. If we plumb some information from
    // MediaTiming we may be able to ensure that this call does not
    // require the Contains() check, which would save time.
    if (!image_finished_times_.Contains(record_id)) {
      image_finished_times_.insert(record_id, base::TimeTicks::Now());
    }
  }

  inline base::WeakPtr<ImageRecord> GetPendingImage(const RecordId& record_id) {
    auto it = pending_images_.find(record_id);
    return it == pending_images_.end() ? nullptr : it->value->AsWeakPtr();
  }
  bool OnFirstAnimatedFramePainted(const RecordId&,
                                   unsigned current_frame_index);
  void OnImageLoaded(const RecordId&,
                     unsigned current_frame_index,
                     const StyleFetchedImage*);

  // Receives a candidate image painted under opacity 0 but without nested
  // opacity. May update |largest_ignored_image_| if the new candidate has a
  // larger size.
  void MaybeUpdateLargestIgnoredImage(const RecordId&,
                                      const uint64_t& visual_size,
                                      const gfx::Rect& frame_visual_rect,
                                      const gfx::RectF& root_visual_rect,
                                      bool is_loaded_after_mouseover);
  void ReportLargestIgnoredImage(unsigned current_frame_index);

  void AssignPaintTimeToRegisteredQueuedRecords(
      const base::TimeTicks&,
      unsigned last_queued_frame_index);

  void ClearImagesQueuedForPaintTime();
  void Clear();

  void Trace(Visitor* visitor) const;

 private:
  std::unique_ptr<ImageRecord> CreateImageRecord(
      const LayoutObject& object,
      const MediaTiming* media_timing,
      const uint64_t& visual_size,
      const gfx::Rect& frame_visual_rect,
      const gfx::RectF& root_visual_rect,
      bool is_loaded_after_mouseover);
  inline void QueueToMeasurePaintTime(const RecordId& record_id,
                                      base::WeakPtr<ImageRecord>& record,
                                      unsigned current_frame_index) {
    record->frame_index = current_frame_index;
    images_queued_for_paint_time_.push_back(std::make_pair(record, record_id));
  }
  inline void SetLoaded(base::WeakPtr<ImageRecord>& record) {
    record->loaded = true;
  }
  void OnImageLoadedInternal(const RecordId&,
                             base::WeakPtr<ImageRecord>&,
                             unsigned current_frame_index);

  // The ImageRecord corresponding to the largest image that has been loaded and
  // painted.
  std::unique_ptr<ImageRecord> largest_painted_image_;

  // This stores the image records which are pending to load and receive a paint
  // timestamp, ordered by size.
  ImageRecordSet size_ordered_set_;

  // RecordId for images for which we have seen a first paint. A RecordId is
  // added to this set regardless of whether the image could be an LCP
  // candidate.
  HashSet<RecordId> recorded_images_;

  // Map of RecordId to ImageRecord for images for which the first paint has
  // been seen but which do not have the paint time set yet. This may contain
  // only images which are potential LCP candidates.
  HashMap<RecordId, std::unique_ptr<ImageRecord>> pending_images_;

  // |ImageRecord|s waiting for paint time are stored in this map
  // until they get a presentation time.
  Deque<std::pair<base::WeakPtr<ImageRecord>, RecordId>>
      images_queued_for_paint_time_;

  // Map containing timestamps of when LayoutObject::ImageNotifyFinished is
  // first called.
  HashMap<RecordId, base::TimeTicks> image_finished_times_;

  Member<LocalFrameView> frame_view_;

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
 public:
  ImagePaintTimingDetector(LocalFrameView*, PaintTimingCallbackManager*);
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
                   const StyleFetchedImage*,
                   const gfx::Rect& image_border,
                   const bool is_loaded_after_mouseover);
  void NotifyImageFinished(const LayoutObject&, const MediaTiming*);
  void OnPaintFinished();
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
  void ReportPresentationTime(unsigned last_queued_frame_index,
                              base::TimeTicks);

  // Return the candidate.
  ImageRecord* UpdateMetricsCandidate();

  // Called when documentElement changes from zero to nonzero opacity. Makes the
  // largest image that was hidden due to this a Largest Contentful Paint
  // candidate.
  void ReportLargestIgnoredImage();

  bool IsRecordingLargestImagePaint() const {
    return recording_largest_image_paint_;
  }
  void StopRecordingLargestImagePaint() {
    recording_largest_image_paint_ = false;
  }
  void RestartRecordingLargestImagePaint() {
    recording_largest_image_paint_ = true;
    records_manager_.Clear();
  }

  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class ImagePaintTimingDetectorTest;
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);

  void PopulateTraceValue(TracedValue&, const ImageRecord& first_image_paint);
  void RegisterNotifyPresentationTime();
  void ReportCandidateToTrace(ImageRecord&, base::TimeTicks);
  void ReportNoCandidateToTrace();
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

  // Used to find the last candidate.
  unsigned count_candidates_ = 0;

  // Used to decide which frame a record belongs to, monotonically increasing.
  unsigned frame_index_ = 1;
  unsigned last_registered_frame_index_ = 0;
  bool added_entry_in_latest_frame_ = false;

  bool contains_full_viewport_image_ = false;

  // We cache the viewport size computation to avoid performing it on every
  // image. This value is reset when paint is finished and is computed if unset
  // when needed. 0 means that the size has not been computed.
  absl::optional<uint64_t> viewport_size_;
  // Whether the viewport size used is the page viewport.
  bool uses_page_viewport_;
  // Are we recording an LCP candidate? True after a navigation (including soft
  // navigations) until the next user interaction.
  bool recording_largest_image_paint_ = true;

  ImageRecordsManager records_manager_;
  Member<LocalFrameView> frame_view_;
  Member<PaintTimingCallbackManager> callback_manager_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_IMAGE_PAINT_TIMING_DETECTOR_H_
