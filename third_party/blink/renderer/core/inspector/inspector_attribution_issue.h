// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ATTRIBUTION_ISSUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ATTRIBUTION_ISSUE_H_

#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;
class LocalFrame;

// Reports an Attribution Reporting API issue to DevTools.
// |reporting_frame| is the current execution context in which the issue
// happens and is reported in (the "target" in DevTools terms).
// |offending_frame_token| is the offending frame that triggered the issue.
// |offending_frame_token| does not necessarly correspond to |reporting_frame|,
// e.g. when an impression click in an iframe is blocked due to an
// insecure main frame.
void ReportAttributionIssue(
    LocalFrame* reporting_frame,
    mojom::blink::AttributionReportingIssueType type,
    const base::Optional<base::UnguessableToken>& offending_frame_token =
        base::nullopt,
    Element* element = nullptr,
    const base::Optional<String>& request_id = base::nullopt,
    const base::Optional<String>& invalid_parameter = base::nullopt);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ATTRIBUTION_ISSUE_H_
