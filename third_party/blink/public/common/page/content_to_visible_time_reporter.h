// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REPORTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REPORTER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace gfx {
struct PresentationFeedback;
}

namespace blink {

// Generates UMA metric to track the duration of tab switching from when the
// active tab is changed until the frame presentation time. The metric will be
// separated into two whether the tab switch has saved frames or not.
class BLINK_COMMON_EXPORT ContentToVisibleTimeReporter {
 public:
  // Matches the TabSwitchResult enum in enums.xml.
  enum class TabSwitchResult {
    // A frame was successfully presented after a tab switch.
    kSuccess = 0,
    // Tab was hidden before a frame was presented after a tab switch.
    kIncomplete = 1,
    // Compositor reported a failure after a tab switch.
    kPresentationFailure = 2,
    kMaxValue = kPresentationFailure,
  };

  ContentToVisibleTimeReporter();
  ContentToVisibleTimeReporter(const ContentToVisibleTimeReporter&) = delete;
  ContentToVisibleTimeReporter& operator=(const ContentToVisibleTimeReporter&) =
      delete;
  ~ContentToVisibleTimeReporter();

  // Invoked when the tab associated with this recorder is shown. Returns a
  // callback to invoke the next time a frame is presented for this tab.
  base::OnceCallback<void(const gfx::PresentationFeedback&)> TabWasShown(
      bool has_saved_frames,
      mojom::RecordContentToVisibleTimeRequestPtr start_state);

  base::OnceCallback<void(const gfx::PresentationFeedback&)> TabWasShown(
      bool has_saved_frames,
      base::TimeTicks event_start_time,
      bool destination_is_loaded,
      bool show_reason_tab_switching,
      bool show_reason_unoccluded,
      bool show_reason_bfcache_restore);

  // Indicates that the tab associated with this recorder was hidden. If no
  // frame was presented since the last tab switch, failure is reported to UMA.
  void TabWasHidden();

 private:
  // Records histograms and trace events for the current tab switch.
  void RecordHistogramsAndTraceEvents(
      bool is_incomplete,
      bool show_reason_tab_switching,
      bool show_reason_unoccluded,
      bool show_reason_bfcache_restore,
      const gfx::PresentationFeedback& feedback);

  // Whether there was a saved frame for the last tab switch.
  bool has_saved_frames_;

  // The information about the last tab switch request, or nullptr if there is
  // no incomplete tab switch.
  mojom::RecordContentToVisibleTimeRequestPtr tab_switch_start_state_;

  // Cache the feature value for faster lookups.
  bool is_tab_switch_metric2_feature_enabled_;

  base::WeakPtrFactory<ContentToVisibleTimeReporter> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REPORTER_H_
