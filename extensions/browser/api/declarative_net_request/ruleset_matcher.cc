// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

}  // namespace

// static
RulesetMatcher::LoadRulesetResult RulesetMatcher::CreateVerifiedMatcher(
    const base::FilePath& indexed_ruleset_path,
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) {
  DCHECK(matcher);
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;

  if (!base::PathExists(indexed_ruleset_path))
    return kLoadErrorInvalidPath;

  std::string ruleset_data;
  if (!base::ReadFileToString(indexed_ruleset_path, &ruleset_data))
    return kLoadErrorFileRead;

  if (!StripVersionHeaderAndParseVersion(&ruleset_data))
    return kLoadErrorVersionMismatch;

  // This guarantees that no memory access will end up outside the buffer.
  if (!IsValidRulesetData(
          base::make_span(reinterpret_cast<const uint8_t*>(ruleset_data.data()),
                          ruleset_data.size()),
          expected_ruleset_checksum)) {
    return kLoadErrorChecksumMismatch;
  }

  UMA_HISTOGRAM_TIMES(
      "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
      timer.Elapsed());

  // Using WrapUnique instead of make_unique since this class has a private
  // constructor.
  *matcher = base::WrapUnique(new RulesetMatcher(std::move(ruleset_data)));
  return kLoadSuccess;
}

RulesetMatcher::~RulesetMatcher() = default;

bool RulesetMatcher::ShouldBlockRequest(const GURL& url,
                                        const url::Origin& first_party_origin,
                                        flat_rule::ElementType element_type,
                                        bool is_third_party) const {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldBlockRequestTime."
      "SingleExtension");

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool disable_generic_rules = false;

  bool success =
      !!blocking_matcher_.FindMatch(
          url, first_party_origin, element_type, flat_rule::ActivationType_NONE,
          is_third_party, disable_generic_rules, FindRuleStrategy::kAny) &&
      !allowing_matcher_.FindMatch(
          url, first_party_origin, element_type, flat_rule::ActivationType_NONE,
          is_third_party, disable_generic_rules, FindRuleStrategy::kAny);
  return success;
}

bool RulesetMatcher::ShouldRedirectRequest(
    const GURL& url,
    const url::Origin& first_party_origin,
    flat_rule::ElementType element_type,
    bool is_third_party,
    GURL* redirect_url) const {
  DCHECK(redirect_url);
  DCHECK_NE(flat_rule::ElementType_WEBSOCKET, element_type);

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.ShouldRedirectRequestTime."
      "SingleExtension");

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool disable_generic_rules = false;

  // Retrieve the highest priority matching rule corresponding to the given
  // request parameters.
  const flat_rule::UrlRule* rule = redirect_matcher_.FindMatch(
      url, first_party_origin, element_type, flat_rule::ActivationType_NONE,
      is_third_party, disable_generic_rules,
      FindRuleStrategy::kHighestPriority);
  if (!rule)
    return false;

  // Find the UrlRuleMetadata corresponding to |rule|. Since |metadata_list_| is
  // sorted by rule id, use LookupByKey which binary searches for fast lookup.
  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());

  // There must be a UrlRuleMetadata object corresponding to each redirect rule.
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());

  *redirect_url = GURL(base::StringPiece(metadata->redirect_url()->c_str(),
                                         metadata->redirect_url()->size()));
  DCHECK(redirect_url->is_valid());
  return true;
}

RulesetMatcher::RulesetMatcher(std::string ruleset_data)
    : ruleset_data_(std::move(ruleset_data)),
      root_(flat::GetExtensionIndexedRuleset(ruleset_data_.data())),
      blocking_matcher_(root_->blocking_index()),
      allowing_matcher_(root_->allowing_index()),
      redirect_matcher_(root_->redirect_index()),
      metadata_list_(root_->extension_metadata()) {}

}  // namespace declarative_net_request
}  // namespace extensions
