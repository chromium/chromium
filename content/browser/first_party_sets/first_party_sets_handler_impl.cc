// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kPersistedFirstPartySetsFileName[] =
    FILE_PATH_LITERAL("persisted_first_party_sets.json");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and empty string on failure.
std::string LoadSetsFromDisk(const base::FilePath& path) {
  DCHECK(!path.empty());

  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    VLOG(1) << "Failed loading serialized First-Party Sets file from "
            << path.MaybeAsASCII();
    return "";
  }
  return result;
}

// Writes the sets as raw JSON to the storage file.
void MaybeWriteSetsToDisk(const base::FilePath& path, base::StringPiece sets) {
  DCHECK(!path.empty());
  if (!base::ImportantFileWriter::WriteFileAtomically(path, sets)) {
    VLOG(1) << "Failed writing serialized First-Party Sets to file "
            << path.MaybeAsASCII();
  }
}

}  // namespace

bool FirstPartySetsHandler::PolicyParsingError::operator==(
    const FirstPartySetsHandler::PolicyParsingError& other) const {
  return std::tie(error, set_type, error_index) ==
         std::tie(other.error, other.set_type, other.error_index);
}

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  return FirstPartySetsHandlerImpl::GetInstance();
}

// static
FirstPartySetsHandlerImpl* FirstPartySetsHandlerImpl::GetInstance() {
  static base::NoDestructor<FirstPartySetsHandlerImpl> instance(
      GetContentClient()->browser()->IsFirstPartySetsEnabled(),
      GetContentClient()->browser()->WillProvidePublicFirstPartySets());
  return instance.get();
}

// static
absl::optional<FirstPartySetsHandler::PolicyParsingError>
FirstPartySetsHandler::ValidateEnterprisePolicy(
    const base::Value::Dict& policy) {
  // Call ParseSetsFromEnterprisePolicy to determine if the all sets in the
  // policy are valid First-Party Sets. A nullptr is provided since we don't
  // have use for the actual parsed sets.
  return FirstPartySetParser::ParseSetsFromEnterprisePolicy(
      policy, /*out_sets=*/nullptr);
}

FirstPartySetsHandlerImpl::FirstPartySetsHandlerImpl(
    bool enabled,
    bool embedder_will_provide_public_sets)
    : enabled_(enabled),
      embedder_will_provide_public_sets_(enabled &&
                                         embedder_will_provide_public_sets) {
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(
      base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this)),
      IsEnabled() ? GetContentClient()->browser()->GetFirstPartySetsOverrides()
                  : base::Value::Dict());
}

FirstPartySetsHandlerImpl::~FirstPartySetsHandlerImpl() = default;

absl::optional<FirstPartySetsHandlerImpl::FlattenedSets>
FirstPartySetsHandlerImpl::GetSets(SetsReadyOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsEnabled());
  DCHECK(!callback.is_null());
  if (sets_.has_value())
    return sets_;

  on_sets_ready_callbacks_.push_back(std::move(callback));
  return absl::nullopt;
}

void FirstPartySetsHandlerImpl::Init(const base::FilePath& user_data_dir,
                                     const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
  DCHECK(persisted_sets_path_.empty());

  initialized_ = true;
  SetPersistedSets(user_data_dir);

  if (IsEnabled()) {
    sets_loader_->SetManuallySpecifiedSet(flag_value);
    if (!embedder_will_provide_public_sets_) {
      sets_loader_->SetComponentSets(base::File());
    }
  } else {
    SetCompleteSets({});
  }
}

bool FirstPartySetsHandlerImpl::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enabled_);
  DCHECK(embedder_will_provide_public_sets_);
  sets_loader_->SetComponentSets(std::move(sets_file));
}

void FirstPartySetsHandlerImpl::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialized_ = false;
  enabled_ = GetContentClient()->browser()->IsFirstPartySetsEnabled();
  embedder_will_provide_public_sets_ =
      GetContentClient()->browser()->WillProvidePublicFirstPartySets();

  // Initializes the `sets_loader_` member with a callback to SetCompleteSets
  // and the result of content::GetFirstPartySetsOverrides.
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(
      base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this)),
      IsEnabled() ? GetContentClient()->browser()->GetFirstPartySetsOverrides()
                  : base::Value::Dict());
  on_sets_ready_callbacks_.clear();
  persisted_sets_path_ = base::FilePath();
  sets_ = absl::nullopt;
  raw_persisted_sets_ = absl::nullopt;
}

void FirstPartySetsHandlerImpl::SetPersistedSets(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!raw_persisted_sets_.has_value());
  DCHECK(persisted_sets_path_.empty());
  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed loading serialized First-Party Sets file.";
    // We have to continue, in case the embedder has enabled FPS but has not
    // provided a directory to store persisted sets.
    OnReadPersistedSetsFile("");
    return;
  }
  persisted_sets_path_ = user_data_dir.Append(kPersistedFirstPartySetsFileName);

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  //
  // base::Unretained(this) is safe because this is a static singleton.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&LoadSetsFromDisk, persisted_sets_path_),
      base::BindOnce(&FirstPartySetsHandlerImpl::OnReadPersistedSetsFile,
                     base::Unretained(this)));
}

void FirstPartySetsHandlerImpl::OnReadPersistedSetsFile(
    const std::string& raw_persisted_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!raw_persisted_sets_.has_value());
  raw_persisted_sets_ = raw_persisted_sets;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadPersistedSets2",
      construction_timer_.Elapsed());

  if (sets_.has_value()) {
    ClearSiteDataOnChangedSets();

    if (IsEnabled()) {
      InvokePendingQueries();
    }
  }
}

void FirstPartySetsHandlerImpl::SetCompleteSets(
    base::flat_map<net::SchemefulSite, net::SchemefulSite> sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!sets_.has_value());
  sets_ = std::move(sets);

  if (raw_persisted_sets_.has_value()) {
    ClearSiteDataOnChangedSets();

    if (IsEnabled()) {
      InvokePendingQueries();
    }
  }
}

void FirstPartySetsHandlerImpl::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!on_sets_ready_callbacks_.empty()) {
    SetsReadyOnceCallback callback =
        std::move(on_sets_ready_callbacks_.front());
    on_sets_ready_callbacks_.pop_front();
    std::move(callback).Run(sets_.value());
  }
  on_sets_ready_callbacks_.shrink_to_fit();
}

// static
base::flat_set<net::SchemefulSite> FirstPartySetsHandlerImpl::ComputeSetsDiff(
    const base::flat_map<net::SchemefulSite, net::SchemefulSite>& old_sets,
    const base::flat_map<net::SchemefulSite, net::SchemefulSite>&
        current_sets) {
  if (old_sets.empty())
    return {};

  std::vector<net::SchemefulSite> result;
  if (current_sets.empty()) {
    result.reserve(old_sets.size());
    for (const auto& pair : old_sets) {
      result.push_back(pair.first);
    }
    return result;
  }
  for (const auto& old_pair : old_sets) {
    const net::SchemefulSite& old_member = old_pair.first;
    const net::SchemefulSite& old_owner = old_pair.second;

    const auto current_pair = current_sets.find(old_member);
    // Look for the removed sites and the ones have owner changed.
    if (current_pair == current_sets.end() ||
        current_pair->second != old_owner) {
      result.push_back(old_member);
    }
  }
  return result;
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSets() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  DCHECK(raw_persisted_sets_.has_value());

  base::flat_set<net::SchemefulSite> diff =
      ComputeSetsDiff(FirstPartySetParser::DeserializeFirstPartySets(
                          raw_persisted_sets_.value()),
                      sets_.value());

  // TODO(shuuran@chromium.org): Implement site state clearing.

  if (!persisted_sets_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &MaybeWriteSetsToDisk, persisted_sets_path_,
            FirstPartySetParser::SerializeFirstPartySets(sets_.value())));
  }
}

}  // namespace content
