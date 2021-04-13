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

void ReportAttributionIssue(LocalFrame* frame,
                            mojom::blink::AttributionReportingIssueType type,
                            Element* element,
                            const base::Optional<String>& request_id);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ATTRIBUTION_ISSUE_H_
