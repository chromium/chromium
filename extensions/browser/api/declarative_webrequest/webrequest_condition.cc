// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_factory.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition_attribute.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"

using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;
using url_matcher::URLMatcherFactory;

namespace keys = extensions::declarative_webrequest_constants;

namespace {
static URLMatcherConditionSet::ID g_next_id = 0;

// TODO(battre): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "declarativeWebRequest.RequestMatcher";
const char kInvalidTypeOfParamter[] = "Attribute '%s' has an invalid type";
const char kConditionCannotBeFulfilled[] = "A condition can never be "
    "fulfilled because its attributes cannot all be tested at the "
    "same time in the request life-cycle.";
}  // namespace

namespace extensions {

namespace keys = declarative_webrequest_constants;

//
// WebRequestData
//

WebRequestData::WebRequestData(const WebRequestInfo* request,
                               RequestStage stage)
    : request(request), stage(stage), original_response_headers(nullptr) {}

WebRequestData::WebRequestData(
    const WebRequestInfo* request,
    RequestStage stage,
    const net::HttpResponseHeaders* original_response_headers)
    : request(request),
      stage(stage),
      original_response_headers(original_response_headers) {}

WebRequestData::~WebRequestData() = default;

//
// WebRequestDataWithMatchIds
//

WebRequestDataWithMatchIds::WebRequestDataWithMatchIds(
    const WebRequestData* request_data)
    : data(request_data) {}

WebRequestDataWithMatchIds::~WebRequestDataWithMatchIds() {}

//
// WebRequestCondition
//

WebRequestCondition::WebRequestCondition(
    scoped_refptr<URLMatcherConditionSet> url_matcher_conditions,
    scoped_refptr<URLMatcherConditionSet> first_party_url_matcher_conditions,
    const WebRequestConditionAttributes& condition_attributes)
    : url_matcher_conditions_(url_matcher_conditions),
      first_party_url_matcher_conditions_(first_party_url_matcher_conditions),
      condition_attributes_(condition_attributes),
      applicable_request_stages_(~0) {
  for (WebRequestConditionAttributes::const_iterator i =
       condition_attributes_.begin(); i != condition_attributes_.end(); ++i) {
    applicable_request_stages_ &= (*i)->GetStages();
  }
}

WebRequestCondition::~WebRequestCondition() {}

bool WebRequestCondition::IsFulfilled(
    const MatchData& request_data) const {
  if (!(request_data.data->stage & applicable_request_stages_)) {
    // A condition that cannot be evaluated is considered as violated.
    return false;
  }

  // Check URL attributes if present.
  if (url_matcher_conditions_.get() &&
      !base::Contains(request_data.url_match_ids,
                      url_matcher_conditions_->id()))
    return false;
  if (first_party_url_matcher_conditions_.get() &&
      !base::Contains(request_data.first_party_url_match_ids,
                      first_party_url_matcher_conditions_->id()))
    return false;

  // All condition attributes must be fulfilled for a fulfilled condition.
  for (auto i = condition_attributes_.cbegin();
       i != condition_attributes_.cend(); ++i) {
    if (!(*i)->IsFulfilled(*(request_data.data)))
      return false;
  }
  return true;
}

void WebRequestCondition::GetURLMatcherConditionSets(
    URLMatcherConditionSet::Vector* condition_sets) const {
  if (url_matcher_conditions_.get())
    condition_sets->push_back(url_matcher_conditions_);
  if (first_party_url_matcher_conditions_.get())
    condition_sets->push_back(first_party_url_matcher_conditions_);
}

// static
std::unique_ptr<WebRequestCondition> WebRequestCondition::Create(
    const Extension* extension,
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value& condition,
    std::string* error) {
  const base::DictionaryValue* condition_dict = NULL;
  if (!condition.GetAsDictionary(&condition_dict)) {
    *error = kExpectedDictionary;
    return std::unique_ptr<WebRequestCondition>();
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!condition_dict->GetString(keys::kInstanceTypeKey, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return std::unique_ptr<WebRequestCondition>();
  }
  if (instance_type != keys::kRequestMatcherType) {
    *error = kExpectedOtherConditionType;
    return std::unique_ptr<WebRequestCondition>();
  }

  WebRequestConditionAttributes attributes;
  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set;
  scoped_refptr<URLMatcherConditionSet> first_party_url_matcher_condition_set;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& condition_attribute_name = iter.key();
    const base::Value& condition_attribute_value = iter.value();
    const bool name_is_url = condition_attribute_name == keys::kUrlKey;
    if (condition_attribute_name == keys::kInstanceTypeKey) {
      // Skip this.
    } else if (name_is_url ||
               condition_attribute_name == keys::kFirstPartyForCookiesUrlKey) {
      const base::DictionaryValue* dict = NULL;
      if (!condition_attribute_value.GetAsDictionary(&dict)) {
        *error = base::StringPrintf(kInvalidTypeOfParamter,
                                    condition_attribute_name.c_str());
      } else {
        if (name_is_url) {
          url_matcher_condition_set =
              URLMatcherFactory::CreateFromURLFilterDictionary(
                  url_matcher_condition_factory, dict, ++g_next_id, error);
        } else {
          first_party_url_matcher_condition_set =
              URLMatcherFactory::CreateFromURLFilterDictionary(
                  url_matcher_condition_factory, dict, ++g_next_id, error);
        }
      }
    } else {
      scoped_refptr<const WebRequestConditionAttribute> attribute =
          WebRequestConditionAttribute::Create(
              condition_attribute_name,
              &condition_attribute_value,
              error);
      if (attribute.get())
        attributes.push_back(attribute);
    }
    if (!error->empty())
      return std::unique_ptr<WebRequestCondition>();
  }

  std::unique_ptr<WebRequestCondition> result(new WebRequestCondition(
      url_matcher_condition_set, first_party_url_matcher_condition_set,
      attributes));

  if (!result->stages()) {
    *error = kConditionCannotBeFulfilled;
    return std::unique_ptr<WebRequestCondition>();
  }

  return result;
}

}  // namespace extensions
