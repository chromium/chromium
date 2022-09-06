// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
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
      FirstPartySetParser::ParseSetsFromStream(stream);
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

void FirstPartySetsLoader::ApplyManuallySpecifiedSet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());
  if (manually_specified_set_->empty())
    return;
  const net::SchemefulSite& manual_owner =
      manually_specified_set_->GetPrimary();
  const FlattenedSets& manual_sites = manually_specified_set_->GetSet();

  // Erase the intersection between |sets_| and |manually_specified_set_| and
  // any members whose owner was in the intersection.
  base::EraseIf(
      sets_, [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
                     public_site_and_entry) {
        const net::SchemefulSite& public_site = public_site_and_entry.first;
        const net::SchemefulSite& public_owner =
            public_site_and_entry.second.primary();
        return public_site == manual_owner || public_owner == manual_owner ||
               base::ranges::any_of(
                   manual_sites, [&](const std::pair<net::SchemefulSite,
                                                     net::FirstPartySetEntry>&
                                         manual_site_and_entry) {
                     const net::SchemefulSite& manual_site =
                         manual_site_and_entry.first;
                     return manual_site == public_site ||
                            manual_site == public_owner;
                   });
      });

  // Next, we must add the manually specified set to |sets_|.
  for (const auto& manual_site : manual_sites) {
    sets_.emplace(manual_site.first, manual_site.second);
  }
  // Now remove singleton sets, which are sets that just contain sites that
  // *are* owners, but no longer have any (other) members.
  std::set<net::SchemefulSite> owners_with_members;
  for (const auto& it : sets_) {
    if (it.first != it.second.primary())
      owners_with_members.insert(it.second.primary());
  }
  base::EraseIf(sets_, [&owners_with_members](const auto& p) {
    return p.first == p.second.primary() &&
           !base::Contains(owners_with_members, p.first);
  });
}

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasAllInputs())
    return;
  ApplyManuallySpecifiedSet();
  network::mojom::PublicFirstPartySetsPtr public_sets =
      network::mojom::PublicFirstPartySets::New();
  public_sets->sets = std::move(sets_);
  public_sets->aliases = std::move(aliases_);
  std::move(on_load_complete_).Run(std::move(public_sets));
}

bool FirstPartySetsLoader::HasAllInputs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_sets_parse_progress_ == Progress::kFinished &&
         manually_specified_set_.has_value();
}

}  // namespace content
