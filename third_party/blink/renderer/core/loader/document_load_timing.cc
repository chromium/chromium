/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/document_load_timing.h"

#include "base/memory/scoped_refptr.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DocumentLoadTiming::DocumentLoadTiming(DocumentLoader& document_loader)
    : redirect_count_(0),
      has_cross_origin_redirect_(false),
      has_same_origin_as_previous_document_(false),
      clock_(base::DefaultClock::GetInstance()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      document_loader_(document_loader) {}

void DocumentLoadTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_loader_);
}

void DocumentLoadTiming::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void DocumentLoadTiming::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

// TODO(csharrison): Remove the null checking logic in a later patch.
LocalFrame* DocumentLoadTiming::GetFrame() const {
  return document_loader_ ? document_loader_->GetFrame() : nullptr;
}

void DocumentLoadTiming::NotifyDocumentTimingChanged() {
  if (document_loader_)
    document_loader_->DidChangePerformanceTiming();
}

void DocumentLoadTiming::EnsureReferenceTimesSet() {
  if (reference_wall_time_.is_zero()) {
    reference_wall_time_ =
        base::TimeDelta::FromSecondsD(clock_->Now().ToDoubleT());
  }
  if (reference_monotonic_time_.is_null())
    reference_monotonic_time_ = tick_clock_->NowTicks();
}

base::TimeDelta DocumentLoadTiming::MonotonicTimeToZeroBasedDocumentTime(
    base::TimeTicks monotonic_time) const {
  if (monotonic_time.is_null() || reference_monotonic_time_.is_null())
    return base::TimeDelta();
  return monotonic_time - reference_monotonic_time_;
}

base::TimeDelta DocumentLoadTiming::MonotonicTimeToPseudoWallTime(
    base::TimeTicks monotonic_time) const {
  if (monotonic_time.is_null() || reference_monotonic_time_.is_null())
    return base::TimeDelta();
  return monotonic_time + reference_wall_time_ - reference_monotonic_time_;
}

void DocumentLoadTiming::MarkNavigationStart() {
  // Allow the embedder to override navigationStart before we record it if
  // they have a more accurate timestamp.
  if (!navigation_start_.is_null()) {
    DCHECK(!reference_monotonic_time_.is_null());
    DCHECK(!reference_wall_time_.is_zero());
    return;
  }
  DCHECK(reference_monotonic_time_.is_null());
  DCHECK(reference_wall_time_.is_zero());
  EnsureReferenceTimesSet();
  navigation_start_ = reference_monotonic_time_;
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "blink.user_timing", "navigationStart", navigation_start_, "frame",
      ToTraceValue(GetFrame()), "data", GetNavigationStartTracingData());
  NotifyDocumentTimingChanged();
}

std::unique_ptr<TracedValue> DocumentLoadTiming::GetNavigationStartTracingData()
    const {
  auto data = std::make_unique<TracedValue>();
  data->SetString("documentLoaderURL",
                  document_loader_ ? document_loader_->Url().GetString() : "");
  data->SetBoolean("isLoadingMainFrame",
                   GetFrame() ? GetFrame()->IsMainFrame() : false);
  data->SetString("navigationId",
                  IdentifiersFactory::LoaderId(document_loader_));
  return data;
}

void DocumentLoadTiming::SetNavigationStart(base::TimeTicks navigation_start) {
  // |m_referenceMonotonicTime| and |m_referenceWallTime| represent
  // navigationStart. We must set these to the current time if they haven't
  // been set yet in order to have a valid reference time in both units.
  EnsureReferenceTimesSet();
  navigation_start_ = navigation_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "blink.user_timing", "navigationStart", navigation_start_, "frame",
      ToTraceValue(GetFrame()), "data", GetNavigationStartTracingData());

  // The reference times are adjusted based on the embedder's navigationStart.
  DCHECK(!reference_monotonic_time_.is_null());
  DCHECK(!reference_wall_time_.is_zero());
  reference_wall_time_ = MonotonicTimeToPseudoWallTime(navigation_start);
  reference_monotonic_time_ = navigation_start;
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetInputStart(base::TimeTicks input_start) {
  input_start_ = input_start;
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::AddRedirect(const KURL& redirecting_url,
                                     const KURL& redirected_url) {
  redirect_count_++;

  // Note: we update load timings for redirects in WebDocumentLoaderImpl::
  // UpdateNavigation, hence updating no timings here.

  // Check if the redirected url is allowed to access the redirecting url's
  // timing information.
  scoped_refptr<const SecurityOrigin> redirected_security_origin =
      SecurityOrigin::Create(redirected_url);
  has_cross_origin_redirect_ |=
      !redirected_security_origin->CanRequest(redirecting_url);
}

void DocumentLoadTiming::SetRedirectStart(base::TimeTicks redirect_start) {
  redirect_start_ = redirect_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectStart",
                                   redirect_start_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetRedirectEnd(base::TimeTicks redirect_end) {
  redirect_end_ = redirect_end;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectEnd",
                                   redirect_end_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkUnloadEventStart(base::TimeTicks start_time) {
  unload_event_start_ = start_time;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventStart",
                                   start_time, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkUnloadEventEnd(base::TimeTicks end_time) {
  unload_event_end_ = end_time;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventEnd",
                                   end_time, "frame", ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkFetchStart() {
  SetFetchStart(tick_clock_->NowTicks());
}

void DocumentLoadTiming::SetFetchStart(base::TimeTicks fetch_start) {
  fetch_start_ = fetch_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "fetchStart",
                                   fetch_start_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetResponseEnd(base::TimeTicks response_end) {
  response_end_ = response_end;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "responseEnd",
                                   response_end_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkLoadEventStart() {
  load_event_start_ = tick_clock_->NowTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "loadEventStart",
                                   load_event_start_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkLoadEventEnd() {
  load_event_end_ = tick_clock_->NowTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "loadEventEnd",
                                   load_event_end_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkRedirectEnd() {
  redirect_end_ = tick_clock_->NowTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectEnd",
                                   redirect_end_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

}  // namespace blink
