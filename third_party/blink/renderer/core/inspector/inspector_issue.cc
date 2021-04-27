// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_attribution_issue.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

InspectorIssue::InspectorIssue(mojom::blink::InspectorIssueCode code,
                               mojom::blink::InspectorIssueDetailsPtr details)
    : code_(code), details_(std::move(details)) {
  DCHECK(details_);
}

InspectorIssue::~InspectorIssue() = default;

InspectorIssue* InspectorIssue::Create(
    mojom::blink::InspectorIssueInfoPtr info) {
  DCHECK(info->details);
  return MakeGarbageCollected<InspectorIssue>(info->code,
                                              std::move(info->details));
}

mojom::blink::InspectorIssueCode InspectorIssue::Code() const {
  return code_;
}

const mojom::blink::InspectorIssueDetailsPtr& InspectorIssue::Details() const {
  return details_;
}

void InspectorIssue::Trace(blink::Visitor* visitor) const {}

void ReportAttributionIssue(
    LocalFrame* reporting_frame,
    mojom::blink::AttributionReportingIssueType type,
    const base::Optional<base::UnguessableToken>& offending_frame_token,
    Element* element,
    const base::Optional<String>& request_id,
    const base::Optional<String>& invalid_parameter) {
  auto attribution_issue = mojom::blink::AttributionReportingIssue::New();
  attribution_issue->violation_type = type;
  if (offending_frame_token) {
    attribution_issue->frame = mojom::blink::AffectedFrame::New(
        IdentifiersFactory::IdFromToken(*offending_frame_token));
  }
  if (element)
    attribution_issue->violating_node_id = DOMNodeIds::IdForNode(element);
  if (request_id) {
    auto affected_request = mojom::blink::AffectedRequest::New();
    affected_request->request_id = *request_id;
    attribution_issue->request = std::move(affected_request);
  }
  if (invalid_parameter)
    attribution_issue->invalid_parameter = *invalid_parameter;
  auto issue_details = mojom::blink::InspectorIssueDetails::New();
  issue_details->attribution_reporting_issue_details =
      std::move(attribution_issue);
  auto issue = mojom::blink::InspectorIssueInfo::New(
      mojom::blink::InspectorIssueCode::kAttributionReportingIssue,
      std::move(issue_details));
  reporting_frame->AddInspectorIssue(std::move(issue));
}

}  // namespace blink
