// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/url_matcher/url_matcher.h"
#include "extensions/browser/api/declarative/declarative_rule.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "net/http/http_response_headers.h"

namespace extensions {
struct WebRequestInfo;

// Container for information about a URL request to determine which
// rules apply to the request.
struct WebRequestData {
  WebRequestData(const WebRequestInfo* request, RequestStage stage);
  WebRequestData(const WebRequestInfo* request,
                 RequestStage stage,
                 const net::HttpResponseHeaders* original_response_headers);
  ~WebRequestData();

  // The network request that is currently being processed.
  const WebRequestInfo* request;
  // The stage (progress) of the network request.
  RequestStage stage;
  const net::HttpResponseHeaders* original_response_headers;
};

// Adds information about URL matches to WebRequestData.
struct WebRequestDataWithMatchIds {
  explicit WebRequestDataWithMatchIds(const WebRequestData* request_data);
  ~WebRequestDataWithMatchIds();

  const WebRequestData* data;
  std::set<url_matcher::URLMatcherConditionSet::ID> url_match_ids;
};

// Representation of a condition in the Declarative WebRequest API. A condition
// consists of several attributes. Each of these attributes needs to be
// fulfilled in order for the condition to be fulfilled.
//
// We distinguish between two types of conditions:
// - URL Matcher conditions are conditions that test the URL of a request.
//   These are treated separately because we use a URLMatcher to efficiently
//   test many of these conditions in parallel by using some advanced
//   data structures. The URLMatcher tells us if all URL Matcher conditions
//   are fulfilled for a WebRequestCondition.
// - All other conditions are represented as WebRequestConditionAttributes.
//   These conditions are probed linearly (only if the URL Matcher found a hit).
//
// TODO(battre) Consider making the URLMatcher an owner of the
// URLMatcherConditionSet and only pass a pointer to URLMatcherConditionSet
// in url_matcher_condition_set(). This saves some copying in
// WebRequestConditionSet::GetURLMatcherConditionSets.
class WebRequestCondition {
 public:
  typedef WebRequestDataWithMatchIds MatchData;

  WebRequestCondition(
      scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_conditions,
      const WebRequestConditionAttributes& condition_attributes);
  ~WebRequestCondition();

  // Factory method that instantiates a WebRequestCondition according to
  // the description |condition| passed by the extension API.
  static std::unique_ptr<WebRequestCondition> Create(
      const Extension* extension,
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error);

  // Returns whether the request matches this condition.
  bool IsFulfilled(const MatchData& request_data) const;

  // If this condition has url attributes, appends them to |condition_sets|.
  void GetURLMatcherConditionSets(
      url_matcher::URLMatcherConditionSet::Vector* condition_sets) const;

  // Returns a bit vector representing extensions::RequestStage. The bit vector
  // contains a 1 for each request stage during which the condition can be
  // tested.
  int stages() const { return applicable_request_stages_; }

 private:
  // URL attributes of this condition.
  scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_conditions_;

  // All non-UrlFilter attributes of this condition.
  WebRequestConditionAttributes condition_attributes_;

  // Bit vector indicating all RequestStage during which all
  // |condition_attributes_| can be evaluated.
  int applicable_request_stages_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestCondition);
};

typedef DeclarativeConditionSet<WebRequestCondition> WebRequestConditionSet;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONDITION_H_
