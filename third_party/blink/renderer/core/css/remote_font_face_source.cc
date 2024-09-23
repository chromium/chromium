// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/remote_font_face_source.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/css/css_custom_font_data.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

bool RemoteFontFaceSource::NeedsInterventionToAlignWithLCPGoal() const {
  DCHECK_EQ(display_, FontDisplay::kAuto);
  if (!base::FeatureList::IsEnabled(
          features::kAlignFontDisplayAutoTimeoutWithLCPGoal)) {
    return false;
  }
  if (!GetDocument() ||
      !FontFaceSetDocument::From(*GetDocument())->HasReachedLCPLimit()) {
    return false;
  }
  // If a 'font-display: auto' font hasn't finished loading by the LCP limit, it
  // should enter the swap or failure period immediately, so that it doesn't
  // become a source of bad LCP. The only exception is when the font is
  // immediately available from the memory cache, in which case it can be used
  // right away without any latency.
  return !IsLoaded() ||
         (!FinishedFromMemoryCache() && !finished_before_lcp_limit_);
}

RemoteFontFaceSource::DisplayPeriod
RemoteFontFaceSource::ComputeFontDisplayAutoPeriod() const {
  DCHECK_EQ(display_, FontDisplay::kAuto);
  if (NeedsInterventionToAlignWithLCPGoal()) {
    using Mode = features::AlignFontDisplayAutoTimeoutWithLCPGoalMode;
    Mode mode =
        features::kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam.Get();
    if (mode == Mode::kToSwapPeriod) {
      return kSwapPeriod;
    }
    DCHECK_EQ(Mode::kToFailurePeriod, mode);
    if (custom_font_data_ && !custom_font_data_->MayBeIconFont()) {
      return kFailurePeriod;
    }
    return kSwapPeriod;
  }

  if (is_intervention_triggered_) {
    return kSwapPeriod;
  }

  switch (phase_) {
    case kNoLimitExceeded:
    case kShortLimitExceeded:
      return kBlockPeriod;
    case kLongLimitExceeded:
      return kSwapPeriod;
  }
}

RemoteFontFaceSource::DisplayPeriod RemoteFontFaceSource::ComputePeriod()
    const {
  switch (display_) {
    case FontDisplay::kAuto:
      return ComputeFontDisplayAutoPeriod();
    case FontDisplay::kBlock:
      switch (phase_) {
        case kNoLimitExceeded:
        case kShortLimitExceeded:
          return kBlockPeriod;
        case kLongLimitExceeded:
          return kSwapPeriod;
      }

    case FontDisplay::kSwap:
      return kSwapPeriod;

    case FontDisplay::kFallback:
      switch (phase_) {
        case kNoLimitExceeded:
          return kBlockPeriod;
        case kShortLimitExceeded:
          return kSwapPeriod;
        case kLongLimitExceeded:
          return kFailurePeriod;
      }

    case FontDisplay::kOptional: {
      if (!GetDocument()) {
        switch (phase_) {
          case kNoLimitExceeded:
            return kBlockPeriod;
          case kShortLimitExceeded:
          case kLongLimitExceeded:
            return kFailurePeriod;
        }
      }

      // We simply skip the block period, as we should never render invisible
      // fallback for 'font-display: optional'.

      if (GetDocument()->RenderingHasBegun()) {
        if (FinishedFromMemoryCache() ||
            finished_before_document_rendering_begin_ ||
            !paint_requested_while_pending_) {
          return kSwapPeriod;
        }
        return kFailurePeriod;
      }

      return kSwapPeriod;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return kSwapPeriod;
}

RemoteFontFaceSource::RemoteFontFaceSource(
    CSSFontFace* css_font_face,
    FontSelector* font_selector,
    FontDisplay display,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : face_(css_font_face),
      font_selector_(font_selector),
      display_(display),
      phase_(kNoLimitExceeded),
      is_intervention_triggered_(ShouldTriggerWebFontsIntervention()),
      finished_before_document_rendering_begin_(false),
      paint_requested_while_pending_(false),
      finished_before_lcp_limit_(false) {
  DCHECK(face_);
  period_ = ComputePeriod();
}

RemoteFontFaceSource::~RemoteFontFaceSource() = default;

Document* RemoteFontFaceSource::GetDocument() const {
  auto* window =
      DynamicTo<LocalDOMWindow>(font_selector_->GetExecutionContext());
  return window ? window->document() : nullptr;
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
  if (!execution_context) {
    return;
  }
  DCHECK(execution_context->IsContextThread());
  // Prevent promise rejection while shutting down the document.
  // See crbug.com/960290
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (window && window->document()->IsDetached()) {
    return;
  }

  auto* font = To<FontResource>(resource);
  histograms_.RecordRemoteFont(font);

  // Refer to the comments in classic_pending_script.cc for the reason why
  // SRI checks should be done here in ResourceClient instead of
  // ResourceFetcher. SRI failure should behave as network error
  // (ErrorOccurred()). PreloadCache even caches network errors.
  // Font fetch itself doesn't support SRI but font preload does.
  // So, if the resource was preloaded we need to check
  // SRI failure and simulate network error if it happens.

  if (resource->IsLinkPreload()) {
    SubresourceIntegrityHelper::DoReport(*execution_context,
                                         resource->IntegrityReportInfo());
  }

  DCHECK(!custom_font_data_);
  // font->GetCustomFontData() returns nullptr if network error happened
  // (ErrorOccurred() is true). To simulate network error we don't update
  // custom_font_data_ to keep the nullptr value in case of SRI failures.
  if (!resource->IsLinkPreload() || resource->IntegrityDisposition() !=
                                        ResourceIntegrityDisposition::kFailed) {
    custom_font_data_ = font->GetCustomFontData();
  }
  url_ = resource->Url().GetString();

  // FIXME: Provide more useful message such as OTS rejection reason.
  // See crbug.com/97467
  if (font->GetStatus() == ResourceStatus::kDecodeError) {
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        "Failed to decode downloaded font: " + font->Url().ElidedString()));
    if (!font->OtsParsingMessage().empty()) {
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "OTS parsing error: " + font->OtsParsingMessage()));
    }
  }

  ClearResource();
  ClearTable();

  if (GetDocument()) {
    if (!GetDocument()->RenderingHasBegun()) {
      finished_before_document_rendering_begin_ = true;
    }
    if (!FontFaceSetDocument::From(*GetDocument())->HasReachedLCPLimit()) {
      finished_before_lcp_limit_ = true;
    }
  }

  if (FinishedFromMemoryCache()) {
    period_ = kNotApplicablePeriod;
  } else {
    UpdatePeriod();
  }

  if (face_->FontLoaded(this)) {
    font_selector_->FontFaceInvalidated(
        FontInvalidationReason::kFontFaceLoaded);
    if (custom_font_data_) {
      probe::FontsUpdated(execution_context, face_->GetFontFace(),
                          resource->Url().GetString(), custom_font_data_.Get());
    }
  }
}

void RemoteFontFaceSource::FontLoadShortLimitExceeded(FontResource*) {
  if (IsLoaded()) {
    return;
  }
  phase_ = kShortLimitExceeded;
  UpdatePeriod();
}

void RemoteFontFaceSource::FontLoadLongLimitExceeded(FontResource*) {
  if (IsLoaded()) {
    return;
  }
  phase_ = kLongLimitExceeded;
  UpdatePeriod();

  histograms_.LongLimitExceeded();
}

void RemoteFontFaceSource::SetDisplay(FontDisplay display) {
  // TODO(ksakamoto): If the font is loaded and in the failure period,
  // changing it to block or swap period should update the font rendering
  // using the loaded font.
  if (IsLoaded()) {
    return;
  }
  display_ = display;
  UpdatePeriod();
}

bool RemoteFontFaceSource::UpdatePeriod() {
  DisplayPeriod new_period = ComputePeriod();
  bool changed = new_period != period_;

  // Fallback font is invisible iff the font is loading and in the block period.
  // Invalidate the font if its fallback visibility has changed.
  if (IsLoading() && period_ != new_period &&
      (period_ == kBlockPeriod || new_period == kBlockPeriod)) {
    ClearTable();
    if (face_->FallbackVisibilityChanged(this)) {
      font_selector_->FontFaceInvalidated(
          FontInvalidationReason::kGeneralInvalidation);
    }
    histograms_.RecordFallbackTime();
  }
  period_ = new_period;
  return changed;
}

bool RemoteFontFaceSource::ShouldTriggerWebFontsIntervention() {
  if (!IsA<LocalDOMWindow>(font_selector_->GetExecutionContext())) {
    return false;
  }

  WebEffectiveConnectionType connection_type =
      GetNetworkStateNotifier().EffectiveType();

  bool network_is_slow =
      WebEffectiveConnectionType::kTypeOffline <= connection_type &&
      connection_type <= WebEffectiveConnectionType::kType3G;

  return network_is_slow && display_ == FontDisplay::kAuto;
}

bool RemoteFontFaceSource::IsLowPriorityLoadingAllowedForRemoteFont() const {
  return is_intervention_triggered_;
}

const SimpleFontData* RemoteFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  if (period_ == kFailurePeriod || !IsValid()) {
    return nullptr;
  }
  if (!IsLoaded()) {
    return CreateLoadingFallbackFontData(font_description);
  }
  DCHECK(custom_font_data_);

  histograms_.RecordFallbackTime();

  return MakeGarbageCollected<SimpleFontData>(
      custom_font_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.AdjustedSpecifiedSize(),
          font_description.IsSyntheticBold() &&
              font_description.SyntheticBoldAllowed(),
          font_description.IsSyntheticItalic() &&
              font_description.SyntheticItalicAllowed(),
          font_description.GetFontSelectionRequest(),
          font_selection_capabilities, font_description.FontOpticalSizing(),
          font_description.TextRendering(),
          font_description.GetFontVariantAlternates()
              ? font_description.GetFontVariantAlternates()
                    ->GetResolvedFontFeatures()
              : ResolvedFontFeatures(),
          font_description.Orientation(), font_description.VariationSettings(),
          font_description.GetFontPalette()),
      MakeGarbageCollected<CustomFontData>());
}

const SimpleFontData* RemoteFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  // This temporary font is not retained and should not be returned.
  FontCachePurgePreventer font_cache_purge_preventer;
  const SimpleFontData* temporary_font =
      FontCache::Get().GetLastResortFallbackFont(font_description);
  if (!temporary_font) {
    DUMP_WILL_BE_NOTREACHED();
    return nullptr;
  }
  CSSCustomFontData* css_font_data = MakeGarbageCollected<CSSCustomFontData>(
      this, period_ == kBlockPeriod ? CSSCustomFontData::kInvisibleFallback
                                    : CSSCustomFontData::kVisibleFallback);
  return MakeGarbageCollected<SimpleFontData>(&temporary_font->PlatformData(),
                                              css_font_data);
}

void RemoteFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded()) {
    return;
  }
  ExecutionContext* const execution_context =
      font_selector_->GetExecutionContext();
  if (!execution_context) {
    return;
  }

  DCHECK(GetResource());

  SetDisplay(face_->GetFontFace()->GetFontDisplay());

  auto* font = To<FontResource>(GetResource());
  CHECK(font);
  if (font->StillNeedsLoad()) {
    TRACE_EVENT("devtools.timeline", "BeginRemoteFontLoad", "id",
                font->InspectorId(), "display",
                face_->GetFontFace()->display());
    if (font->IsLowPriorityLoadingAllowedForRemoteFont()) {
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kIntervention,
          mojom::blink::ConsoleMessageLevel::kInfo,
          "Slow network is detected. See "
          "https://www.chromestatus.com/feature/5636954674692096 for more "
          "details. Fallback font will be used while loading: " +
              font->Url().ElidedString()));

      // Set the loading priority to VeryLow only when all other clients agreed
      // that this font is not required for painting the text.
      font->DidChangePriority(ResourceLoadPriority::kVeryLow, 0);
    }
    if (execution_context->Fetcher()->StartLoad(font)) {
      histograms_.LoadStarted();
      if (LocalDOMWindow* window =
              DynamicTo<LocalDOMWindow>(execution_context)) {
        if (LocalFrame* frame = window->GetFrame()) {
          if (frame->IsOutermostMainFrame()) {
            if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
              lcpp->OnFontFetched(font->Url());
            }
          }
        }
      }
    }
  }

  // Start the timers upon the first load request from RemoteFontFaceSource.
  // Note that <link rel=preload> may have initiated loading without kicking
  // off the timers.
  font->StartLoadLimitTimersIfNecessary(
      execution_context->GetTaskRunner(TaskType::kInternalLoading).get());

  face_->DidBeginLoad();
}

void RemoteFontFaceSource::Trace(Visitor* visitor) const {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  visitor->Trace(custom_font_data_);
  CSSFontFaceSource::Trace(visitor);
  FontResourceClient::Trace(visitor);
}

void RemoteFontFaceSource::FontLoadHistograms::LoadStarted() {
  if (load_start_time_.is_null()) {
    load_start_time_ = base::TimeTicks::Now();
  }
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

bool RemoteFontFaceSource::IsPendingDataUrl() const {
  return GetResource() && GetResource()->Url().ProtocolIsData();
}

void RemoteFontFaceSource::PaintRequested() {
  // The function must not be called after the font is loaded.
  DCHECK(!IsLoaded());
  paint_requested_while_pending_ = true;
  histograms_.FallbackFontPainted(period_);
}

void RemoteFontFaceSource::FontLoadHistograms::RecordFallbackTime() {
  if (blank_paint_time_.is_null() || blank_paint_time_recorded_) {
    return;
  }
  // TODO(https://crbug.com/1049257): This time should be recorded using a more
  // appropriate UMA helper, since >1% of samples are in the overflow bucket.
  base::TimeDelta duration = base::TimeTicks::Now() - blank_paint_time_;
  base::UmaHistogramTimes("WebFont.BlankTextShownTime", duration);
  blank_paint_time_recorded_ = true;
}

void RemoteFontFaceSource::FontLoadHistograms::RecordRemoteFont(
    const FontResource* font) {
  MaySetDataSource(DataSourceForLoadFinish(font));

  base::UmaHistogramEnumeration("WebFont.CacheHit", DataSourceMetricsValue());

  if (data_source_ == kFromDiskCache || data_source_ == kFromNetwork) {
    DCHECK(!load_start_time_.is_null());
    RecordLoadTimeHistogram(font, base::TimeTicks::Now() - load_start_time_);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::MaySetDataSource(
    DataSource data_source) {
  if (data_source_ != kFromUnknown) {
    return;
  }
  // Classify as memory cache hit if |load_start_time_| is not set, i.e.
  // this RemoteFontFaceSource instance didn't trigger FontResource
  // loading.
  if (load_start_time_.is_null()) {
    data_source_ = kFromMemoryCache;
  } else {
    data_source_ = data_source;
  }
}

void RemoteFontFaceSource::FontLoadHistograms::RecordLoadTimeHistogram(
    const FontResource* font,
    base::TimeDelta delta) {
  CHECK_NE(kFromUnknown, data_source_);

  // TODO(https://crbug.com/1049257): These times should be recorded using a
  // more appropriate UMA helper, since >1% of samples are in the overflow
  // bucket.
  if (font->ErrorOccurred()) {
    base::UmaHistogramTimes("WebFont.DownloadTime.LoadError", delta);
    return;
  }
}

RemoteFontFaceSource::FontLoadHistograms::CacheHitMetrics
RemoteFontFaceSource::FontLoadHistograms::DataSourceMetricsValue() {
  switch (data_source_) {
    case kFromDataURL:
      return CacheHitMetrics::kDataUrl;
    case kFromMemoryCache:
      return CacheHitMetrics::kMemoryHit;
    case kFromDiskCache:
      return CacheHitMetrics::kDiskHit;
    case kFromNetwork:
      return CacheHitMetrics::kMiss;
    case kFromUnknown:
      return CacheHitMetrics::kMiss;
  }
  NOTREACHED_IN_MIGRATION();
  return CacheHitMetrics::kMiss;
}

}  // namespace blink
