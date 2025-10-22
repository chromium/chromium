// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"

namespace base {
class ElapsedTimer;
}

namespace web {
class WebState;
}

class ReaderModeMetricsHelper
    : public dom_distiller::DistilledPagePrefs::Observer {
 public:
  ReaderModeMetricsHelper(web::WebState* web_state,
                          dom_distiller::DistilledPagePrefs* prefs);
  ~ReaderModeMetricsHelper() override;

  // Records histograms for the Reading Mode heuristic event.
  void RecordReaderHeuristicTriggered();
  void RecordReaderHeuristicCompleted(ReaderModeHeuristicResult result);

  // Records the heuristic cancelation and resets state for the next event.
  void RecordReaderHeuristicCanceled();

  // Returns true if the Reading Mode feature usage meets the configurable
  // criteria for number of times used across a time span.
  bool ReaderModeIsRecentlyUsed();

  // Records histograms for the Reading Mode distillation event.
  void RecordReaderDistillerTriggered(ReaderModeAccessPoint access_point,
                                      bool is_incognito);
  void RecordReaderDistillerCompleted(ReaderModeAccessPoint access_point,
                                      ReaderModeDistillerResult result);

  // Records the distillation timeout and resets state for the next event.
  void RecordReaderDistillerTimedOut();

  // Records that the last event, showing the Reading Mode UI, has completed.
  void RecordReaderShown();

  // Records the last state of Reading Mode events.
  void Flush(ReaderModeDeactivationReason reason);

  // dom_distiller::DistilledPagePrefs::Observer implementation.
  void OnChangeFontFamily(dom_distiller::mojom::FontFamily font) override;
  void OnChangeTheme(
      dom_distiller::mojom::Theme theme,
      dom_distiller::ThemeSettingsUpdateSource source) override;
  void OnChangeFontScaling(float scaling) override;

 private:
  // Records the distillation time for the web page and its result if the
  // distillation completed successfully.
  void RecordDistillationTime(std::optional<ReaderModeDistillerResult> result);

  std::unique_ptr<base::ElapsedTimer> heuristic_timer_;
  std::unique_ptr<base::ElapsedTimer> distiller_timer_;
  std::unique_ptr<base::ElapsedTimer> reading_timer_;

  // Tracks the last state that was recorded in the Reading Mode events.
  std::optional<ReaderModeState> last_reader_mode_state_;
  // Access point used to trigger Reading Mode distillation.
  std::optional<ReaderModeAccessPoint> reader_mode_distilled_access_point_;
  raw_ptr<web::WebState> web_state_;
  raw_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs_;
  base::ScopedObservation<dom_distiller::DistilledPagePrefs,
                          dom_distiller::DistilledPagePrefs::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_METRICS_HELPER_H_
