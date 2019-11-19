// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy/csp_disposition_enum.h"
#include "content/common/navigation_params.mojom-forward.h"
#include "content/common/prefetched_signed_exchange_info.mojom.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/was_activated_option.mojom.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Struct keeping track of the Javascript SourceLocation that triggered the
// navigation. This is initialized based on information from Blink at the start
// of navigation, and passed back to Blink when the navigation commits.
struct CONTENT_EXPORT SourceLocation {
  SourceLocation();
  SourceLocation(const std::string& url,
                 unsigned int line_number,
                 unsigned int column_number);
  ~SourceLocation();
  std::string url;
  unsigned int line_number = 0;
  unsigned int column_number = 0;
};

// The following structures hold parameters used during a navigation. In
// particular they are used by FrameMsg_Navigate, FrameHostMsg_BeginNavigation,
// and mojom::FrameNavigationControl.

// Provided by the browser or the renderer -------------------------------------

// Represents the Content Security Policy of the initator of the navigation.
struct CONTENT_EXPORT InitiatorCSPInfo {
  InitiatorCSPInfo();
  InitiatorCSPInfo(CSPDisposition should_check_main_world_csp,
                   const std::vector<ContentSecurityPolicy>& initiator_csp,
                   const base::Optional<CSPSource>& initiator_self_source);
  InitiatorCSPInfo(const InitiatorCSPInfo& other);
  ~InitiatorCSPInfo();

  // Whether or not the CSP of the main world should apply. When the navigation
  // is initiated from a content script in an isolated world, the CSP defined
  // in the main world should not apply.
  // TODO(arthursonzogni): Instead of this boolean, the origin of the isolated
  // world which has initiated the navigation should be passed.
  // See https://crbug.com/702540
  CSPDisposition should_check_main_world_csp = CSPDisposition::CHECK;

  // The relevant CSP policies and the initiator 'self' source to be used.
  std::vector<ContentSecurityPolicy> initiator_csp;
  base::Optional<CSPSource> initiator_self_source;
};

CONTENT_EXPORT mojom::CommonNavigationParamsPtr CreateCommonNavigationParams();
CONTENT_EXPORT mojom::CommitNavigationParamsPtr CreateCommitNavigationParams();

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
