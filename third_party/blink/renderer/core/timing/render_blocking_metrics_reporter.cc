// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/render_blocking_metrics_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

namespace blink {

const char RenderBlockingMetricsReporter::kSupplementName[] =
    "RenderBlockingMetricsReporter";

RenderBlockingMetricsReporter::RenderBlockingMetricsReporter(Document& document)
    : Supplement<Document>(document) {}

// static
RenderBlockingMetricsReporter& RenderBlockingMetricsReporter::From(
    Document& document) {
  RenderBlockingMetricsReporter* supplement =
      Supplement<Document>::From<RenderBlockingMetricsReporter>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<RenderBlockingMetricsReporter>(document);
    ProvideTo(document, supplement);
  }
  return *supplement;
}

void RenderBlockingMetricsReporter::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

base::TimeDelta RenderBlockingMetricsReporter::GetDeltaFromTimeOrigin() {
  Document* document = GetSupplementable();
  DCHECK(document);
  LocalDOMWindow* window = document->domWindow();
  if (!window) {
    return base::TimeDelta();
  }
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  return (base::TimeTicks::Now() - performance->GetTimeOriginInternal());
}

void RenderBlockingMetricsReporter::Report() {
  //  If we were to wait on preloaded fonts as critical, how long would it block
  //  rendering?
  base::TimeDelta critical_font_delay =
      (preloaded_fonts_loaded_timestamp_ >
       render_blocking_resources_loaded_timestamp_)
          ? preloaded_fonts_loaded_timestamp_ -
                render_blocking_resources_loaded_timestamp_
          : base::TimeDelta();
  base::UmaHistogramTimes("Renderer.CriticalFonts.CriticalFontDelay",
                          critical_font_delay);
  base::UmaHistogramTimes("Renderer.CriticalFonts.BlockingResourcesLoadTime",
                          render_blocking_resources_loaded_timestamp_);
  base::UmaHistogramTimes("Renderer.CriticalFonts.PreloadedFontsLoadTime",
                          preloaded_fonts_loaded_timestamp_);
}

void RenderBlockingMetricsReporter::RenderBlockingResourcesLoaded() {
  DCHECK(!render_blocking_resources_reported_);
  render_blocking_resources_reported_ = true;
  render_blocking_resources_loaded_timestamp_ = GetDeltaFromTimeOrigin();
  if (preloaded_fonts_reported_) {
    Report();
  }
}
void RenderBlockingMetricsReporter::PreloadedFontStartedLoading() {
  ++pending_preloaded_fonts_;
}
void RenderBlockingMetricsReporter::PreloadedFontFinishedLoading() {
  --pending_preloaded_fonts_;
  if (!pending_preloaded_fonts_ && !preloaded_fonts_reported_) {
    preloaded_fonts_reported_ = true;
    preloaded_fonts_loaded_timestamp_ = GetDeltaFromTimeOrigin();
    if (render_blocking_resources_reported_) {
      Report();
    }
  }
}

}  // namespace blink
