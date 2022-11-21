// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <iterator>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
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

void FirstPartySetsLoader::SetComponentSets(base::Version version,
                                            base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component_sets_parse_progress_ != Progress::kNotStarted) {
    DisposeFile(std::move(sets_file));
    return;
  }

  component_sets_parse_progress_ = Progress::kStarted;

  if (!sets_file.IsValid() || !version.IsValid()) {
    OnReadSetsFile(base::Version(), "");
    return;
  }

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadSetsFile, std::move(sets_file)),
      base::BindOnce(&FirstPartySetsLoader::OnReadSetsFile,
                     weak_factory_.GetWeakPtr(), std::move(version)));
}

void FirstPartySetsLoader::OnReadSetsFile(base::Version version,
                                          const std::string& raw_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(component_sets_parse_progress_, Progress::kStarted);

  std::istringstream stream(raw_sets);
  FirstPartySetParser::SetsAndAliases public_sets =
      FirstPartySetParser::ParseSetsFromStream(stream, /*emit_errors=*/false);
  sets_ = net::GlobalFirstPartySets(std::move(version),
                                    std::move(public_sets.first),
                                    std::move(public_sets.second));

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

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component_sets_parse_progress_ != Progress::kFinished ||
      !manually_specified_set_.has_value()) {
    return;
  }
  if (!manually_specified_set_->empty()) {
    sets_->ApplyManuallySpecifiedSet(manually_specified_set_->GetSet());
  }
  std::move(on_load_complete_).Run(std::move(sets_).value());
}

}  // namespace content
