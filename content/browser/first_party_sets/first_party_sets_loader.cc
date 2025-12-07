// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <sstream>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/features.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"

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
    const net::LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (manually_specified_set_.has_value()) {
    return;
  }
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

  // We may use USER_BLOCKING here since First-Party Set initialization may
  // block network navigations at startup. Otherwise, initialization blocks
  // resolution of promises from `document.requestStorageAccess()`, but those
  // calls are unlikely to occur during startup.
  base::TaskPriority priority =
      base::FeatureList::IsEnabled(net::features::kWaitForFirstPartySetsInit)
          ? base::TaskPriority::USER_BLOCKING
          : base::TaskPriority::USER_VISIBLE;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), priority},
      base::BindOnce(&ReadSetsFile, std::move(sets_file)),
      base::BindOnce(&FirstPartySetsLoader::OnReadSetsFile,
                     weak_factory_.GetWeakPtr(), std::move(version)));
}

// static
void FirstPartySetsLoader::DisposeFile(base::File file) {
  if (!file.IsValid()) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](base::File file) {
            // Run `file`'s dtor in the threadpool.
          },
          std::move(file)));
}

void FirstPartySetsLoader::OnReadSetsFile(base::Version version,
                                          const std::string& raw_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(component_sets_parse_progress_, Progress::kStarted);

  std::istringstream stream(raw_sets);
  sets_ = FirstPartySetParser::ParseSetsFromStream(stream, std::move(version),
                                                   /*emit_errors=*/false,
                                                   /*emit_metrics=*/true);

  component_sets_parse_progress_ = Progress::kFinished;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadComponentSets2",
      construction_timer_.Elapsed());
  MaybeFinishLoading();
}

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component_sets_parse_progress_ != Progress::kFinished ||
      !manually_specified_set_.has_value()) {
    return;
  }
  sets_->ApplyManuallySpecifiedSet(manually_specified_set_.value());
  std::move(on_load_complete_).Run(std::move(sets_).value());
}

}  // namespace content
