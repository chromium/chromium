// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_

#include <cstdint>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

// A struct representing an action to be applied to a request based on DNR rule
// matches. Each action is attributed to exactly one extension.
struct RequestAction {
  enum class Type {
    // Block the network request.
    BLOCK,
    // Block the network request and collapse the corresponding DOM element.
    COLLAPSE,
    // Allow the network request, preventing it from being intercepted by other
    // matching rules. Only used for tracking a matched allow rule.
    ALLOW,
    // Redirect the network request.
    REDIRECT,
    // Remove request/response headers.
    REMOVE_HEADERS,
  };

  RequestAction(Type type,
                uint32_t rule_id,
                uint32_t rule_priority,
                api::declarative_net_request::SourceType source_type,
                const ExtensionId& extension_id);
  ~RequestAction();
  RequestAction(RequestAction&&);
  RequestAction& operator=(RequestAction&&);

  Type type = Type::BLOCK;

  // Valid iff |type| is |REDIRECT|.
  base::Optional<GURL> redirect_url;

  // The ID of the matching rule for this action.
  uint32_t rule_id;

  // The priority of the matching rule for this action. Only really valid for
  // redirect actions.
  uint32_t rule_priority;

  // The source type of the matching rule for this action.
  api::declarative_net_request::SourceType source_type;

  // The id of the extension the action is attributed to.
  ExtensionId extension_id;

  // Valid iff |type| is |REMOVE_HEADERS|. The vectors point to strings of
  // static storage duration.
  std::vector<const char*> request_headers_to_remove;
  std::vector<const char*> response_headers_to_remove;

  // Whether the action has already been tracked by the ActionTracker.
  // TODO(crbug.com/983761): Move the tracking of actions matched to
  // ActionTracker.
  mutable bool tracked = false;

  DISALLOW_COPY_AND_ASSIGN(RequestAction);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
