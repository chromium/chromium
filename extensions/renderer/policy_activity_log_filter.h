// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_POLICY_ACTIVITY_LOG_FILTER_H_
#define EXTENSIONS_RENDERER_POLICY_ACTIVITY_LOG_FILTER_H_

#include <string>

#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace extensions {

// An interface for an embedder-provided filter that decides which extension
// activities are high-risk enough to be reported for enterprise telemetry.
//
// This filter is stateless and is used by the DOMActivityLogger to prune
// benign events before they are sent to the browser process, minimizing
// IPC overhead and privacy impact.
class PolicyActivityLogFilter {
 public:
  virtual ~PolicyActivityLogFilter() = default;

  // Returns true if the specific activity should be logged for policy auditing.
  // This is called by the DOMActivityLogger before sending an IPC to the
  // browser process.
  //
  // `extension_id`: The ID of the extension performing the action.
  // `action_type`: Whether the action is a GETTER, SETTER, or METHOD call.
  // `api_name`: The DOM API being accessed (e.g., "document.cookie").
  // `args`: The converted arguments passed to the API call.
  // `url`: The URL of the page where the activity is occurring.
  virtual bool IsHighRiskEvent(const ExtensionId& extension_id,
                               DomActionType::Type action_type,
                               const std::string& api_name,
                               const base::ListValue& args,
                               const GURL& url) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_POLICY_ACTIVITY_LOG_FILTER_H_
