// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kFirstPartySetsDatabase[] =
    FILE_PATH_LITERAL("first_party_sets.db");

}  // namespace

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
std::pair<absl::optional<FirstPartySetsHandler::ParseError>,
          std::vector<FirstPartySetsHandler::ParseWarning>>
FirstPartySetsHandler::ValidateEnterprisePolicy(
    const base::Value::Dict& policy) {
  FirstPartySetParser::PolicyParseResult parsed_or_error =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  if (!parsed_or_error.has_value()) {
    return {parsed_or_error.error().first, parsed_or_error.error().second};
  }
  return {absl::nullopt, parsed_or_error.value().second};
}

void FirstPartySetsHandlerImpl::GetContextConfigForPolicy(
    const base::Value::Dict* policy,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!policy || !enabled_) {
    std::move(callback).Run(net::FirstPartySetsContextConfig());
    return;
  }
  if (global_sets_.has_value()) {
    std::move(callback).Run(GetContextConfigForPolicyInternal(*policy));
    return;
  }
  // Add to the deque of callbacks that will be processed once the list
  // of First-Party Sets has been fully initialized.
  on_sets_ready_callbacks_.push_back(
      base::BindOnce(
          &FirstPartySetsHandlerImpl::GetContextConfigForPolicyInternal,
          // base::Unretained(this) is safe here because this is a static
          // singleton.
          base::Unretained(this), policy->Clone())
          .Then(std::move(callback)));
}

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImpl::ComputeEnterpriseContextConfig(
    const net::GlobalFirstPartySets& global_sets,
    const FirstPartySetParser::ParsedPolicySetLists& policy) {
  return global_sets.ComputeConfig(
      /*replacement_sets=*/policy.replacements,
      /*addition_sets=*/
      policy.additions);
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
                     base::Unretained(this)));
}

FirstPartySetsHandlerImpl::~FirstPartySetsHandlerImpl() = default;

absl::optional<net::GlobalFirstPartySets> FirstPartySetsHandlerImpl::GetSets(
    SetsReadyOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsEnabled());
  if (global_sets_.has_value())
    return global_sets_->Clone();

  if (!callback.is_null()) {
    // base::Unretained(this) is safe here because this is a static singleton.
    on_sets_ready_callbacks_.push_back(
        base::BindOnce(&FirstPartySetsHandlerImpl::GetGlobalSetsSync,
                       base::Unretained(this))
            .Then(std::move(callback)));
  }

  return absl::nullopt;
}

void FirstPartySetsHandlerImpl::Init(const base::FilePath& user_data_dir,
                                     const LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);

  initialized_ = true;
  SetDatabase(user_data_dir);

  if (IsEnabled()) {
    sets_loader_->SetManuallySpecifiedSet(local_set);
    if (!embedder_will_provide_public_sets_) {
      sets_loader_->SetComponentSets(base::File());
    }
  } else {
    SetCompleteSets(net::GlobalFirstPartySets());
  }
}

bool FirstPartySetsHandlerImpl::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enabled_);
  DCHECK(embedder_will_provide_public_sets_);

  // TODO(crbug.com/1219656): Use this value to compute sets diff.
  version_ = version;
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
                     base::Unretained(this)));
  on_sets_ready_callbacks_.clear();
  global_sets_ = absl::nullopt;
  db_helper_.Reset();
}

const net::GlobalFirstPartySets*
FirstPartySetsHandlerImpl::GetGlobalSetsIfReady() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::OptionalToPtr(global_sets_);
}

void FirstPartySetsHandlerImpl::GetPersistedGlobalSetsForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<void(absl::optional<net::GlobalFirstPartySets>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());
  if (db_helper_.is_null()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::GetPersistedGlobalSets)
      .WithArgs(browser_context_id)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImpl::SetCompleteSets(
    net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!global_sets_.has_value());
  global_sets_ = std::move(sets);

  if (IsEnabled())
    InvokePendingQueries();
}

void FirstPartySetsHandlerImpl::SetDatabase(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_helper_.is_null());

  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed initializing First-Party Sets database.";
    return;
  }
  db_helper_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
                         {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                     user_data_dir.Append(kFirstPartySetsDatabase));
}

void FirstPartySetsHandlerImpl::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enabled_);
  base::circular_deque<base::OnceClosure> queue;
  queue.swap(on_sets_ready_callbacks_);
  while (!queue.empty()) {
    base::OnceCallback callback = std::move(queue.front());
    queue.pop_front();
    std::move(callback).Run();
  }
}

net::GlobalFirstPartySets FirstPartySetsHandlerImpl::GetGlobalSetsSync() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const net::GlobalFirstPartySets* sets = GetGlobalSetsIfReady();
  DCHECK(sets);
  return sets->Clone();
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_) {
    std::move(callback).Run(std::move(context_config));
    return;
  }

  if (global_sets_.has_value()) {
    ClearSiteDataOnChangedSetsForContextInternal(
        browser_context_getter, browser_context_id, std::move(context_config),
        std::move(callback));
    return;
  }

  // base::Unretained(this) is safe because this is a static singleton.
  on_sets_ready_callbacks_.push_back(base::BindOnce(
      &FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContextInternal,
      base::Unretained(this), browser_context_getter, browser_context_id,
      std::move(context_config), std::move(callback)));
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContextInternal(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(global_sets_.has_value());
  DCHECK(!browser_context_id.empty());
  DCHECK(enabled_);

  if (!db_helper_.is_null()) {
    // TODO(crbug.com/1219656): Call site state clearing.
    // TODO(https://crbug.com/1219656): don't invoke `callback` until site state
    // clearing is complete.
    db_helper_.AsyncCall(&FirstPartySetsHandlerDatabaseHelper::PersistSets)
        .WithArgs(browser_context_id, version_, global_sets_->Clone(),
                  context_config.Clone());
  }
  std::move(callback).Run(std::move(context_config));
}

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImpl::GetContextConfigForPolicyInternal(
    const base::Value::Dict& policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FirstPartySetParser::PolicyParseResult parsed_or_error =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  // Provide empty customization if the policy is malformed.
  return parsed_or_error.has_value()
             ? FirstPartySetsHandlerImpl::ComputeEnterpriseContextConfig(
                   global_sets_.value(), parsed_or_error.value().first)
             : net::FirstPartySetsContextConfig();
}

}  // namespace content
