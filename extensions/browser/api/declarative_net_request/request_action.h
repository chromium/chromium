// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions::declarative_net_request {

namespace flat {
struct ModifyHeaderInfo;
}  // namespace flat

// A struct representing an action to be applied to a request based on DNR rule
// matches. Each action is attributed to exactly one extension.
struct RequestAction {
  // A copyable version of api::declarative_net_request::ModifyHeaderInfo.
  // This is used instead of ModifyHeaderInfo so it can be copied in Clone().
  struct HeaderInfo {
    HeaderInfo(std::string header,
               api::declarative_net_request::HeaderOperation operation,
               std::optional<std::string> value);
    explicit HeaderInfo(const flat::ModifyHeaderInfo& info);
    ~HeaderInfo();
    HeaderInfo(const HeaderInfo& other);
    HeaderInfo& operator=(const HeaderInfo& other);
    HeaderInfo(HeaderInfo&&);
    HeaderInfo& operator=(HeaderInfo&&);

    // The name of the header to be modified, specified in lowercase.
    std::string header;
    api::declarative_net_request::HeaderOperation operation;
    // The value for |header| to be appended or set.
    std::optional<std::string> value;
  };

  enum class Type {
    // Block the network request.
    BLOCK,
    // Block the network request and collapse the corresponding DOM element.
    COLLAPSE,
    // Allow the network request, preventing it from being intercepted by other
    // matching rules.
    ALLOW,
    // Redirect the network request.
    REDIRECT,
    // Upgrade the scheme of the network request.
    UPGRADE,
    // Allow the network request. This request is either for an allowlisted
    // frame or originated from one.
    ALLOW_ALL_REQUESTS,
    // Modify request/response headers.
    MODIFY_HEADERS,
  };

  RequestAction(Type type,
                uint32_t rule_id,
                uint64_t index_priority,
                RulesetID ruleset_id,
                const ExtensionId& extension_id);
  ~RequestAction();
  RequestAction(RequestAction&&);
  RequestAction& operator=(RequestAction&&);

  // Helper to create a copy.
  RequestAction Clone() const;

  Type type = Type::BLOCK;

  // Valid iff |IsRedirectOrUpgrade()| is true.
  std::optional<GURL> redirect_url;

  // The ID of the matching rule for this action.
  uint32_t rule_id;

  // The priority of this action in the index. This is a combination of the
  // rule's priority and the rule's action's priority.
  uint64_t index_priority;

  // The id of the ruleset corresponding to the matched rule.
  RulesetID ruleset_id;

  // The id of the extension the action is attributed to.
  ExtensionId extension_id;

  // Valid iff |type| is |MODIFY_HEADERS|.
  // TODO(crbug.com/40686893): Constructing these vectors could involve lots of
  // string copies. One potential enhancement involves storing a WeakPtr to the
  // flatbuffer index that contain the actual header strings.
  std::vector<HeaderInfo> request_headers_to_modify;
  std::vector<HeaderInfo> response_headers_to_modify;

  // Whether the action has already been tracked by the ActionTracker.
  // TODO(crbug.com/40635953): Move the tracking of actions matched to
  // ActionTracker.
  mutable bool tracked = false;

  bool IsBlockOrCollapse() const {
    return type == Type::BLOCK || type == Type::COLLAPSE;
  }
  bool IsRedirectOrUpgrade() const {
    return type == Type::REDIRECT || type == Type::UPGRADE;
  }
  bool IsAllowOrAllowAllRequests() const {
    return type == Type::ALLOW || type == Type::ALLOW_ALL_REQUESTS;
  }

 private:
  RequestAction(const RequestAction&);
};

// Compares RequestAction by |index_priority|, breaking ties by |ruleset_id|
// then |rule_id|.
bool operator<(const RequestAction& lhs, const RequestAction& rhs);
bool operator>(const RequestAction& lhs, const RequestAction& rhs);

std::optional<RequestAction> GetMaxPriorityAction(
    std::optional<RequestAction> lhs,
    std::optional<RequestAction> rhs);

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_ACTION_H_
