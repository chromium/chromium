// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions {

namespace dnr_api = api::declarative_net_request;
namespace declarative_net_request {

// static
const char* DNRManifestData::kManifestDataKey =
    dnr_api::ManifestKeys::kDeclarativeNetRequest;

DNRManifestData::RulesetInfo::RulesetInfo() = default;
DNRManifestData::RulesetInfo::~RulesetInfo() = default;
DNRManifestData::RulesetInfo::RulesetInfo(RulesetInfo&&) = default;
DNRManifestData::RulesetInfo& DNRManifestData::RulesetInfo::operator=(
    RulesetInfo&&) = default;

DNRManifestData::DNRManifestData(std::vector<RulesetInfo> rulesets)
    : rulesets_(std::move(rulesets)) {
  for (const RulesetInfo& info : rulesets_) {
    manifest_id_to_ruleset_map_.emplace(info.manifest_id, &info);
  }
}

DNRManifestData::~DNRManifestData() = default;

// static
base::span<const DNRManifestData::RulesetInfo> DNRManifestData::GetRulesets(
    const Extension& extension) {
  const auto* data = extension.GetManifestData<DNRManifestData>();
  if (!data) {
    return {};
  }

  return data->rulesets_;
}

const DNRManifestData::RulesetInfo* DNRManifestData::FindRulesetByManifestID(
    std::string_view manifest_id) const {
  return base::FindPtrOrNull(manifest_id_to_ruleset_map_, manifest_id);
}

// static
const DNRManifestData::RulesetInfo* DNRManifestData::FindRulesetByManifestID(
    const Extension& extension,
    std::string_view manifest_id) {
  const auto* data = extension.GetManifestData<DNRManifestData>();
  return data ? data->FindRulesetByManifestID(manifest_id) : nullptr;
}

// static
const DNRManifestData::RulesetInfo& DNRManifestData::GetRuleset(
    const Extension& extension,
    RulesetID ruleset_id) {
  const auto* data = extension.GetManifestData<DNRManifestData>();
  DCHECK(data);

  const std::vector<DNRManifestData::RulesetInfo>& rulesets = data->rulesets_;

  int index = ruleset_id.value() - kMinValidStaticRulesetID.value();
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), rulesets.size());

  return rulesets[index];
}

}  // namespace declarative_net_request
}  // namespace extensions
