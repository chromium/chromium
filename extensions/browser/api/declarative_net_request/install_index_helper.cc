// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/install_index_helper.h"

#include <iterator>
#include <utility>

#include "base/barrier_closure.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/ruleset_parse_result.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions::declarative_net_request {

namespace {
namespace dnr_api = api::declarative_net_request;

// A boolean that indicates if a ruleset should be ignored.
constexpr char kDNRIgnoreRulesetKey[] = "ignore_ruleset";

// Key corresponding to which we store a ruleset's checksum for the Declarative
// Net Request API.
constexpr char kDNRChecksumKey[] = "checksum";

// Converts a single ruleset result into a Dict.
base::Value::Dict ConvertRulesetToDict(bool ignored,
                                       std::optional<int> checksum) {
  base::Value::Dict result;
  result.Set(kDNRIgnoreRulesetKey, ignored);
  if (checksum) {
    result.Set(kDNRChecksumKey, *checksum);
  }

  return result;
}

void SetRulesetDict(base::Value::Dict& dict,
                    RulesetID id,
                    base::Value::Dict ruleset) {
  std::string key = base::NumberToString(id.value());
  DCHECK(!dict.Find(key));
  dict.Set(key, std::move(ruleset));
}

// Combines indexing results from multiple FileBackedRulesetSources into a
// single InstallIndexHelper::Result.
RulesetParseResult CombineResults(
    std::vector<std::pair<const FileBackedRulesetSource*,
                          IndexAndPersistJSONRulesetResult>> results,
    bool log_histograms) {
  using IndexStatus = IndexAndPersistJSONRulesetResult::Status;

  RulesetParseResult total_result;
  bool any_ruleset_indexed_successfully = false;
  size_t enabled_rules_count = 0;
  size_t enabled_regex_rules_count = 0;
  base::TimeDelta total_index_and_persist_time;

  // TODO(crbug.com/40534665): Limit the number of install warnings across all
  // rulesets.

  // Note |results| may be empty.
  for (auto& result_pair : results) {
    IndexAndPersistJSONRulesetResult& index_result = result_pair.second;
    const FileBackedRulesetSource* source = result_pair.first;

    // Per-ruleset limits should have been enforced during ruleset indexing.
    DCHECK_LE(index_result.regex_rules_count,
              static_cast<size_t>(GetRegexRuleLimit()));
    DCHECK_LE(index_result.rules_count, source->rule_count_limit());

    if (index_result.status == IndexStatus::kError) {
      total_result.error = std::move(index_result.error);
      return total_result;
    }

    total_result.warnings.insert(
        total_result.warnings.end(),
        std::make_move_iterator(index_result.warnings.begin()),
        std::make_move_iterator(index_result.warnings.end()));

    if (index_result.status == IndexStatus::kIgnore) {
      // If the ruleset was ignored and not indexed, there should be install
      // warnings associated.
      DCHECK(!index_result.warnings.empty());
      SetRulesetDict(total_result.ruleset_install_prefs, source->id(),
                     ConvertRulesetToDict(/*ignored=*/true,
                                          /*checksum=*/std::nullopt));
      continue;
    }

    DCHECK_EQ(IndexStatus::kSuccess, index_result.status);

    if (index_result.status == IndexStatus::kSuccess) {
      any_ruleset_indexed_successfully = true;

      SetRulesetDict(
          total_result.ruleset_install_prefs, source->id(),
          ConvertRulesetToDict(/*ignored=*/false,
                               std::move(index_result.ruleset_checksum)));

      total_index_and_persist_time += index_result.index_and_persist_time;

      if (source->enabled_by_default()) {
        enabled_rules_count += index_result.rules_count;
        enabled_regex_rules_count += index_result.regex_rules_count;
      }
    }
  }

  // Raise an install warning if the enabled regex rule count exceeds the API
  // limits. We don't raise a hard error to maintain forwards compatibility.
  if (enabled_regex_rules_count > static_cast<size_t>(GetRegexRuleLimit())) {
    total_result.warnings.emplace_back(
        kEnabledRegexRuleCountExceeded,
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
  }

  if (log_histograms && any_ruleset_indexed_successfully) {
    UMA_HISTOGRAM_TIMES(
        declarative_net_request::kIndexAndPersistRulesTimeHistogram,
        total_index_and_persist_time);

    UMA_HISTOGRAM_COUNTS_1M(
        declarative_net_request::kManifestEnabledRulesCountHistogram,
        enabled_rules_count);
  }

  return total_result;
}

}  // namespace

// static
void InstallIndexHelper::IndexStaticRulesets(
    const Extension& extension,
    FileBackedRulesetSource::RulesetFilter ruleset_filter,
    uint8_t parse_flags,
    IndexCallback callback) {
  // Note we use ref-counting instead of manual memory management since there
  // are some subtle cases:
  //  - Zero rulesets to index.
  //  - All individual callbacks return synchronously.
  // In these cases there's a potential for a use-after-free with manual memory
  // management.
  auto install_index_helper = base::WrapRefCounted(new InstallIndexHelper(
      FileBackedRulesetSource::CreateStatic(extension, ruleset_filter),
      std::move(callback)));
  install_index_helper->Start(parse_flags);
}

// static
base::expected<base::Value::Dict, std::string>
InstallIndexHelper::IndexAndPersistRulesOnInstall(Extension& extension) {
  // Index all static rulesets and therefore parse all static rules at
  // installation time for unpacked extensions. Throw an error for invalid rules
  // where possible so that the extension developer is immediately notified.
  auto ruleset_filter = declarative_net_request::FileBackedRulesetSource::
      RulesetFilter::kIncludeAll;
  auto parse_flags =
      declarative_net_request::RulesetSource::kRaiseErrorOnInvalidRules |
      declarative_net_request::RulesetSource::kRaiseWarningOnLargeRegexRules;

  // TODO(crbug.com/40538050): IndexStaticRulesetsUnsafe will read and parse
  // JSON synchronously. Change this so that we don't need to parse JSON in the
  // browser process.
  RulesetParseResult result =
      IndexStaticRulesetsUnsafe(extension, ruleset_filter, parse_flags);
  if (result.error) {
    return base::unexpected(std::move(*result.error));
  }

  if (!result.warnings.empty()) {
    extension.AddInstallWarnings(std::move(result.warnings));
  }

  return std::move(result.ruleset_install_prefs);
}

// static
RulesetParseResult InstallIndexHelper::IndexStaticRulesetsUnsafe(
    const Extension& extension,
    FileBackedRulesetSource::RulesetFilter ruleset_filter,
    uint8_t parse_flags) {
  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(extension, ruleset_filter);

  IndexResults results;
  results.reserve(sources.size());
  for (const FileBackedRulesetSource& source : sources) {
    results.emplace_back(&source,
                         source.IndexAndPersistJSONRulesetUnsafe(parse_flags));
  }

  // Don't log histograms for unpacked extensions so that the histograms reflect
  // real world usage.
  DCHECK(Manifest::IsUnpackedLocation(extension.location()));
  const bool log_histograms = false;
  return CombineResults(std::move(results), log_histograms);
}

InstallIndexHelper::InstallIndexHelper(
    std::vector<FileBackedRulesetSource> sources,
    IndexCallback callback)
    : sources_(std::move(sources)), callback_(std::move(callback)) {}

InstallIndexHelper::~InstallIndexHelper() = default;

void InstallIndexHelper::Start(uint8_t parse_flags) {
  // |all_done_closure| will be invoked once |barrier_closure| is run
  // |sources_.size()| times.
  base::OnceClosure all_done_closure =
      base::BindOnce(&InstallIndexHelper::OnAllRulesetsIndexed, this);
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(sources_.size(), std::move(all_done_closure));

  for (size_t i = 0; i < sources_.size(); ++i) {
    // Since |sources_| is const, |sources_[i]| is guaranteed to remain valid.
    auto callback = base::BindOnce(&InstallIndexHelper::OnRulesetIndexed, this,
                                   barrier_closure, i);
    sources_[i].IndexAndPersistJSONRuleset(&decoder_, parse_flags,
                                           std::move(callback));
  }
}

void InstallIndexHelper::OnAllRulesetsIndexed() {
  DCHECK_EQ(sources_.size(), results_.size());

  bool log_histograms = !sources_.empty();
  std::move(callback_).Run(CombineResults(std::move(results_), log_histograms));
}

// Callback invoked when indexing of a single ruleset is completed.
void InstallIndexHelper::OnRulesetIndexed(
    base::OnceClosure ruleset_done_closure,
    size_t source_index,
    IndexAndPersistJSONRulesetResult result) {
  results_.emplace_back(&sources_[source_index], std::move(result));
  std::move(ruleset_done_closure).Run();
}

}  // namespace extensions::declarative_net_request
