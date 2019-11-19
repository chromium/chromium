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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/web/web_navigation_type.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/heap/handle.h"  // nogncheck
#endif

namespace blink {

class WindowPerformance;

class WebPerformance {
 public:
  ~WebPerformance() { Reset(); }

  WebPerformance() = default;

  WebPerformance(const WebPerformance& p) { Assign(p); }

  WebPerformance& operator=(const WebPerformance& p) {
    Assign(p);
    return *this;
  }

  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebPerformance&);

  // This only returns one of {Other|Reload|BackForward}.
  // Form submits and link clicks all fall under other.
  BLINK_EXPORT WebNavigationType GetNavigationType() const;

  // These functions return time in seconds (not milliseconds) since the epoch.
  BLINK_EXPORT double InputForNavigationStart() const;
  BLINK_EXPORT double NavigationStart() const;
  BLINK_EXPORT double UnloadEventEnd() const;
  BLINK_EXPORT double RedirectStart() const;
  BLINK_EXPORT double RedirectEnd() const;
  BLINK_EXPORT uint16_t RedirectCount() const;
  BLINK_EXPORT double FetchStart() const;
  BLINK_EXPORT double DomainLookupStart() const;
  BLINK_EXPORT double DomainLookupEnd() const;
  BLINK_EXPORT double ConnectStart() const;
  BLINK_EXPORT double ConnectEnd() const;
  BLINK_EXPORT double RequestStart() const;
  BLINK_EXPORT double ResponseStart() const;
  BLINK_EXPORT double ResponseEnd() const;
  BLINK_EXPORT double DomLoading() const;
  BLINK_EXPORT double DomInteractive() const;
  BLINK_EXPORT double DomContentLoadedEventStart() const;
  BLINK_EXPORT double DomContentLoadedEventEnd() const;
  BLINK_EXPORT double DomComplete() const;
  BLINK_EXPORT double LoadEventStart() const;
  BLINK_EXPORT double LoadEventEnd() const;
  BLINK_EXPORT double FirstLayout() const;
  BLINK_EXPORT double FirstPaint() const;
  BLINK_EXPORT double FirstImagePaint() const;
  BLINK_EXPORT double FirstContentfulPaint() const;
  BLINK_EXPORT double FirstMeaningfulPaint() const;
  BLINK_EXPORT double FirstMeaningfulPaintCandidate() const;
  BLINK_EXPORT double LargestImagePaint() const;
  BLINK_EXPORT uint64_t LargestImagePaintSize() const;
  BLINK_EXPORT double LargestTextPaint() const;
  BLINK_EXPORT uint64_t LargestTextPaintSize() const;
  BLINK_EXPORT double PageInteractive() const;
  BLINK_EXPORT double PageInteractiveDetection() const;
  BLINK_EXPORT double FirstInputInvalidatingInteractive() const;
  BLINK_EXPORT double FirstInputDelay() const;
  BLINK_EXPORT double FirstInputTimestamp() const;
  BLINK_EXPORT double LongestInputDelay() const;
  BLINK_EXPORT double LongestInputTimestamp() const;
  BLINK_EXPORT double ParseStart() const;
  BLINK_EXPORT double ParseStop() const;
  BLINK_EXPORT double ParseBlockedOnScriptLoadDuration() const;
  BLINK_EXPORT double ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  BLINK_EXPORT double ParseBlockedOnScriptExecutionDuration() const;
  BLINK_EXPORT double ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
      const;

#if INSIDE_BLINK
  BLINK_EXPORT WebPerformance(WindowPerformance*);
  BLINK_EXPORT WebPerformance& operator=(WindowPerformance*);
#endif

 private:
  WebPrivatePtr<WindowPerformance> private_;
};

}  // namespace blink

#endif
