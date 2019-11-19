// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_
#define CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace content {

struct CONTENT_EXPORT RecordTabSwitchTimeRequest {
  RecordTabSwitchTimeRequest() = default;
  ~RecordTabSwitchTimeRequest() = default;

  RecordTabSwitchTimeRequest(base::TimeTicks tab_switch_start_time,
                             bool destination_is_loaded,
                             bool destination_is_frozen);

  // The time at which the tab switch has been initiated.
  base::TimeTicks tab_switch_start_time = base::TimeTicks();
  // Indicates if the destination tab is loaded when initiating the tab switch.
  bool destination_is_loaded = false;
  // Indicates if the destination tab is frozen when initiating the tab switch.
  bool destination_is_frozen = false;
};

// Generates UMA metric to track the duration of tab switching from when the
// active tab is changed until the frame presentation time. The metric will be
// separated into two whether the tab switch has saved frames or not.
class CONTENT_EXPORT TabSwitchTimeRecorder {
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

  TabSwitchTimeRecorder();
  ~TabSwitchTimeRecorder();

  // Invoked when the tab associated with this recorder is shown. Returns a
  // callback to invoke the next time a frame is presented for this tab.
  base::OnceCallback<void(const gfx::PresentationFeedback&)> TabWasShown(
      bool has_saved_frames,
      const RecordTabSwitchTimeRequest& start_state,
      base::TimeTicks render_widget_visibility_request_timestamp);

  // Indicates that the tab associated with this recorder was hidden. If no
  // frame was presented since the last tab switch, failure is reported to UMA.
  void TabWasHidden();

 private:
  // Records histograms and trace events for the current tab switch.
  void RecordHistogramsAndTraceEvents(
      bool is_incomplete,
      const gfx::PresentationFeedback& feedback);

  // Whether there was a saved frame for the last tab switch.
  bool has_saved_frames_;

  // The information about the last tab switch request, or nullopt if there is
  // no incomplete tab switch.
  base::Optional<RecordTabSwitchTimeRequest> tab_switch_start_state_;

  // The render widget visibility request timestamp for the last tab switch, or
  // null if there is no incomplete tab switch.
  base::TimeTicks render_widget_visibility_request_timestamp_;

  base::WeakPtrFactory<TabSwitchTimeRecorder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabSwitchTimeRecorder);
};

}  // namespace content

#endif  // CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_
