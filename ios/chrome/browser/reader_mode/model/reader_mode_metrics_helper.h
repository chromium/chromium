// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"

namespace base {
class ElapsedTimer;
}

namespace web {
class WebState;
}

class ReaderModeMetricsHelper {
 public:
  ReaderModeMetricsHelper(web::WebState* web_state);
  ~ReaderModeMetricsHelper();

  // Records histograms for the Reading Mode heuristic event.
  void RecordReaderHeuristicTriggered();
  void RecordReaderHeuristicCompleted(ReaderModeHeuristicResult result);

  // Stops recording heuristic and resets state for the next event.
  void CancelReaderHeuristicRecording();

 private:
  std::unique_ptr<base::ElapsedTimer> heuristic_timer_;
  // Tracks the last state that was recorded in the Reading Mode events.
  std::optional<ReaderModeState> last_reader_mode_state_;
  raw_ptr<web::WebState> web_state_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_
