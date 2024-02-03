// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue_reporter.h"

#include <optional>

#include "base/unguessable_token.h"
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
  std::optional<network::mojom::BlockedByResponseReason>
      blocked_by_response_reason = error.GetBlockedByResponseReason();
  if (!blocked_by_response_reason)
    return;

  auto issue = AuditsIssue::CreateBlockedByResponseIssue(
      *blocked_by_response_reason, identifier, loader, error, token);
  storage_->AddInspectorIssue(sink, std::move(issue));
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
      document->GetDomNodeId(), url.GetString(),
      IdentifiersFactory::FrameId(frame),
      IdentifiersFactory::LoaderId(document->Loader()));
}

}  // namespace blink
