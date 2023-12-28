// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"

#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

static const unsigned kMaxIssueCount = 1000;

InspectorIssueStorage::InspectorIssueStorage() = default;
InspectorIssueStorage::~InspectorIssueStorage() = default;

void InspectorIssueStorage::AddInspectorIssue(
    CoreProbeSink* sink,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  DCHECK(issues_.size() <= kMaxIssueCount);
  probe::InspectorIssueAdded(sink, issue.get());
  if (issues_.size() == kMaxIssueCount) {
    issues_.pop_front();
  }
  issues_.push_back(std::move(issue));
}

void InspectorIssueStorage::AddInspectorIssue(CoreProbeSink* sink,
                                              AuditsIssue issue) {
  AddInspectorIssue(sink, issue.TakeIssue());
}

void InspectorIssueStorage::AddInspectorIssue(ExecutionContext* context,
                                              AuditsIssue issue) {
  AddInspectorIssue(probe::ToCoreProbeSink(context), issue.TakeIssue());
}

void InspectorIssueStorage::Clear() {
  issues_.clear();
}

wtf_size_t InspectorIssueStorage::size() const {
  return issues_.size();
}

protocol::Audits::InspectorIssue* InspectorIssueStorage::at(
    wtf_size_t index) const {
  return issues_[index].get();
}

}  // namespace blink
