// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/remote_font_face_source.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/css/css_custom_font_data.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

RemoteFontFaceSource::DisplayPeriod ComputePeriod(
    FontDisplay displayValue,
    RemoteFontFaceSource::Phase phase,
    bool is_intervention_triggered) {
  switch (displayValue) {
    case kFontDisplayAuto:
      if (is_intervention_triggered)
        return RemoteFontFaceSource::kSwapPeriod;
      FALLTHROUGH;
    case kFontDisplayBlock:
      switch (phase) {
        case RemoteFontFaceSource::kNoLimitExceeded:
        case RemoteFontFaceSource::kShortLimitExceeded:
          return RemoteFontFaceSource::kBlockPeriod;
        case RemoteFontFaceSource::kLongLimitExceeded:
          return RemoteFontFaceSource::kSwapPeriod;
      }

    case kFontDisplaySwap:
      return RemoteFontFaceSource::kSwapPeriod;

    case kFontDisplayFallback:
      switch (phase) {
        case RemoteFontFaceSource::kNoLimitExceeded:
          return RemoteFontFaceSource::kBlockPeriod;
        case RemoteFontFaceSource::kShortLimitExceeded:
          return RemoteFontFaceSource::kSwapPeriod;
        case RemoteFontFaceSource::kLongLimitExceeded:
          return RemoteFontFaceSource::kFailurePeriod;
      }

    case kFontDisplayOptional:
      switch (phase) {
        case RemoteFontFaceSource::kNoLimitExceeded:
          return RemoteFontFaceSource::kBlockPeriod;
        case RemoteFontFaceSource::kShortLimitExceeded:
        case RemoteFontFaceSource::kLongLimitExceeded:
          return RemoteFontFaceSource::kFailurePeriod;
      }

    case kFontDisplayEnumMax:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return RemoteFontFaceSource::kSwapPeriod;
}

}  // namespace

RemoteFontFaceSource::RemoteFontFaceSource(CSSFontFace* css_font_face,
                                           FontSelector* font_selector,
                                           FontDisplay display)
    : face_(css_font_face),
      font_selector_(font_selector),
      // No need to report the violation here since the font is not loaded yet
      display_(
          GetFontDisplayWithFeaturePolicyCheck(display,
                                               font_selector,
                                               ReportOptions::kDoNotReport)),
      phase_(kNoLimitExceeded),
      is_intervention_triggered_(ShouldTriggerWebFontsIntervention()) {
  DCHECK(face_);
  period_ = ComputePeriod(display_, phase_, is_intervention_triggered_);
}

RemoteFontFaceSource::~RemoteFontFaceSource() = default;

void RemoteFontFaceSource::Dispose() {
  ClearResource();
  PruneTable();
}

bool RemoteFontFaceSource::IsLoading() const {
  return GetResource() && GetResource()->IsLoading();
}

bool RemoteFontFaceSource::IsLoaded() const {
  return !GetResource();
}

bool RemoteFontFaceSource::IsValid() const {
  return GetResource() || custom_font_data_;
}

void RemoteFontFaceSource::NotifyFinished(Resource* resource) {
  ExecutionContext* execution_context = font_selector_->GetExecutionContext();
  if (!execution_context)
    return;
  // Prevent promise rejection while shutting down the document.
  // See crbug.com/960290
  if (execution_context->IsDocument() &&
      To<Document>(execution_context)->IsDetached())
    return;

  FontResource* font = ToFontResource(resource);
  histograms_.RecordRemoteFont(font);

  custom_font_data_ = font->GetCustomFontData();

  // FIXME: Provide more useful message such as OTS rejection reason.
  // See crbug.com/97467
  if (font->GetStatus() == ResourceStatus::kDecodeError) {
    execution_context->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        "Failed to decode downloaded font: " + font->Url().ElidedString()));
    if (font->OtsParsingMessage().length() > 1) {
      execution_context->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "OTS parsing error: " + font->OtsParsingMessage()));
    }
  }

  ClearResource();

  PruneTable();
  if (face_->FontLoaded(this)) {
    font_selector_->FontFaceInvalidated();

    const scoped_refptr<FontCustomPlatformData> customFontData =
        font->GetCustomFontData();
    if (customFontData) {
      probe::FontsUpdated(execution_context, face_->GetFontFace(),
                          resource->Url().GetString(), customFontData.get());
    }
  }
}

void RemoteFontFaceSource::FontLoadShortLimitExceeded(FontResource*) {
  if (IsLoaded())
    return;
  phase_ = kShortLimitExceeded;
  UpdatePeriod();
}

void RemoteFontFaceSource::FontLoadLongLimitExceeded(FontResource*) {
  if (IsLoaded())
    return;
  phase_ = kLongLimitExceeded;
  UpdatePeriod();

  histograms_.LongLimitExceeded();
}

void RemoteFontFaceSource::SetDisplay(FontDisplay display) {
  // TODO(ksakamoto): If the font is loaded and in the failure period,
  // changing it to block or swap period should update the font rendering
  // using the loaded font.
  if (IsLoaded())
    return;
  display_ = GetFontDisplayWithFeaturePolicyCheck(
      display, font_selector_, ReportOptions::kReportOnFailure);
  UpdatePeriod();
}

void RemoteFontFaceSource::UpdatePeriod() {
  DisplayPeriod new_period =
      ComputePeriod(display_, phase_, is_intervention_triggered_);

  // Fallback font is invisible iff the font is loading and in the block period.
  // Invalidate the font if its fallback visibility has changed.
  if (IsLoading() && period_ != new_period &&
      (period_ == kBlockPeriod || new_period == kBlockPeriod)) {
    PruneTable();
    if (face_->FallbackVisibilityChanged(this))
      font_selector_->FontFaceInvalidated();
    histograms_.RecordFallbackTime();
  }
  period_ = new_period;
}

FontDisplay RemoteFontFaceSource::GetFontDisplayWithFeaturePolicyCheck(
    FontDisplay display,
    const FontSelector* font_selector,
    ReportOptions report) const {
  ExecutionContext* context = font_selector->GetExecutionContext();
  if (display != kFontDisplayFallback && display != kFontDisplayOptional &&
      context && context->IsDocument() &&
      !To<Document>(context)->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kFontDisplay, report)) {
    return kFontDisplayOptional;
  }
  return display;
}

bool RemoteFontFaceSource::ShouldTriggerWebFontsIntervention() {
  const auto* document =
      DynamicTo<Document>(font_selector_->GetExecutionContext());
  if (!document)
    return false;

  WebEffectiveConnectionType connection_type =
      GetNetworkStateNotifier().EffectiveType();

  bool network_is_slow =
      WebEffectiveConnectionType::kTypeOffline <= connection_type &&
      connection_type <= WebEffectiveConnectionType::kType3G;

  return network_is_slow && display_ == kFontDisplayAuto;
}

bool RemoteFontFaceSource::IsLowPriorityLoadingAllowedForRemoteFont() const {
  return is_intervention_triggered_;
}

scoped_refptr<SimpleFontData> RemoteFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  if (period_ == kFailurePeriod || !IsValid())
    return nullptr;
  if (!IsLoaded())
    return CreateLoadingFallbackFontData(font_description);
  DCHECK(custom_font_data_);

  histograms_.RecordFallbackTime();

  return SimpleFontData::Create(
      custom_font_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.IsSyntheticBold(),
          font_description.IsSyntheticItalic(),
          font_description.GetFontSelectionRequest(),
          font_selection_capabilities, font_description.FontOpticalSizing(),
          font_description.Orientation(), font_description.VariationSettings()),
      CustomFontData::Create());
}

scoped_refptr<SimpleFontData>
RemoteFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  // This temporary font is not retained and should not be returned.
  FontCachePurgePreventer font_cache_purge_preventer;
  scoped_refptr<SimpleFontData> temporary_font =
      FontCache::GetFontCache()->GetLastResortFallbackFont(font_description,
                                                           kDoNotRetain);
  if (!temporary_font) {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CSSCustomFontData> css_font_data = CSSCustomFontData::Create(
      this, period_ == kBlockPeriod ? CSSCustomFontData::kInvisibleFallback
                                    : CSSCustomFontData::kVisibleFallback);
  return SimpleFontData::Create(temporary_font->PlatformData(), css_font_data);
}

void RemoteFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded())
    return;
  DCHECK(GetResource());

  SetDisplay(face_->GetFontFace()->GetFontDisplay());

  FontResource* font = ToFontResource(GetResource());
  if (font->StillNeedsLoad()) {
    if (font->IsLowPriorityLoadingAllowedForRemoteFont()) {
      font_selector_->GetExecutionContext()->AddConsoleMessage(
          ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kIntervention,
              mojom::ConsoleMessageLevel::kInfo,
              "Slow network is detected. See "
              "https://www.chromestatus.com/feature/5636954674692096 for more "
              "details. Fallback font will be used while loading: " +
                  font->Url().ElidedString()));

      // Set the loading priority to VeryLow only when all other clients agreed
      // that this font is not required for painting the text.
      font->DidChangePriority(ResourceLoadPriority::kVeryLow, 0);
    }
    if (font_selector_->GetExecutionContext()->Fetcher()->StartLoad(font))
      histograms_.LoadStarted();
  }

  // Start the timers upon the first load request from RemoteFontFaceSource.
  // Note that <link rel=preload> may have initiated loading without kicking
  // off the timers.
  font->StartLoadLimitTimersIfNecessary(
      font_selector_->GetExecutionContext()
          ->GetTaskRunner(TaskType::kInternalLoading)
          .get());

  face_->DidBeginLoad();
}

void RemoteFontFaceSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  CSSFontFaceSource::Trace(visitor);
  FontResourceClient::Trace(visitor);
}

void RemoteFontFaceSource::FontLoadHistograms::LoadStarted() {
  if (load_start_time_.is_null())
    load_start_time_ = base::TimeTicks::Now();
}

void RemoteFontFaceSource::FontLoadHistograms::FallbackFontPainted(
    DisplayPeriod period) {
  if (period == kBlockPeriod && blank_paint_time_.is_null()) {
    blank_paint_time_ = base::TimeTicks::Now();
    blank_paint_time_recorded_ = false;
  }
}

void RemoteFontFaceSource::FontLoadHistograms::LongLimitExceeded() {
  is_long_limit_exceeded_ = true;
  MaySetDataSource(kFromNetwork);
}

void RemoteFontFaceSource::FontLoadHistograms::RecordFallbackTime() {
  if (blank_paint_time_.is_null() || blank_paint_time_recorded_)
    return;
  base::TimeDelta duration = base::TimeTicks::Now() - blank_paint_time_;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram,
                                  blank_text_shown_time_histogram,
                                  ("WebFont.BlankTextShownTime", 0, 10000, 50));
  blank_text_shown_time_histogram.Count(
      base::saturated_cast<base::HistogramBase::Sample>(
          duration.InMilliseconds()));
  blank_paint_time_recorded_ = true;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordRemoteFont(
    const FontResource* font) {
  MaySetDataSource(DataSourceForLoadFinish(font));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, cache_hit_histogram,
                                  ("WebFont.CacheHit", kCacheHitEnumMax));
  cache_hit_histogram.Count(DataSourceMetricsValue());

  if (data_source_ == kFromDiskCache || data_source_ == kFromNetwork) {
    DCHECK(!load_start_time_.is_null());
    RecordLoadTimeHistogram(font, base::TimeTicks::Now() - load_start_time_);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::MaySetDataSource(
    DataSource data_source) {
  if (data_source_ != kFromUnknown)
    return;
  // Classify as memory cache hit if |load_start_time_| is not set, i.e.
  // this RemoteFontFaceSource instance didn't trigger FontResource
  // loading.
  if (load_start_time_.is_null())
    data_source_ = kFromMemoryCache;
  else
    data_source_ = data_source;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordLoadTimeHistogram(
    const FontResource* font,
    base::TimeDelta delta) {
  CHECK_NE(kFromUnknown, data_source_);

  int duration =
      base::saturated_cast<base::HistogramBase::Sample>(delta.InMilliseconds());
  if (font->ErrorOccurred()) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, load_error_histogram,
        ("WebFont.DownloadTime.LoadError", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_load_error_histogram,
        ("WebFont.MissedCache.DownloadTime.LoadError", 0, 10000, 50));
    load_error_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_load_error_histogram.Count(duration);
    return;
  }

  size_t size = font->EncodedSize();
  if (size < 10 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under10k_histogram,
        ("WebFont.DownloadTime.0.Under10KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under10k_histogram,
        ("WebFont.MissedCache.DownloadTime.0.Under10KB", 0, 10000, 50));
    under10k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under10k_histogram.Count(duration);
    return;
  }
  if (size < 50 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under50k_histogram,
        ("WebFont.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under50k_histogram,
        ("WebFont.MissedCache.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    under50k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under50k_histogram.Count(duration);
    return;
  }
  if (size < 100 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under100k_histogram,
        ("WebFont.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under100k_histogram,
        ("WebFont.MissedCache.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    under100k_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under100k_histogram.Count(duration);
    return;
  }
  if (size < 1024 * 1024) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, under1mb_histogram,
        ("WebFont.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, missed_cache_under1mb_histogram,
        ("WebFont.MissedCache.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    under1mb_histogram.Count(duration);
    if (data_source_ == kFromNetwork)
      missed_cache_under1mb_histogram.Count(duration);
    return;
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, over1mb_histogram,
      ("WebFont.DownloadTime.4.Over1MB", 0, 10000, 50));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, missed_cache_over1mb_histogram,
      ("WebFont.MissedCache.DownloadTime.4.Over1MB", 0, 10000, 50));
  over1mb_histogram.Count(duration);
  if (data_source_ == kFromNetwork)
    missed_cache_over1mb_histogram.Count(duration);
}

RemoteFontFaceSource::FontLoadHistograms::CacheHitMetrics
RemoteFontFaceSource::FontLoadHistograms::DataSourceMetricsValue() {
  switch (data_source_) {
    case kFromDataURL:
      return kDataUrl;
    case kFromMemoryCache:
      return kMemoryHit;
    case kFromDiskCache:
      return kDiskHit;
    case kFromNetwork:
      return kMiss;
    case kFromUnknown:
    // Fall through.
    default:
      NOTREACHED();
  }
  return kMiss;
}

}  // namespace blink
