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
      GetContentClient()->browser()->IsFirstPartySetsEnabled());
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

FirstPartySetsHandlerImpl::FirstPartySetsHandlerImpl(bool enabled)
    : enabled_(enabled) {
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
FirstPartySetsHandlerImpl::GetSetsIfEnabledAndReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsEnabledAndReady() ? sets_ : absl::nullopt;
}

void FirstPartySetsHandlerImpl::Init(const base::FilePath& user_data_dir,
                                     const std::string& flag_value,
                                     SetsReadyOnceCallback on_sets_ready) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_sets_ready_ = std::move(on_sets_ready);
  SetPersistedSets(user_data_dir);
  SetManuallySpecifiedSet(flag_value);

  if (!IsEnabled())
    SetCompleteSets({});
}

bool FirstPartySetsHandlerImpl::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    sets_loader_->DisposeFile(std::move(sets_file));
    return;
  }
  sets_loader_->SetComponentSets(std::move(sets_file));
}

void FirstPartySetsHandlerImpl::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = GetContentClient()->browser()->IsFirstPartySetsEnabled();

  // Initializes the `sets_loader_` member with a callback to SetCompleteSets
  // and the result of content::GetFirstPartySetsOverrides.
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(
      base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this)),
      IsEnabled() ? GetContentClient()->browser()->GetFirstPartySetsOverrides()
                  : base::Value::Dict());
  on_sets_ready_.Reset();
  persisted_sets_path_ = base::FilePath();
  sets_ = absl::nullopt;
  raw_persisted_sets_ = absl::nullopt;
}

void FirstPartySetsHandlerImpl::SetManuallySpecifiedSet(
    const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled())
    return;
  sets_loader_->SetManuallySpecifiedSet(flag_value);
}

void FirstPartySetsHandlerImpl::SetPersistedSets(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed loading serialized First-Party Sets file.";
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
  DCHECK(!persisted_sets_path_.empty());
  raw_persisted_sets_ = raw_persisted_sets;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadPersistedSets",
      construction_timer_.Elapsed());
  ClearSiteDataOnChangedSetsIfReady();
}

void FirstPartySetsHandlerImpl::SetCompleteSets(
    base::flat_map<net::SchemefulSite, net::SchemefulSite> sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sets_ = std::move(sets);
  ClearSiteDataOnChangedSetsIfReady();
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

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!raw_persisted_sets_.has_value() || !sets_.has_value())
    return;

  base::flat_set<net::SchemefulSite> diff =
      ComputeSetsDiff(FirstPartySetParser::DeserializeFirstPartySets(
                          raw_persisted_sets_.value()),
                      sets_.value());

  // TODO(shuuran@chromium.org): Implement site state clearing.

  if (!on_sets_ready_.is_null() && IsEnabledAndReady())
    std::move(on_sets_ready_).Run(sets_.value());

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &MaybeWriteSetsToDisk, persisted_sets_path_,
          FirstPartySetParser::SerializeFirstPartySets(sets_.value())));
}

bool FirstPartySetsHandlerImpl::IsEnabledAndReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsEnabled() && sets_.has_value();
}

}  // namespace content
