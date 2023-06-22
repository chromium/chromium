// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
namespace blink {

static uint64_t ToIntegerMilliseconds(base::TimeDelta duration,
                                      bool cross_origin_isolated_capability) {
  // TODO(npm): add histograms to understand when/why |duration| is sometimes
  // negative.
  // TODO(crbug.com/1063989): stop clamping when it is not needed (i.e. for
  // methods which do not expose the timestamp to a web perf API).
  return static_cast<uint64_t>(Performance::ClampTimeResolution(
      duration, cross_origin_isolated_capability));
}

PerformanceTiming::PerformanceTiming(ExecutionContext* context)
    : ExecutionContextClient(context) {
  cross_origin_isolated_capability_ =
      context && context->CrossOriginIsolatedCapability();
}

uint64_t PerformanceTiming::navigationStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->NavigationStart());
}

uint64_t PerformanceTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->CanRequestFromPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventStart());
}

uint64_t PerformanceTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->CanRequestFromPreviousDocument())
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
  base::TimeTicks domain_lookup_start = timing->DomainLookupStart();
  if (domain_lookup_start.is_null())
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(domain_lookup_start);
}

uint64_t PerformanceTiming::domainLookupEnd() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return domainLookupStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // domainLookupStart.
  base::TimeTicks domain_lookup_end = timing->DomainLookupEnd();
  if (domain_lookup_end.is_null())
    return domainLookupStart();

  return MonotonicTimeToIntegerMilliseconds(domain_lookup_end);
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
  if (!timing->DomainLookupEnd().is_null() &&
      timing->DomainLookupEnd() > connect_start) {
    connect_start = timing->DomainLookupEnd();
  }

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

DocumentLoader* PerformanceTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->GetFrame()->Loader().GetDocumentLoader()
                     : nullptr;
}

const DocumentTiming* PerformanceTiming::GetDocumentTiming() const {
  if (!DomWindow() || !DomWindow()->document())
    return nullptr;
  return &DomWindow()->document()->GetTiming();
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

void PerformanceTiming::WriteInto(perfetto::TracedDictionary& dict) const {
  dict.Add("navigationId", IdentifiersFactory::LoaderId(GetDocumentLoader()));
}

// static
bool PerformanceTiming::IsAttributeName(const AtomicString& name) {
  return GetAttributeMapping().Contains(name);
}

uint64_t PerformanceTiming::GetNamedAttribute(const AtomicString& name) const {
  DCHECK(IsAttributeName(name)) << "The string passed as parameter must be an "
                                   "attribute of performance.timing";
  PerformanceTimingGetter fn = GetAttributeMapping().at(name);
  return (this->*fn)();
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

  return ToIntegerMilliseconds(timing->MonotonicTimeToPseudoWallTime(time),
                               cross_origin_isolated_capability_);
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

void PerformanceTiming::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
