// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/index_helper.h"

#include <iterator>
#include <utility>

#include "base/barrier_closure.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {
namespace declarative_net_request {

namespace {
namespace dnr_api = api::declarative_net_request;

// Combines indexing results from multiple FileBackedRulesetSources into a
// single IndexHelper::Result.
IndexHelper::Result CombineResults(
    std::vector<std::pair<const FileBackedRulesetSource*,
                          IndexAndPersistJSONRulesetResult>> results,
    bool log_histograms) {
  using IndexStatus = IndexAndPersistJSONRulesetResult::Status;

  IndexHelper::Result total_result;
  total_result.ruleset_install_prefs.reserve(results.size());
  bool any_ruleset_indexed_successfully = false;
  size_t total_rules_count = 0;
  size_t enabled_rules_count = 0;
  size_t enabled_regex_rules_count = 0;
  base::TimeDelta total_index_and_persist_time;

  // TODO(crbug.com/754526): Limit the number of install warnings across all
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
      total_result.ruleset_install_prefs.emplace_back(
          source->id(), base::nullopt /* ruleset_checksum */,
          true /* ignored */);
      continue;
    }

    DCHECK_EQ(IndexStatus::kSuccess, index_result.status);

    if (index_result.status == IndexStatus::kSuccess) {
      any_ruleset_indexed_successfully = true;

      total_result.ruleset_install_prefs.emplace_back(
          source->id(), std::move(index_result.ruleset_checksum),
          false /* ignored */);

      total_index_and_persist_time += index_result.index_and_persist_time;
      total_rules_count += index_result.rules_count;

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
        declarative_net_request::kManifestRulesCountHistogram,
        total_rules_count);

    UMA_HISTOGRAM_COUNTS_1M(
        declarative_net_request::kManifestEnabledRulesCountHistogram,
        enabled_rules_count);
  }

  return total_result;
}

}  // namespace

IndexHelper::Result::Result() = default;
IndexHelper::Result::~Result() = default;
IndexHelper::Result::Result(Result&&) = default;
IndexHelper::Result& IndexHelper::Result::operator=(Result&&) = default;

// static
void IndexHelper::IndexStaticRulesets(const Extension& extension,
                                      IndexCallback callback) {
  // Note we use ref-counting instead of manual memory management since there
  // are some subtle cases:
  //  - Zero rulesets to index.
  //  - All individual callbacks return synchronously.
  // In these cases there's a potential for a use-after-free with manual memory
  // management.
  auto index_helper = base::WrapRefCounted(new IndexHelper(
      FileBackedRulesetSource::CreateStatic(extension), std::move(callback)));
  index_helper->Start();
}

// static
IndexHelper::Result IndexHelper::IndexStaticRulesetsUnsafe(
    const Extension& extension) {
  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(extension);

  IndexResults results;
  results.reserve(sources.size());
  for (const FileBackedRulesetSource& source : sources)
    results.emplace_back(&source, source.IndexAndPersistJSONRulesetUnsafe());

  // Don't log histograms for unpacked extensions so that the histograms reflect
  // real world usage.
  DCHECK(Manifest::IsUnpackedLocation(extension.location()));
  const bool log_histograms = false;
  return CombineResults(std::move(results), log_histograms);
}

IndexHelper::IndexHelper(std::vector<FileBackedRulesetSource> sources,
                         IndexCallback callback)
    : sources_(std::move(sources)), callback_(std::move(callback)) {}

IndexHelper::~IndexHelper() = default;

void IndexHelper::Start() {
  // |all_done_closure| will be invoked once |barrier_closure| is run
  // |sources_.size()| times.
  base::OnceClosure all_done_closure =
      base::BindOnce(&IndexHelper::OnAllRulesetsIndexed, this);
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(sources_.size(), std::move(all_done_closure));

  for (size_t i = 0; i < sources_.size(); ++i) {
    // Since |sources_| is const, |sources_[i]| is guaranteed to remain valid.
    auto callback = base::BindOnce(&IndexHelper::OnRulesetIndexed, this,
                                   barrier_closure, i);
    sources_[i].IndexAndPersistJSONRuleset(&decoder_, std::move(callback));
  }
}

void IndexHelper::OnAllRulesetsIndexed() {
  DCHECK_EQ(sources_.size(), results_.size());

  bool log_histograms = !sources_.empty();
  std::move(callback_).Run(CombineResults(std::move(results_), log_histograms));
}

// Callback invoked when indexing of a single ruleset is completed.
void IndexHelper::OnRulesetIndexed(base::OnceClosure ruleset_done_closure,
                                   size_t source_index,
                                   IndexAndPersistJSONRulesetResult result) {
  results_.emplace_back(&sources_[source_index], std::move(result));
  std::move(ruleset_done_closure).Run();
}

}  // namespace declarative_net_request
}  // namespace extensions
