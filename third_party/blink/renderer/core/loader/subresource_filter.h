// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_FILTER_H_

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"

namespace blink {

class ExecutionContext;
class KURL;

// Wrapper around a WebDocumentSubresourceFilter. This class will make it easier
// to extend the subresource filter with optimizations only possible using blink
// types (e.g. a caching layer using StringImpl).
class CORE_EXPORT SubresourceFilter final
    : public GarbageCollected<SubresourceFilter> {
 public:
  static SubresourceFilter* Create(
      ExecutionContext&,
      std::unique_ptr<WebDocumentSubresourceFilter>);

  SubresourceFilter(ExecutionContext*,
                    std::unique_ptr<WebDocumentSubresourceFilter>);
  ~SubresourceFilter();

  bool AllowLoad(const KURL& resource_url,
                 mojom::RequestContextType,
                 SecurityViolationReportingPolicy);
  bool AllowWebSocketConnection(const KURL&);

  // Returns if |resource_url| is an ad resource.
  bool IsAdResource(const KURL& resource_url, mojom::RequestContextType);
  // Reports the resource request id as an ad to the |subresource_filter_|.
  void ReportAdRequestId(int request_id);

  virtual void Trace(blink::Visitor*);

 private:
  void ReportLoad(const KURL& resource_url,
                  WebDocumentSubresourceFilter::LoadPolicy);

  Member<ExecutionContext> execution_context_;
  std::unique_ptr<WebDocumentSubresourceFilter> subresource_filter_;

  // Save the last resource check's result in the single element cache.
  std::pair<std::pair<KURL, mojom::RequestContextType>,
            WebDocumentSubresourceFilter::LoadPolicy>
      last_resource_check_result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SUBRESOURCE_FILTER_H_
