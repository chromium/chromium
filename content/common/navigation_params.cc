// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {

SourceLocation::SourceLocation() = default;

SourceLocation::SourceLocation(const std::string& url,
                               unsigned int line_number,
                               unsigned int column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

SourceLocation::~SourceLocation() = default;

InitiatorCSPInfo::InitiatorCSPInfo() = default;
InitiatorCSPInfo::InitiatorCSPInfo(
    CSPDisposition should_check_main_world_csp,
    const std::vector<ContentSecurityPolicy>& initiator_csp,
    const base::Optional<CSPSource>& initiator_self_source)
    : should_check_main_world_csp(should_check_main_world_csp),
      initiator_csp(initiator_csp),
      initiator_self_source(initiator_self_source) {}
InitiatorCSPInfo::InitiatorCSPInfo(const InitiatorCSPInfo& other) = default;

InitiatorCSPInfo::~InitiatorCSPInfo() = default;

mojom::CommonNavigationParamsPtr CreateCommonNavigationParams() {
  auto common_params = mojom::CommonNavigationParams::New();
  common_params->referrer = blink::mojom::Referrer::New();
  common_params->navigation_start = base::TimeTicks::Now();

  return common_params;
}

mojom::CommitNavigationParamsPtr CreateCommitNavigationParams() {
  auto commit_params = mojom::CommitNavigationParams::New();
  commit_params->navigation_token = base::UnguessableToken::Create();
  commit_params->navigation_timing = mojom::NavigationTiming::New();

  return commit_params;
}

}  // namespace content
