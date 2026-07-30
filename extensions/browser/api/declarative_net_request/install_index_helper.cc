// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/install_index_helper.h"

#include <iterator>
#include <numeric>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
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
base::DictValue ConvertRulesetToDict(bool ignored,
                                     std::optional<int> checksum) {
  base::DictValue result;
  result.Set(kDNRIgnoreRulesetKey, ignored);
  if (checksum) {
    result.Set(kDNRChecksumKey, *checksum);
  }

  return result;
}

void SetRulesetDict(base::DictValue& dict,
                    RulesetID id,
                    base::DictValue ruleset) {
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

InstallIndexHelper::InstallIndexHelper(
    std::vector<FileBackedRulesetSource> sources,
    uint8_t parse_flags,
    bool log_histograms,
    IndexCallback callback)
    : sources_(std::move(sources)),
      parse_flags_(parse_flags),
      log_histograms_(log_histograms),
      callback_(std::move(callback)) {
  DCHECK(callback_);
  results_.reserve(sources_.size());
}

InstallIndexHelper::~InstallIndexHelper() = default;

// static
void InstallIndexHelper::IndexStaticRulesets(
    const Extension& extension,
    FileBackedRulesetSource::RulesetFilter ruleset_filter,
    uint8_t parse_flags,
    IndexCallback callback) {
  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(extension, ruleset_filter);

  // Don't log histograms for unpacked extensions so that the histograms reflect
  // real world usage.
  const bool log_histograms =
      !Manifest::IsUnpackedLocation(extension.location());

  auto helper = base::WrapRefCounted(new InstallIndexHelper(
      std::move(sources), parse_flags, log_histograms, std::move(callback)));
  helper->Start();
}

// static
bool InstallIndexHelper::IndexRuleset(const FileBackedRulesetSource& source,
                                      uint8_t parse_flags,
                                      IndexResults& results) {
  IndexAndPersistJSONRulesetResult result =
      source.IndexAndPersistJSONRuleset(parse_flags);

  bool is_error =
      (result.status == IndexAndPersistJSONRulesetResult::Status::kError);

  results.emplace_back(&source, std::move(result));
  return !is_error;
}

void InstallIndexHelper::Start(size_t start_index) {
  base::ElapsedTimer timer;

  for (size_t i = start_index; i < sources_.size(); ++i) {
    if (!IndexRuleset(sources_[i], parse_flags_, results_)) {
      OnIndexingFinished();
      return;
    }

    if (i + 1 < sources_.size() && timer.Elapsed() >= kMaxTimeSlice) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&InstallIndexHelper::Start, this, i + 1));
      return;
    }
  }

  OnIndexingFinished();
}

void InstallIndexHelper::OnIndexingFinished() {
  DCHECK(callback_);
  RulesetParseResult total_result =
      CombineResults(std::move(results_), log_histograms_);
  std::move(callback_).Run(std::move(total_result));
}

// static
RulesetParseResult InstallIndexHelper::IndexAndPersistRulesOnInstall(
    const Extension& extension) {
  auto ruleset_filter = declarative_net_request::FileBackedRulesetSource::
      RulesetFilter::kIncludeAll;
  auto parse_flags =
      declarative_net_request::RulesetSource::kRaiseErrorOnInvalidRules |
      declarative_net_request::RulesetSource::kRaiseWarningOnLargeRegexRules;

  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(extension, ruleset_filter);

  IndexResults results;
  results.reserve(sources.size());

  for (const auto& source : sources) {
    if (!IndexRuleset(source, parse_flags, results)) {
      break;
    }
  }

  const bool log_histograms =
      !Manifest::IsUnpackedLocation(extension.location());

  return CombineResults(std::move(results), log_histograms);
}

}  // namespace extensions::declarative_net_request
