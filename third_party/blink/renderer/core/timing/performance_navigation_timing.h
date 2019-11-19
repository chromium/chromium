// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_NAVIGATION_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_NAVIGATION_TIMING_H_

#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

namespace blink {

class Document;
class DocumentTiming;
class DocumentLoader;
class DocumentLoadTiming;
class LocalFrame;
class ExecutionContext;
class ResourceTimingInfo;
class ResourceLoadTiming;

class CORE_EXPORT PerformanceNavigationTiming final
    : public PerformanceResourceTiming,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceNavigationTiming);
  friend class PerformanceNavigationTimingTest;

 public:
  PerformanceNavigationTiming(LocalFrame*,
                              ResourceTimingInfo*,
                              base::TimeTicks time_origin,
                              const WebVector<WebServerTimingInfo>&);

  // Attributes inheritted from PerformanceEntry.
  DOMHighResTimeStamp duration() const override;
  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  AtomicString initiatorType() const override;

  // PerformanceNavigationTiming's unique attributes.
  DOMHighResTimeStamp unloadEventStart() const;
  DOMHighResTimeStamp unloadEventEnd() const;
  DOMHighResTimeStamp domInteractive() const;
  DOMHighResTimeStamp domContentLoadedEventStart() const;
  DOMHighResTimeStamp domContentLoadedEventEnd() const;
  DOMHighResTimeStamp domComplete() const;
  DOMHighResTimeStamp loadEventStart() const;
  DOMHighResTimeStamp loadEventEnd() const;
  AtomicString type() const;
  uint16_t redirectCount() const;

  // PerformanceResourceTiming overrides:
  DOMHighResTimeStamp fetchStart() const override;
  DOMHighResTimeStamp redirectStart() const override;
  DOMHighResTimeStamp redirectEnd() const override;
  DOMHighResTimeStamp responseEnd() const override;

  void Trace(blink::Visitor*) override;

 protected:
  void BuildJSONValue(V8ObjectBuilder&) const override;

 private:
  ~PerformanceNavigationTiming() override;

  static AtomicString GetNavigationType(WebNavigationType, const Document*);

  const DocumentTiming* GetDocumentTiming() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;

  ResourceLoadTiming* GetResourceLoadTiming() const override;
  bool AllowTimingDetails() const override;
  bool DidReuseConnection() const override;
  uint64_t GetTransferSize() const override;
  uint64_t GetEncodedBodySize() const override;
  uint64_t GetDecodedBodySize() const override;

  bool GetAllowRedirectDetails() const;

  AtomicString AlpnNegotiatedProtocol() const override;
  AtomicString ConnectionInfo() const override;

  scoped_refptr<ResourceTimingInfo> resource_timing_info_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_NAVIGATION_TIMING_H_
