// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <iterator>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/public_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

std::string ReadSetsFile(base::File sets_file) {
  std::string raw_sets;
  base::ScopedFILE file(FileToFILE(std::move(sets_file), "r"));
  return base::ReadStreamToString(file.get(), &raw_sets) ? raw_sets : "";
}

}  // namespace

FirstPartySetsLoader::FirstPartySetsLoader(
    LoadCompleteOnceCallback on_load_complete)
    : on_load_complete_(std::move(on_load_complete)) {}

FirstPartySetsLoader::~FirstPartySetsLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FirstPartySetsLoader::SetManuallySpecifiedSet(
    const LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manually_specified_set_ = local_set;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadCommandLineSet2",
      construction_timer_.Elapsed());

  MaybeFinishLoading();
}

void FirstPartySetsLoader::SetComponentSets(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component_sets_parse_progress_ != Progress::kNotStarted) {
    DisposeFile(std::move(sets_file));
    return;
  }

  component_sets_parse_progress_ = Progress::kStarted;

  if (!sets_file.IsValid()) {
    OnReadSetsFile("");
    return;
  }

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadSetsFile, std::move(sets_file)),
      base::BindOnce(&FirstPartySetsLoader::OnReadSetsFile,
                     weak_factory_.GetWeakPtr()));
}

void FirstPartySetsLoader::OnReadSetsFile(const std::string& raw_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(component_sets_parse_progress_, Progress::kStarted);

  std::istringstream stream(raw_sets);
  FirstPartySetParser::SetsAndAliases public_sets =
      FirstPartySetParser::ParseSetsFromStream(stream, /*emit_errors=*/false);
  sets_ = std::move(public_sets.first);
  aliases_ = std::move(public_sets.second);

  component_sets_parse_progress_ = Progress::kFinished;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadComponentSets2",
      construction_timer_.Elapsed());
  MaybeFinishLoading();
}

void FirstPartySetsLoader::DisposeFile(base::File sets_file) {
  if (sets_file.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::File sets_file) {
              // Run `sets_file`'s dtor in the threadpool.
            },
            std::move(sets_file)));
  }
}

base::flat_set<net::SchemefulSite> FirstPartySetsLoader::FindIntersection()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());
  std::vector<net::SchemefulSite> intersection;
  for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
           public_site_and_entry : sets_) {
    const net::SchemefulSite& public_site = public_site_and_entry.first;
    const net::SchemefulSite& public_primary =
        public_site_and_entry.second.primary();
    bool is_affected_by_local_set =
        public_site == manually_specified_set_->GetPrimary() ||
        public_primary == manually_specified_set_->GetPrimary() ||
        base::ranges::any_of(
            manually_specified_set_->GetSet(),
            [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
                    manual_site_and_entry) {
              const net::SchemefulSite& manual_site =
                  manual_site_and_entry.first;
              return manual_site == public_site ||
                     manual_site == public_primary;
            });
    if (is_affected_by_local_set) {
      intersection.push_back(public_site_and_entry.first);
    }
  };

  return intersection;
}

base::flat_set<net::SchemefulSite> FirstPartySetsLoader::FindSingletons()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<net::SchemefulSite> primaries_with_members;
  for (const auto& [site, entry] : sets_) {
    if (site != entry.primary())
      primaries_with_members.push_back(entry.primary());
  }
  std::vector<net::SchemefulSite> singletons;
  for (const auto& [site, entry] : sets_) {
    if (site == entry.primary() &&
        !base::Contains(primaries_with_members, site)) {
      singletons.push_back(site);
    }
  }

  return singletons;
}

void FirstPartySetsLoader::ApplyManuallySpecifiedSet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());
  if (manually_specified_set_->empty())
    return;

  base::flat_set<net::SchemefulSite> intersection = FindIntersection();
  for (const auto& site : intersection) {
    sets_.erase(site);
  }

  base::flat_set<net::SchemefulSite> singletons = FindSingletons();
  for (const auto& singleton : singletons) {
    sets_.erase(singleton);
  }

  base::ranges::copy(manually_specified_set_->GetSet(),
                     std::inserter(sets_, sets_.end()));

  // Finally, remove any aliases for public sites that were affected (deleted),
  // and add any aliases defined in the local set.
  base::EraseIf(
      aliases_,
      [&](const std::pair<net::SchemefulSite, net::SchemefulSite>& alias) {
        return intersection.contains(alias.second) ||
               singletons.contains(alias.second);
      });
  base::flat_map<net::SchemefulSite, net::SchemefulSite> manual_aliases =
      manually_specified_set_->GetAliases();
  aliases_.insert(manual_aliases.begin(), manual_aliases.end());
}

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasAllInputs())
    return;
  ApplyManuallySpecifiedSet();
  std::move(on_load_complete_)
      .Run(net::PublicSets(std::move(sets_), std::move(aliases_)));
}

bool FirstPartySetsLoader::HasAllInputs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_sets_parse_progress_ == Progress::kFinished &&
         manually_specified_set_.has_value();
}

}  // namespace content
