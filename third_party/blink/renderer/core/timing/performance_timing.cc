/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/performance_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
namespace blink {

static uint64_t ToIntegerMilliseconds(base::TimeDelta duration) {
  // TODO(npm): add histograms to understand when/why |duration| is sometimes
  // negative.
  // TODO(crbug.com/1063989): stop clamping when it is not needed (i.e. for
  // methods which do not expose the timestamp to a web perf API).
  double clamped_seconds =
      Performance::ClampTimeResolution(duration.InSecondsF());
  return static_cast<uint64_t>(clamped_seconds * 1000.0);
}

PerformanceTiming::PerformanceTiming(ExecutionContext* context)
    : ExecutionContextClient(context) {}

uint64_t PerformanceTiming::navigationStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->NavigationStart());
}

uint64_t PerformanceTiming::inputStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->InputStart());
}

uint64_t PerformanceTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventStart());
}

uint64_t PerformanceTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventEnd());
}

uint64_t PerformanceTiming::redirectStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectStart());
}

uint64_t PerformanceTiming::redirectEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectEnd());
}

uint64_t PerformanceTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FetchStart());
}

uint64_t PerformanceTiming::domainLookupStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return fetchStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // fetchStart.
  base::TimeTicks dns_start = timing->DnsStart();
  if (dns_start.is_null())
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(dns_start);
}

uint64_t PerformanceTiming::domainLookupEnd() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return domainLookupStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // domainLookupStart.
  base::TimeTicks dns_end = timing->DnsEnd();
  if (dns_end.is_null())
    return domainLookupStart();

  return MonotonicTimeToIntegerMilliseconds(dns_end);
}

uint64_t PerformanceTiming::connectStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return domainLookupEnd();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return domainLookupEnd();

  // connectStart will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with domainLookupEnd.
  base::TimeTicks connect_start = timing->ConnectStart();
  if (connect_start.is_null() || loader->GetResponse().ConnectionReused())
    return domainLookupEnd();

  // ResourceLoadTiming's connect phase includes DNS, however Navigation
  // Timing's connect phase should not. So if there is DNS time, trim it from
  // the start.
  if (!timing->DnsEnd().is_null() && timing->DnsEnd() > connect_start)
    connect_start = timing->DnsEnd();

  return MonotonicTimeToIntegerMilliseconds(connect_start);
}

uint64_t PerformanceTiming::connectEnd() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return connectStart();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return connectStart();

  // connectEnd will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with connectStart.
  base::TimeTicks connect_end = timing->ConnectEnd();
  if (connect_end.is_null() || loader->GetResponse().ConnectionReused())
    return connectStart();

  return MonotonicTimeToIntegerMilliseconds(connect_end);
}

uint64_t PerformanceTiming::secureConnectionStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return 0;

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return 0;

  base::TimeTicks ssl_start = timing->SslStart();
  if (ssl_start.is_null())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(ssl_start);
}

uint64_t PerformanceTiming::requestStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();

  if (!timing || timing->SendStart().is_null())
    return connectEnd();

  return MonotonicTimeToIntegerMilliseconds(timing->SendStart());
}

uint64_t PerformanceTiming::responseStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return requestStart();

  base::TimeTicks response_start = timing->ReceiveHeadersStart();
  if (response_start.is_null())
    response_start = timing->ReceiveHeadersEnd();
  if (response_start.is_null())
    return requestStart();

  return MonotonicTimeToIntegerMilliseconds(response_start);
}

uint64_t PerformanceTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ResponseEnd());
}

uint64_t PerformanceTiming::domLoading() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(timing->DomLoading());
}

uint64_t PerformanceTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomInteractive());
}

uint64_t PerformanceTiming::domContentLoadedEventStart() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->DomContentLoadedEventStart());
}

uint64_t PerformanceTiming::domContentLoadedEventEnd() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomContentLoadedEventEnd());
}

uint64_t PerformanceTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomComplete());
}

uint64_t PerformanceTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventStart());
}

uint64_t PerformanceTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventEnd());
}

base::TimeTicks PerformanceTiming::NavigationStartAsMonotonicTime() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return base::TimeTicks();

  return timing->NavigationStart();
}

PerformanceTiming::BackForwardCacheRestoreTimings
PerformanceTiming::BackForwardCacheRestore() const {
  DocumentLoadTiming* load_timing = GetDocumentLoadTiming();
  if (!load_timing)
    return {};

  const PaintTiming* paint_timing = GetPaintTiming();
  if (!paint_timing)
    return {};

  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return {};

  WTF::Vector<base::TimeTicks> navigation_starts =
      load_timing->BackForwardCacheRestoreNavigationStarts();
  WTF::Vector<base::TimeTicks> first_paints =
      paint_timing->FirstPaintsAfterBackForwardCacheRestore();
  WTF::Vector<base::Optional<base::TimeDelta>> first_input_delays =
      interactive_detector->GetFirstInputDelaysAfterBackForwardCacheRestore();
  DCHECK_EQ(navigation_starts.size(), first_paints.size());
  DCHECK_EQ(navigation_starts.size(), first_input_delays.size());

  WTF::Vector<BackForwardCacheRestoreTiming> restore_timings(
      navigation_starts.size());
  for (size_t i = 0; i < restore_timings.size(); i++) {
    restore_timings[i].navigation_start =
        MonotonicTimeToIntegerMilliseconds(navigation_starts[i]);
    restore_timings[i].first_paint =
        MonotonicTimeToIntegerMilliseconds(first_paints[i]);
    restore_timings[i].first_input_delay = first_input_delays[i];
  }
  return restore_timings;
}

uint64_t PerformanceTiming::FirstPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstPaint());
}

uint64_t PerformanceTiming::FirstImagePaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstImagePaint());
}

uint64_t PerformanceTiming::FirstContentfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstContentfulPaint());
}

base::TimeTicks PerformanceTiming::FirstContentfulPaintAsMonotonicTime() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return base::TimeTicks();

  return timing->FirstContentfulPaint();
}

uint64_t PerformanceTiming::FirstMeaningfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstMeaningfulPaint());
}

uint64_t PerformanceTiming::FirstMeaningfulPaintCandidate() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->FirstMeaningfulPaintCandidate());
}

uint64_t PerformanceTiming::LargestImagePaint() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->LargestImagePaint());
}

uint64_t PerformanceTiming::LargestImagePaintSize() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return paint_timing_detector->LargestImagePaintSize();
}

uint64_t PerformanceTiming::LargestTextPaint() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->LargestTextPaint());
}

uint64_t PerformanceTiming::LargestTextPaintSize() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return paint_timing_detector->LargestTextPaintSize();
}

uint64_t PerformanceTiming::ExperimentalLargestImagePaint() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->ExperimentalLargestImagePaint());
}

uint64_t PerformanceTiming::ExperimentalLargestImagePaintSize() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return paint_timing_detector->ExperimentalLargestImagePaintSize();
}

uint64_t PerformanceTiming::ExperimentalLargestTextPaint() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->ExperimentalLargestTextPaint());
}

uint64_t PerformanceTiming::ExperimentalLargestTextPaintSize() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return paint_timing_detector->ExperimentalLargestTextPaintSize();
}

base::TimeTicks PerformanceTiming::LargestContentfulPaintAsMonotonicTime()
    const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return base::TimeTicks();

  return paint_timing_detector->LargestContentfulPaint();
}

uint64_t PerformanceTiming::FirstEligibleToPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstEligibleToPaint());
}

uint64_t PerformanceTiming::FirstInputOrScrollNotifiedTimestamp() const {
  PaintTimingDetector* paint_timing_detector = GetPaintTimingDetector();
  if (!paint_timing_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_timing_detector->FirstInputOrScrollNotifiedTimestamp());
}

base::Optional<base::TimeDelta> PerformanceTiming::FirstInputDelay() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return interactive_detector->GetFirstInputDelay();
}

base::Optional<base::TimeDelta> PerformanceTiming::FirstInputTimestamp() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return MonotonicTimeToPseudoWallTime(
      interactive_detector->GetFirstInputTimestamp());
}

base::Optional<base::TimeDelta> PerformanceTiming::LongestInputDelay() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return interactive_detector->GetLongestInputDelay();
}

base::Optional<base::TimeDelta> PerformanceTiming::LongestInputTimestamp()
    const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return MonotonicTimeToPseudoWallTime(
      interactive_detector->GetLongestInputTimestamp());
}

base::Optional<base::TimeDelta> PerformanceTiming::FirstInputProcessingTime()
    const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return interactive_detector->GetFirstInputProcessingTime();
}

base::Optional<base::TimeDelta> PerformanceTiming::FirstScrollDelay() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return interactive_detector->GetFirstScrollDelay();
}

base::Optional<base::TimeDelta> PerformanceTiming::FirstScrollTimestamp()
    const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return base::nullopt;

  return MonotonicTimeToPseudoWallTime(
      interactive_detector->GetFirstScrollTimestamp());
}

uint64_t PerformanceTiming::ParseStart() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStart());
}

uint64_t PerformanceTiming::ParseStop() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStop());
}

uint64_t PerformanceTiming::ParseBlockedOnScriptLoadDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->ParserBlockedOnScriptLoadDuration());
}

uint64_t PerformanceTiming::ParseBlockedOnScriptLoadFromDocumentWriteDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptLoadFromDocumentWriteDuration());
}

uint64_t PerformanceTiming::ParseBlockedOnScriptExecutionDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionDuration());
}

uint64_t
PerformanceTiming::ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionFromDocumentWriteDuration());
}

base::Optional<base::TimeTicks> PerformanceTiming::LastPortalActivatedPaint()
    const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return base::nullopt;

  return timing->LastPortalActivatedPaint();
}

DocumentLoader* PerformanceTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->GetFrame()->Loader().GetDocumentLoader()
                     : nullptr;
}

const DocumentTiming* PerformanceTiming::GetDocumentTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &DomWindow()->document()->GetTiming();
}

const PaintTiming* PerformanceTiming::GetPaintTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &PaintTiming::From(*DomWindow()->document());
}

const DocumentParserTiming* PerformanceTiming::GetDocumentParserTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &DocumentParserTiming::From(*DomWindow()->document());
}

DocumentLoadTiming* PerformanceTiming::GetDocumentLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

ResourceLoadTiming* PerformanceTiming::GetResourceLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return loader->GetResponse().GetResourceLoadTiming();
}

InteractiveDetector* PerformanceTiming::GetInteractiveDetector() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return InteractiveDetector::From(*DomWindow()->document());
}

PaintTimingDetector* PerformanceTiming::GetPaintTimingDetector() const {
  if (!DomWindow())
    return nullptr;
  return &DomWindow()->GetFrame()->View()->GetPaintTimingDetector();
}

base::Optional<base::TimeDelta>
PerformanceTiming::MonotonicTimeToPseudoWallTime(
    const base::Optional<base::TimeTicks>& time) const {
  if (!time.has_value())
    return base::nullopt;

  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return base::nullopt;

  return timing->MonotonicTimeToPseudoWallTime(*time);
}

std::unique_ptr<TracedValue> PerformanceTiming::GetNavigationTracingData() {
  auto data = std::make_unique<TracedValue>();
  data->SetString("navigationId",
                  IdentifiersFactory::LoaderId(GetDocumentLoader()));
  return data;
}

// static
const PerformanceTiming::NameToAttributeMap&
PerformanceTiming::GetAttributeMapping() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<NameToAttributeMap>, map, ());
  if (!map.IsSet()) {
    *map = {
        {"navigationStart", &PerformanceTiming::navigationStart},
        {"unloadEventStart", &PerformanceTiming::unloadEventStart},
        {"unloadEventEnd", &PerformanceTiming::unloadEventEnd},
        {"redirectStart", &PerformanceTiming::redirectStart},
        {"redirectEnd", &PerformanceTiming::redirectEnd},
        {"fetchStart", &PerformanceTiming::fetchStart},
        {"domainLookupStart", &PerformanceTiming::domainLookupStart},
        {"domainLookupEnd", &PerformanceTiming::domainLookupEnd},
        {"connectStart", &PerformanceTiming::connectStart},
        {"connectEnd", &PerformanceTiming::connectEnd},
        {"secureConnectionStart", &PerformanceTiming::secureConnectionStart},
        {"requestStart", &PerformanceTiming::requestStart},
        {"responseStart", &PerformanceTiming::responseStart},
        {"responseEnd", &PerformanceTiming::responseEnd},
        {"domLoading", &PerformanceTiming::domLoading},
        {"domInteractive", &PerformanceTiming::domInteractive},
        {"domContentLoadedEventStart",
         &PerformanceTiming::domContentLoadedEventStart},
        {"domContentLoadedEventEnd",
         &PerformanceTiming::domContentLoadedEventEnd},
        {"domComplete", &PerformanceTiming::domComplete},
        {"loadEventStart", &PerformanceTiming::loadEventStart},
        {"loadEventEnd", &PerformanceTiming::loadEventEnd},
    };
  }
  return *map;
}

ScriptValue PerformanceTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  for (const auto& name_attribute_pair : GetAttributeMapping()) {
    result.AddNumber(name_attribute_pair.key,
                     (this->*(name_attribute_pair.value))());
  }
  return result.GetScriptValue();
}

uint64_t PerformanceTiming::MonotonicTimeToIntegerMilliseconds(
    base::TimeTicks time) const {
  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->MonotonicTimeToPseudoWallTime(time));
}

void PerformanceTiming::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
