// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/subresource_filter.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String GetErrorStringForDisallowedLoad(const KURL& url) {
  StringBuilder builder;
  builder.Append("Chrome blocked resource ");
  builder.Append(url.GetString());
  builder.Append(
      " on this site because this site tends to show ads that interrupt, "
      "distract, mislead, or prevent user control. Learn more at "
      "https://www.chromestatus.com/feature/5738264052891648");
  return builder.ToString();
}

}  // namespace

// static
SubresourceFilter* SubresourceFilter::Create(
    ExecutionContext& execution_context,
    std::unique_ptr<WebDocumentSubresourceFilter> filter) {
  return MakeGarbageCollected<SubresourceFilter>(&execution_context,
                                                 std::move(filter));
}

SubresourceFilter::SubresourceFilter(
    ExecutionContext* execution_context,
    std::unique_ptr<WebDocumentSubresourceFilter> subresource_filter)
    : execution_context_(execution_context),
      subresource_filter_(std::move(subresource_filter)) {
  DCHECK(subresource_filter_);
  // Report the main resource as an ad if the subresource filter is
  // associated with an ad subframe.
  if (auto* document = DynamicTo<Document>(execution_context_.Get())) {
    auto* loader = document->Loader();
    if (document->GetFrame()->IsAdSubframe()) {
      ReportAdRequestId(loader->GetResponse().RequestId());
    }
  }
}

SubresourceFilter::~SubresourceFilter() = default;

bool SubresourceFilter::AllowLoad(
    const KURL& resource_url,
    mojom::RequestContextType request_context,
    SecurityViolationReportingPolicy reporting_policy) {
  // TODO(csharrison): Implement a caching layer here which is a HashMap of
  // Pair<url string, context> -> LoadPolicy.
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicy(resource_url, request_context);

  if (reporting_policy == SecurityViolationReportingPolicy::kReport)
    ReportLoad(resource_url, load_policy);

  last_resource_check_result_ = std::make_pair(
      std::make_pair(resource_url, request_context), load_policy);

  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::AllowWebSocketConnection(const KURL& url) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicyForWebSocketConnect(url);

  // Post a task to notify this load to avoid unduly blocking the worker
  // thread. Note that this unconditionally calls reportLoad unlike allowLoad,
  // because there aren't developer-invisible connections (like speculative
  // preloads) happening here.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context_->GetTaskRunner(TaskType::kNetworking);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  task_runner->PostTask(
      FROM_HERE, WTF::Bind(&SubresourceFilter::ReportLoad, WrapPersistent(this),
                           url, load_policy));
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::IsAdResource(
    const KURL& resource_url,
    mojom::RequestContextType request_context) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy;
  if (last_resource_check_result_.first ==
      std::make_pair(resource_url, request_context)) {
    load_policy = last_resource_check_result_.second;
  } else {
    load_policy =
        subresource_filter_->GetLoadPolicy(resource_url, request_context);
  }

  return load_policy != WebDocumentSubresourceFilter::kAllow;
}

void SubresourceFilter::ReportAdRequestId(int request_id) {
  subresource_filter_->ReportAdRequestId(request_id);
}

void SubresourceFilter::ReportLoad(
    const KURL& resource_url,
    WebDocumentSubresourceFilter::LoadPolicy load_policy) {
  switch (load_policy) {
    case WebDocumentSubresourceFilter::kAllow:
      break;
    case WebDocumentSubresourceFilter::kDisallow:
      subresource_filter_->ReportDisallowedLoad();

      // Display console message for actually blocked resource. For a
      // resource with |load_policy| as kWouldDisallow, we will be logging a
      // document wide console message, so no need to log it here.
      // TODO: Consider logging this as a kIntervention for showing
      // warning in Lighthouse.
      if (subresource_filter_->ShouldLogToConsole()) {
        execution_context_->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kError,
            GetErrorStringForDisallowedLoad(resource_url)));
      }
      FALLTHROUGH;
    case WebDocumentSubresourceFilter::kWouldDisallow:
      // TODO(csharrison): Consider posting a task to the main thread from
      // worker thread, or adding support for DidObserveLoadingBehavior to
      // ExecutionContext.
      if (auto* document = DynamicTo<Document>(execution_context_.Get())) {
        if (DocumentLoader* loader = document->Loader()) {
          loader->DidObserveLoadingBehavior(
              kLoadingBehaviorSubresourceFilterMatch);
        }
      }
      break;
  }
}

void SubresourceFilter::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
