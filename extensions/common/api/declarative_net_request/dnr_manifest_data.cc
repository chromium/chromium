// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions {

namespace dnr_api = api::declarative_net_request;
namespace declarative_net_request {

DNRManifestData::RulesetInfo::RulesetInfo() = default;
DNRManifestData::RulesetInfo::~RulesetInfo() = default;
DNRManifestData::RulesetInfo::RulesetInfo(RulesetInfo&&) = default;
DNRManifestData::RulesetInfo& DNRManifestData::RulesetInfo::operator=(
    RulesetInfo&&) = default;

DNRManifestData::DNRManifestData(std::vector<RulesetInfo> rulesets)
    : rulesets(std::move(rulesets)) {
  for (const RulesetInfo& info : this->rulesets)
    manifest_id_to_ruleset_map.emplace(info.manifest_id, &info);
}

DNRManifestData::~DNRManifestData() = default;

// static
const std::vector<DNRManifestData::RulesetInfo>& DNRManifestData::GetRulesets(
    const Extension& extension) {
  // Since we return a reference, use a function local static for the case where
  // the extension didn't specify any rulesets.
  static const base::NoDestructor<std::vector<DNRManifestData::RulesetInfo>>
      empty_vector;

  Extension::ManifestData* data =
      extension.GetManifestData(dnr_api::ManifestKeys::kDeclarativeNetRequest);
  if (!data)
    return *empty_vector;

  return static_cast<DNRManifestData*>(data)->rulesets;
}

const DNRManifestData::ManifestIDToRulesetMap&
DNRManifestData::GetManifestIDToRulesetMap(const Extension& extension) {
  // Since we return a reference, use a function local static for the case where
  // the extension didn't specify any rulesets.
  static const base::NoDestructor<ManifestIDToRulesetMap> empty_map;

  Extension::ManifestData* data =
      extension.GetManifestData(dnr_api::ManifestKeys::kDeclarativeNetRequest);
  if (!data)
    return *empty_map;

  return static_cast<DNRManifestData*>(data)->manifest_id_to_ruleset_map;
}

// static
const DNRManifestData::RulesetInfo& DNRManifestData::GetRuleset(
    const Extension& extension,
    RulesetID ruleset_id) {
  Extension::ManifestData* data =
      extension.GetManifestData(dnr_api::ManifestKeys::kDeclarativeNetRequest);
  DCHECK(data);

  const std::vector<DNRManifestData::RulesetInfo>& rulesets =
      static_cast<DNRManifestData*>(data)->rulesets;

  int index = ruleset_id.value() - kMinValidStaticRulesetID.value();
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), rulesets.size());

  return rulesets[index];
}

}  // namespace declarative_net_request
}  // namespace extensions
