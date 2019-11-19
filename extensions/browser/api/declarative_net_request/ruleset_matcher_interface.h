// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_

#include <vector>

#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace declarative_net_request {
struct RequestAction;
struct RequestParams;

// An abstract class for rule matchers. Overridden by different kinds of
// matchers, e.g. filter lists and regex.
// TODO(karandeepb): This is no longer an interface. Rename this class.
class RulesetMatcherInterface {
 public:
  RulesetMatcherInterface(const ExtensionId& extension_id,
                          api::declarative_net_request::SourceType source_type);

  virtual ~RulesetMatcherInterface();

  // Returns the ruleset's matching RequestAction with type |BLOCK| or
  // |COLLAPSE|, or base::nullopt if the ruleset has no matching blocking rule.
  virtual base::Optional<RequestAction> GetBlockOrCollapseAction(
      const RequestParams& params) const = 0;

  // Returns the ruleset's matching RequestAction with type |ALLOW| or
  // base::nullopt if the ruleset has no matching allow rule.
  virtual base::Optional<RequestAction> GetAllowAction(
      const RequestParams& params) const = 0;

  // Returns a RequestAction constructed from the matching redirect rule with
  // the highest priority, or base::nullopt if no matching redirect rules are
  // found for this request.
  virtual base::Optional<RequestAction> GetRedirectAction(
      const RequestParams& params) const = 0;

  // Returns a RequestAction constructed from the matching upgrade rule with the
  // highest priority, or base::nullopt if no matching upgrade rules are found
  // for this request.
  virtual base::Optional<RequestAction> GetUpgradeAction(
      const RequestParams& params) const = 0;

  // Returns the bitmask of headers to remove from the request. The bitmask
  // corresponds to flat::RemoveHeaderType. |ignored_mask| denotes the mask of
  // headers to be skipped for evaluation and is excluded in the return value.
  virtual uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t ignored_mask,
      std::vector<RequestAction>* remove_headers_actions) const = 0;

  // Returns whether this modifies "extraHeaders".
  virtual bool IsExtraHeadersMatcher() const = 0;

  // Returns the extension ID with which this matcher is associated.
  const ExtensionId& extension_id() const { return extension_id_; }

  // The source type of the matcher.
  api::declarative_net_request::SourceType source_type() const {
    return source_type_;
  }

 protected:
  using ExtensionMetadataList =
      ::flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;

  // Returns true if the given request can be upgraded.
  static bool IsUpgradeableRequest(const RequestParams& params);

  // Helper to create a RequestAction of type |BLOCK| or |COLLAPSE|.
  RequestAction CreateBlockOrCollapseRequestAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |ALLOW|.
  RequestAction CreateAllowAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |REDIRECT| with the request
  // upgraded.
  RequestAction CreateUpgradeAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |REDIRECT| with the appropriate
  // redirect url. Can return base::nullopt if the redirect url is ill-formed or
  // same as the current request url.
  base::Optional<RequestAction> CreateRedirectAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule,
      const ExtensionMetadataList& metadata_list) const;

  // Helper to create a RequestAction of type |REMOVE_HEADERS|. |mask|
  // corresponds to bitmask of flat::RemoveHeaderType, and must be non-empty.
  RequestAction GetRemoveHeadersActionForMask(
      const url_pattern_index::flat::UrlRule& rule,
      uint8_t mask) const;

 private:
  const ExtensionId extension_id_;
  const api::declarative_net_request::SourceType source_type_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcherInterface);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_INTERFACE_H_
