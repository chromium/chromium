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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_tracker.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
namespace blink {

static unsigned long long ToIntegerMilliseconds(TimeDelta duration) {
  // TODO(npm): add histograms to understand when/why |duration| is sometimes
  // negative.
  double clamped_seconds =
      Performance::ClampTimeResolution(duration.InSecondsF());
  return static_cast<unsigned long long>(clamped_seconds * 1000.0);
}

PerformanceTiming::PerformanceTiming(LocalFrame* frame)
    : DOMWindowClient(frame) {}

unsigned long long PerformanceTiming::navigationStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->NavigationStart());
}

unsigned long long PerformanceTiming::inputStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->InputStart());
}

unsigned long long PerformanceTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventStart());
}

unsigned long long PerformanceTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventEnd());
}

unsigned long long PerformanceTiming::redirectStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectStart());
}

unsigned long long PerformanceTiming::redirectEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectEnd());
}

unsigned long long PerformanceTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FetchStart());
}

unsigned long long PerformanceTiming::domainLookupStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return fetchStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // fetchStart.
  TimeTicks dns_start = timing->DnsStart();
  if (dns_start.is_null())
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(dns_start);
}

unsigned long long PerformanceTiming::domainLookupEnd() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return domainLookupStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // domainLookupStart.
  TimeTicks dns_end = timing->DnsEnd();
  if (dns_end.is_null())
    return domainLookupStart();

  return MonotonicTimeToIntegerMilliseconds(dns_end);
}

unsigned long long PerformanceTiming::connectStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return domainLookupEnd();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return domainLookupEnd();

  // connectStart will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with domainLookupEnd.
  TimeTicks connect_start = timing->ConnectStart();
  if (connect_start.is_null() || loader->GetResponse().ConnectionReused())
    return domainLookupEnd();

  // ResourceLoadTiming's connect phase includes DNS, however Navigation
  // Timing's connect phase should not. So if there is DNS time, trim it from
  // the start.
  if (!timing->DnsEnd().is_null() && timing->DnsEnd() > connect_start)
    connect_start = timing->DnsEnd();

  return MonotonicTimeToIntegerMilliseconds(connect_start);
}

unsigned long long PerformanceTiming::connectEnd() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return connectStart();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return connectStart();

  // connectEnd will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with connectStart.
  TimeTicks connect_end = timing->ConnectEnd();
  if (connect_end.is_null() || loader->GetResponse().ConnectionReused())
    return connectStart();

  return MonotonicTimeToIntegerMilliseconds(connect_end);
}

unsigned long long PerformanceTiming::secureConnectionStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return 0;

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return 0;

  TimeTicks ssl_start = timing->SslStart();
  if (ssl_start.is_null())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(ssl_start);
}

unsigned long long PerformanceTiming::requestStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();

  if (!timing || timing->SendStart().is_null())
    return connectEnd();

  return MonotonicTimeToIntegerMilliseconds(timing->SendStart());
}

unsigned long long PerformanceTiming::responseStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->ReceiveHeadersEnd().is_null())
    return requestStart();

  // FIXME: Response start needs to be the time of the first received byte.
  // However, the ResourceLoadTiming API currently only supports the time
  // the last header byte was received. For many responses with reasonable
  // sized cookies, the HTTP headers fit into a single packet so this time
  // is basically equivalent. But for some responses, particularly those with
  // headers larger than a single packet, this time will be too late.
  return MonotonicTimeToIntegerMilliseconds(timing->ReceiveHeadersEnd());
}

unsigned long long PerformanceTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ResponseEnd());
}

unsigned long long PerformanceTiming::domLoading() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(timing->DomLoading());
}

unsigned long long PerformanceTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomInteractive());
}

unsigned long long PerformanceTiming::domContentLoadedEventStart() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->DomContentLoadedEventStart());
}

unsigned long long PerformanceTiming::domContentLoadedEventEnd() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomContentLoadedEventEnd());
}

unsigned long long PerformanceTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomComplete());
}

unsigned long long PerformanceTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventStart());
}

unsigned long long PerformanceTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventEnd());
}

unsigned long long PerformanceTiming::FirstLayout() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstLayout());
}

unsigned long long PerformanceTiming::FirstPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstPaint());
}

unsigned long long PerformanceTiming::FirstTextPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstTextPaint());
}

unsigned long long PerformanceTiming::FirstImagePaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstImagePaint());
}

unsigned long long PerformanceTiming::FirstContentfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstContentfulPaint());
}

unsigned long long PerformanceTiming::FirstMeaningfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstMeaningfulPaint());
}

unsigned long long PerformanceTiming::FirstMeaningfulPaintCandidate() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->FirstMeaningfulPaintCandidate());
}

unsigned long long PerformanceTiming::LargestImagePaint() const {
  PaintTracker* paint_tracker = GetPaintTracker();
  if (!paint_tracker)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_tracker->GetImagePaintTimingDetector().LargestImagePaint());
}

unsigned long long PerformanceTiming::LastImagePaint() const {
  PaintTracker* paint_tracker = GetPaintTracker();
  if (!paint_tracker)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_tracker->GetImagePaintTimingDetector().LastImagePaint());
}

unsigned long long PerformanceTiming::LargestTextPaint() const {
  PaintTracker* paint_tracker = GetPaintTracker();
  if (!paint_tracker)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_tracker->GetTextPaintTimingDetector().LargestTextPaint());
}

unsigned long long PerformanceTiming::LastTextPaint() const {
  PaintTracker* paint_tracker = GetPaintTracker();
  if (!paint_tracker)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      paint_tracker->GetTextPaintTimingDetector().LastTextPaint());
}

unsigned long long PerformanceTiming::PageInteractive() const {
  InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      interactive_detector->GetInteractiveTime());
}

unsigned long long PerformanceTiming::PageInteractiveDetection() const {
  InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      interactive_detector->GetInteractiveDetectionTime());
}

unsigned long long PerformanceTiming::FirstInputInvalidatingInteractive()
    const {
  InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      interactive_detector->GetFirstInvalidatingInputTime());
}

unsigned long long PerformanceTiming::FirstInputDelay() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return ToIntegerMilliseconds(interactive_detector->GetFirstInputDelay());
}

unsigned long long PerformanceTiming::FirstInputTimestamp() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      interactive_detector->GetFirstInputTimestamp());
}

unsigned long long PerformanceTiming::LongestInputDelay() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return ToIntegerMilliseconds(interactive_detector->GetLongestInputDelay());
}

unsigned long long PerformanceTiming::LongestInputTimestamp() const {
  const InteractiveDetector* interactive_detector = GetInteractiveDetector();
  if (!interactive_detector)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      interactive_detector->GetLongestInputTimestamp());
}

unsigned long long PerformanceTiming::ParseStart() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStart());
}

unsigned long long PerformanceTiming::ParseStop() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStop());
}

unsigned long long PerformanceTiming::ParseBlockedOnScriptLoadDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->ParserBlockedOnScriptLoadDuration());
}

unsigned long long
PerformanceTiming::ParseBlockedOnScriptLoadFromDocumentWriteDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptLoadFromDocumentWriteDuration());
}

unsigned long long PerformanceTiming::ParseBlockedOnScriptExecutionDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionDuration());
}

unsigned long long
PerformanceTiming::ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionFromDocumentWriteDuration());
}

DocumentLoader* PerformanceTiming::GetDocumentLoader() const {
  if (!GetFrame())
    return nullptr;

  return GetFrame()->Loader().GetDocumentLoader();
}

const DocumentTiming* PerformanceTiming::GetDocumentTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &document->GetTiming();
}

const PaintTiming* PerformanceTiming::GetPaintTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &PaintTiming::From(*document);
}

const DocumentParserTiming* PerformanceTiming::GetDocumentParserTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &DocumentParserTiming::From(*document);
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
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return InteractiveDetector::From(*document);
}

PaintTracker* PerformanceTiming::GetPaintTracker() const {
  if (!GetFrame())
    return nullptr;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return nullptr;

  return &view->GetPaintTracker();
}

ScriptValue PerformanceTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("navigationStart", navigationStart());
  result.AddNumber("unloadEventStart", unloadEventStart());
  result.AddNumber("unloadEventEnd", unloadEventEnd());
  result.AddNumber("redirectStart", redirectStart());
  result.AddNumber("redirectEnd", redirectEnd());
  result.AddNumber("fetchStart", fetchStart());
  result.AddNumber("domainLookupStart", domainLookupStart());
  result.AddNumber("domainLookupEnd", domainLookupEnd());
  result.AddNumber("connectStart", connectStart());
  result.AddNumber("connectEnd", connectEnd());
  result.AddNumber("secureConnectionStart", secureConnectionStart());
  result.AddNumber("requestStart", requestStart());
  result.AddNumber("responseStart", responseStart());
  result.AddNumber("responseEnd", responseEnd());
  result.AddNumber("domLoading", domLoading());
  result.AddNumber("domInteractive", domInteractive());
  result.AddNumber("domContentLoadedEventStart", domContentLoadedEventStart());
  result.AddNumber("domContentLoadedEventEnd", domContentLoadedEventEnd());
  result.AddNumber("domComplete", domComplete());
  result.AddNumber("loadEventStart", loadEventStart());
  result.AddNumber("loadEventEnd", loadEventEnd());
  return result.GetScriptValue();
}

unsigned long long PerformanceTiming::MonotonicTimeToIntegerMilliseconds(
    TimeTicks time) const {
  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->MonotonicTimeToPseudoWallTime(time));
}

void PerformanceTiming::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
}

}  // namespace blink
