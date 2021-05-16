// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue_reporter.h"

#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"

namespace blink {

InspectorIssueReporter::InspectorIssueReporter(InspectorIssueStorage* storage)
    : storage_(storage) {}

InspectorIssueReporter::~InspectorIssueReporter() = default;

void InspectorIssueReporter::Trace(Visitor* visitor) const {
}

void InspectorIssueReporter::DidFailLoading(
    CoreProbeSink* sink,
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceError& error,
    const base::UnguessableToken& token) {
  if (!storage_)
    return;
  absl::optional<network::mojom::BlockedByResponseReason>
      blocked_by_response_reason = error.GetBlockedByResponseReason();
  if (!blocked_by_response_reason)
    return;
  auto blocked_by_response_details =
      mojom::blink::BlockedByResponseIssueDetails::New();
  blocked_by_response_details->reason = *blocked_by_response_reason;
  auto affected_request = mojom::blink::AffectedRequest::New();
  affected_request->request_id =
      IdentifiersFactory::RequestId(loader, identifier);
  affected_request->url = error.FailingURL();
  blocked_by_response_details->request = std::move(affected_request);

  auto affected_frame = mojom::blink::AffectedFrame::New();
  affected_frame->frame_id = IdentifiersFactory::IdFromToken(token);
  blocked_by_response_details->parentFrame = std::move(affected_frame);

  auto details = mojom::blink::InspectorIssueDetails::New();
  details->blocked_by_response_issue_details =
      std::move(blocked_by_response_details);

  storage_->AddInspectorIssue(
      sink, mojom::blink::InspectorIssueInfo::New(
                mojom::blink::InspectorIssueCode::kBlockedByResponseIssue,
                std::move(details)));
}

void InspectorIssueReporter::DomContentLoadedEventFired(LocalFrame* frame) {
  if (!frame)
    return;

  auto* document = frame->GetDocument();
  if (!document || !document->GetExecutionContext())
    return;

  auto url = document->Url();
  if (url.IsEmpty() || url.IsAboutBlankURL())
    return;

  if (document->InNoQuirksMode())
    return;

  AuditsIssue::ReportQuirksModeIssue(
      document->GetExecutionContext(), document->InLimitedQuirksMode(),
      DOMNodeIds::IdForNode(document), url.GetString(),
      IdentifiersFactory::FrameId(frame),
      IdentifiersFactory::LoaderId(document->Loader()));
}

}  // namespace blink
