// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

#include "third_party/blink/renderer/core/inspector/protocol/Audits.h"

namespace blink {

AuditsIssue::AuditsIssue(
    std::unique_ptr<protocol::Audits::InspectorIssue> issue)
    : issue_(std::move(issue)) {}

AuditsIssue::AuditsIssue(AuditsIssue&&) = default;
AuditsIssue& AuditsIssue::operator=(AuditsIssue&&) = default;
AuditsIssue::~AuditsIssue() = default;

}  // namespace blink
