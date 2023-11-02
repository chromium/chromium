// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue.h"

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

}  // namespace blink
