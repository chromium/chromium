// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"

#import "base/json/values_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/timer/elapsed_timer.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_prefs.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"

namespace {

// Updates the most recently used timestamp for Reading Mode usage with
// the current event time.
void UpdateRecentlyUsedTimestamps(PrefService* prefs) {
  const base::Time now = base::Time::Now();
  ScopedListPrefUpdate reader_mode_timestamps_pref_update(
      prefs, reader_mode_prefs::kReaderModeRecentlyUsedTimestampsPref);
  reader_mode_timestamps_pref_update->Append(base::TimeToValue(now));

  // Only keep the last 5 timestamps to maintain a small size.
  constexpr size_t kMaxTimestamps = 5;
  if (reader_mode_timestamps_pref_update->size() > kMaxTimestamps) {
    size_t entries_to_erase =
        reader_mode_timestamps_pref_update->size() - kMaxTimestamps;
    reader_mode_timestamps_pref_update->erase(
        reader_mode_timestamps_pref_update->begin(),
        reader_mode_timestamps_pref_update->begin() + entries_to_erase);
  }
}

// Converts dom_distiller::mojom::FontFamily to Reader mode metric type.
ReaderModeFontFamily ConvertMojomFontFamily(
    dom_distiller::mojom::FontFamily font_family) {
  switch (font_family) {
    case dom_distiller::mojom::FontFamily::kSansSerif:
      return ReaderModeFontFamily::kSansSerif;
    case dom_distiller::mojom::FontFamily::kSerif:
      return ReaderModeFontFamily::kSerif;
    case dom_distiller::mojom::FontFamily::kMonospace:
      return ReaderModeFontFamily::kMonospace;
  }
}

// Converts dom_distiller::mojom::Theme to Reader mode metric type.
ReaderModeTheme ConvertMojomTheme(dom_distiller::mojom::Theme theme) {
  switch (theme) {
    case dom_distiller::mojom::Theme::kDark:
      return ReaderModeTheme::kDark;
    case dom_distiller::mojom::Theme::kLight:
      return ReaderModeTheme::kLight;
    case dom_distiller::mojom::Theme::kSepia:
      return ReaderModeTheme::kSepia;
  }
}

// Returns the mapping of access point and distillation result for recording.
ReaderModeDistillerOutcome GetDistillerOutcome(
    ReaderModeAccessPoint access_point,
    ReaderModeDistillerResult result) {
  switch (access_point) {
    case ReaderModeAccessPoint::kContextualChip:
      return result == ReaderModeDistillerResult::kPageIsDistillable
                 ? ReaderModeDistillerOutcome::kContextualChipIsDistillable
                 : ReaderModeDistillerOutcome::kContextualChipIsNotDistillable;
    case ReaderModeAccessPoint::kToolsMenu:
      return result == ReaderModeDistillerResult::kPageIsDistillable
                 ? ReaderModeDistillerOutcome::kToolsMenuIsDistillable
                 : ReaderModeDistillerOutcome::kToolsMenuIsNotDistillable;
    case ReaderModeAccessPoint::kAIHub:
      return result == ReaderModeDistillerResult::kPageIsDistillable
                 ? ReaderModeDistillerOutcome::kAIHubIsDistillable
                 : ReaderModeDistillerOutcome::kAIHubIsNotDistillable;
  }
}

}  // namespace

ReaderModeMetricsHelper::ReaderModeMetricsHelper(
    web::WebState* web_state,
    dom_distiller::DistilledPagePrefs* distilled_page_prefs)
    : web_state_(web_state), distilled_page_prefs_(distilled_page_prefs) {
  scoped_observation_.Observe(distilled_page_prefs);
}

ReaderModeMetricsHelper::~ReaderModeMetricsHelper() {
  Flush();
}

void ReaderModeMetricsHelper::RecordReaderHeuristicCanceled() {
  last_reader_mode_state_ = ReaderModeState::kHeuristicCanceled;
  Flush();
}

void ReaderModeMetricsHelper::RecordReaderHeuristicTriggered() {
  heuristic_timer_ = std::make_unique<base::ElapsedTimer>();
  last_reader_mode_state_ = ReaderModeState::kHeuristicStarted;
}

void ReaderModeMetricsHelper::RecordReaderHeuristicCompleted(
    ReaderModeHeuristicResult result) {
  base::UmaHistogramEnumeration(kReaderModeHeuristicResultHistogram, result);

  last_reader_mode_state_ = ReaderModeState::kHeuristicCompleted;

  const ukm::SourceId result_source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (result_source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Result(result_source_id)
        .SetResult(static_cast<int64_t>(result))
        .Record(ukm::UkmRecorder::Get());
  }

  // If the heuristic is canceled before the start delay then skip latency
  // recording.
  if (!heuristic_timer_) {
    return;
  }
  base::TimeDelta elapsed = heuristic_timer_->Elapsed();
  base::UmaHistogramTimes(kReaderModeHeuristicLatencyHistogram, elapsed);

  const ukm::SourceId latency_source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (latency_source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Latency(latency_source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}

void ReaderModeMetricsHelper::RecordReaderDistillerTriggered(
    ReaderModeAccessPoint access_point) {
  distiller_timer_ = std::make_unique<base::ElapsedTimer>();
  last_reader_mode_state_ = ReaderModeState::kDistillationStarted;
  base::UmaHistogramEnumeration(kReaderModeAccessPointHistogram, access_point);
}

void ReaderModeMetricsHelper::RecordReaderDistillerTimedOut() {
  last_reader_mode_state_ = ReaderModeState::kDistillationTimedOut;
  RecordDistillationTime(std::nullopt);
  Flush();
}

void ReaderModeMetricsHelper::RecordReaderDistillerCompleted(
    ReaderModeAccessPoint access_point,
    ReaderModeDistillerResult result) {
  last_reader_mode_state_ = ReaderModeState::kDistillationCompleted;

  CHECK(distiller_timer_);
  RecordDistillationTime(result);
  base::UmaHistogramEnumeration(kReaderModeDistillerResultHistogram,
                                GetDistillerOutcome(access_point, result));
}

void ReaderModeMetricsHelper::RecordReaderShown() {
  last_reader_mode_state_.reset();
  base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                ReaderModeState::kReaderShown);
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  UpdateRecentlyUsedTimestamps(pref_service);

  reading_timer_ = std::make_unique<base::ElapsedTimer>();
}

void ReaderModeMetricsHelper::Flush() {
  if (last_reader_mode_state_.has_value()) {
    base::UmaHistogramEnumeration(kReaderModeStateHistogram,
                                  last_reader_mode_state_.value());
    last_reader_mode_state_.reset();
  }
  if (reading_timer_) {
    base::TimeDelta elapsed = reading_timer_->Elapsed();
    base::UmaHistogramLongTimes100(kReaderModeTimeSpentHistogram, elapsed);
    reading_timer_.reset();
  }
  heuristic_timer_.reset();
}

void ReaderModeMetricsHelper::OnChangeFontFamily(
    dom_distiller::mojom::FontFamily font) {
  base::UmaHistogramEnumeration(kReaderModeCustomizationHistogram,
                                ReaderModeCustomizationType::kFontFamily);
  base::UmaHistogramEnumeration(kReaderModeFontFamilyCustomizationHistogram,
                                ConvertMojomFontFamily(font));
}

void ReaderModeMetricsHelper::OnChangeTheme(dom_distiller::mojom::Theme theme) {
  base::UmaHistogramEnumeration(kReaderModeCustomizationHistogram,
                                ReaderModeCustomizationType::kTheme);
  base::UmaHistogramEnumeration(kReaderModeThemeCustomizationHistogram,
                                ConvertMojomTheme(theme));
}

void ReaderModeMetricsHelper::OnChangeFontScaling(float scaling) {
  base::UmaHistogramEnumeration(kReaderModeCustomizationHistogram,
                                ReaderModeCustomizationType::kFontScale);
  base::UmaHistogramSparse(kReaderModeFontScaleCustomizationHistogram,
                           std::floor(scaling * 100));
}

void ReaderModeMetricsHelper::RecordDistillationTime(
    std::optional<ReaderModeDistillerResult> result) {
  if (!distiller_timer_) {
    return;
  }
  base::TimeDelta elapsed = distiller_timer_->Elapsed();
  base::UmaHistogramTimes(kReaderModeDistillerLatencyHistogram, elapsed);

  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Latency(source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
    if (result.has_value()) {
      ukm::builders::IOS_ReaderMode_Distiller_Result(source_id)
          .SetResult(static_cast<int64_t>(result.value()))
          .Record(ukm::UkmRecorder::Get());
    }
  }

  distiller_timer_.reset();
}
