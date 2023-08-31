// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/browser/first_party_sets/first_party_sets_site_data_remover.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

namespace {

constexpr base::FilePath::CharType kFirstPartySetsDatabase[] =
    FILE_PATH_LITERAL("first_party_sets.db");

// Global FirstPartySetsHandler instance for testing.
FirstPartySetsHandler* g_test_instance = nullptr;

base::TaskPriority GetTaskPriority() {
  return base::FeatureList::IsEnabled(net::features::kWaitForFirstPartySetsInit)
             ? base::TaskPriority::USER_BLOCKING
             : base::TaskPriority::BEST_EFFORT;
}

}  // namespace

// static
void FirstPartySetsHandler::SetInstanceForTesting(
    FirstPartySetsHandler* test_instance) {
  g_test_instance = test_instance;
}

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  if (g_test_instance)
    return g_test_instance;

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
std::pair<base::expected<void, FirstPartySetsHandler::ParseError>,
          std::vector<FirstPartySetsHandler::ParseWarning>>
FirstPartySetsHandler::ValidateEnterprisePolicy(
    const base::Value::Dict& policy) {
  auto [parsed, warnings] =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);

  const auto discard_value = [](const auto&)
      -> base::expected<void, FirstPartySetsHandler::ParseError> {
    return base::ok();
  };
  return {parsed.and_then(discard_value), warnings};
}

// static
FirstPartySetsHandlerImpl FirstPartySetsHandlerImpl::CreateForTesting(
    bool enabled,
    bool embedder_will_provide_public_sets) {
  return FirstPartySetsHandlerImpl(enabled, embedder_will_provide_public_sets);
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
    std::move(callback).Run(
        GetContextConfigForPolicyInternal(*policy, absl::nullopt));
    return;
  }
  // Add to the deque of callbacks that will be processed once the list
  // of First-Party Sets has been fully initialized.
  EnqueuePendingTask(
      base::BindOnce(
          &FirstPartySetsHandlerImpl::GetContextConfigForPolicyInternal,
          // base::Unretained(this) is safe here because this is a static
          // singleton.
          base::Unretained(this), policy->Clone(), base::ElapsedTimer())
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
                                         embedder_will_provide_public_sets),
      sets_loader_(std::make_unique<FirstPartySetsLoader>(
          base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                         // base::Unretained(this) is safe here because
                         // this is a static singleton.
                         base::Unretained(this)))) {}

FirstPartySetsHandlerImpl::~FirstPartySetsHandlerImpl() = default;

absl::optional<net::GlobalFirstPartySets> FirstPartySetsHandlerImpl::GetSets(
    SetsReadyOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsEnabled());
  if (global_sets_.has_value())
    return global_sets_->Clone();

  if (!callback.is_null()) {
    // base::Unretained(this) is safe here because this is a static singleton.
    EnqueuePendingTask(
        base::BindOnce(&FirstPartySetsHandlerImpl::GetGlobalSetsSync,
                       base::Unretained(this))
            .Then(std::move(callback)));
  }

  return absl::nullopt;
}

void FirstPartySetsHandlerImpl::Init(const base::FilePath& user_data_dir,
                                     const LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!initialized_);
  CHECK(sets_loader_);

  initialized_ = true;
  SetDatabase(user_data_dir);

  if (IsEnabled()) {
    sets_loader_->SetManuallySpecifiedSet(local_set);
    if (!embedder_will_provide_public_sets_) {
      sets_loader_->SetComponentSets(base::Version(), base::File());
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
  CHECK(enabled_);
  CHECK(embedder_will_provide_public_sets_);
  CHECK(sets_loader_);

  // TODO(crbug.com/1219656): Use the version to compute sets diff.
  sets_loader_->SetComponentSets(version, std::move(sets_file));
}

void FirstPartySetsHandlerImpl::GetPersistedSetsForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<
        void(absl::optional<std::pair<net::GlobalFirstPartySets,
                                      net::FirstPartySetsContextConfig>>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (db_helper_.is_null()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::
                     GetGlobalSetsAndConfigForTesting)  // IN-TEST
      .WithArgs(browser_context_id)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImpl::HasBrowserContextClearedForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<void(absl::optional<bool>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (db_helper_.is_null()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::
                     HasEntryInBrowserContextsClearedForTesting)  // IN-TEST
      .WithArgs(browser_context_id)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImpl::SetCompleteSets(
    net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());
  CHECK(sets_loader_);
  global_sets_ = std::move(sets);
  sets_loader_.reset();

  InvokePendingQueries();
}

void FirstPartySetsHandlerImpl::SetDatabase(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_helper_.is_null());

  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed initializing First-Party Sets database.";
    return;
  }
  db_helper_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
                         {base::MayBlock(), GetTaskPriority(),
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                     user_data_dir.Append(kFirstPartySetsDatabase));
}

void FirstPartySetsHandlerImpl::EnqueuePendingTask(base::OnceClosure run_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());

  if (!first_async_task_timer_.has_value()) {
    first_async_task_timer_ = base::ElapsedTimer();
  }

  on_sets_ready_callbacks_.push_back(std::move(run_task));
}

void FirstPartySetsHandlerImpl::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::circular_deque<base::OnceClosure> queue;
  queue.swap(on_sets_ready_callbacks_);

  base::UmaHistogramCounts10000(
      "Cookie.FirstPartySets.Browser.DelayedQueriesCount", queue.size());
  base::UmaHistogramTimes("Cookie.FirstPartySets.Browser.MostDelayedQueryDelta",
                          first_async_task_timer_.has_value()
                              ? first_async_task_timer_->Elapsed()
                              : base::TimeDelta());

  while (!queue.empty()) {
    base::OnceCallback callback = std::move(queue.front());
    queue.pop_front();
    std::move(callback).Run();
  }
}

absl::optional<net::FirstPartySetEntry> FirstPartySetsHandlerImpl::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets) ||
      !global_sets_.has_value()) {
    return absl::nullopt;
  }
  return global_sets_->FindEntry(site, config);
}

net::GlobalFirstPartySets FirstPartySetsHandlerImpl::GetGlobalSetsSync() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());
  return global_sets_->Clone();
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_ || !features::kFirstPartySetsClearSiteDataOnChangedSets.Get()) {
    std::move(callback).Run(std::move(context_config),
                            net::FirstPartySetsCacheFilter());
    return;
  }

  if (global_sets_.has_value()) {
    ClearSiteDataOnChangedSetsForContextInternal(
        browser_context_getter, browser_context_id, std::move(context_config),
        std::move(callback));
    return;
  }

  // base::Unretained(this) is safe because this is a static singleton.
  EnqueuePendingTask(base::BindOnce(
      &FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContextInternal,
      base::Unretained(this), browser_context_getter, browser_context_id,
      std::move(context_config), std::move(callback)));
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContextInternal(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());
  CHECK(!browser_context_id.empty());
  CHECK(enabled_ && features::kFirstPartySetsClearSiteDataOnChangedSets.Get());

  if (db_helper_.is_null()) {
    VLOG(1) << "Invalid First-Party Sets database. Failed to clear site data "
               "for browser_context_id="
            << browser_context_id;
    std::move(callback).Run(std::move(context_config),
                            net::FirstPartySetsCacheFilter());
    return;
  }

  // Extract the callback into a variable and pass it into DB async call args,
  // to prevent the case that `context_config` gets used after it's moved. This
  // is because C++ does not have a defined evaluation order for function
  // parameters.
  base::OnceCallback<void(std::pair<std::vector<net::SchemefulSite>,
                                    net::FirstPartySetsCacheFilter>)>
      on_get_sites_to_clear = base::BindOnce(
          &FirstPartySetsHandlerImpl::OnGetSitesToClear,
          // base::Unretained(this) is safe here because this
          // is a static singleton.
          base::Unretained(this), browser_context_getter, browser_context_id,
          context_config.Clone(), std::move(callback));

  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::
                     UpdateAndGetSitesToClearForContext)
      .WithArgs(browser_context_id, global_sets_->Clone(),
                std::move(context_config))
      .Then(std::move(on_get_sites_to_clear));
}

void FirstPartySetsHandlerImpl::OnGetSitesToClear(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback,
    std::pair<std::vector<net::SchemefulSite>, net::FirstPartySetsCacheFilter>
        sites_to_clear) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BrowserContext* browser_context = browser_context_getter.Run();
  if (!browser_context) {
    DVLOG(1) << "Invalid Browser Context. Failed to clear site data for "
                "browser_context_id="
             << browser_context_id;

    std::move(callback).Run(std::move(context_config),
                            net::FirstPartySetsCacheFilter());
    return;
  }

  FirstPartySetsSiteDataRemover::RemoveSiteData(
      *browser_context->GetBrowsingDataRemover(),
      std::move(sites_to_clear.first),
      base::BindOnce(
          &FirstPartySetsHandlerImpl::DidClearSiteDataOnChangedSetsForContext,
          // base::Unretained(this) is safe here because
          // this is a static singleton.
          base::Unretained(this), browser_context_id, std::move(context_config),
          std::move(sites_to_clear.second), std::move(callback)));
}

void FirstPartySetsHandlerImpl::DidClearSiteDataOnChangedSetsForContext(
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    net::FirstPartySetsCacheFilter cache_filter,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback,
    uint64_t failed_data_types) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!db_helper_.is_null());

  // Only measures the successful rate without parsing the failed types, since
  // `failed_data_types` only has value if the failure is related to passwords
  // or is for all data types if the task is dropped at shutdown, which is not
  // for our interest.
  bool success = failed_data_types == 0;
  base::UmaHistogramBoolean(
      "FirstPartySets.Initialization.ClearSiteDataOutcome", success);
  if (success) {
    db_helper_
        .AsyncCall(
            &FirstPartySetsHandlerDatabaseHelper::UpdateClearStatusForContext)
        .WithArgs(browser_context_id);
  }

  db_helper_.AsyncCall(&FirstPartySetsHandlerDatabaseHelper::PersistSets)
      .WithArgs(browser_context_id, global_sets_->Clone(),
                context_config.Clone());
  std::move(callback).Run(std::move(context_config), std::move(cache_filter));
}

void FirstPartySetsHandlerImpl::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const net::FirstPartySetsContextConfig& config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    EnqueuePendingTask(base::BindOnce(
        &FirstPartySetsHandlerImpl::ComputeFirstPartySetMetadataInternal,
        base::Unretained(this), site, base::OptionalFromPtr(top_frame_site),
        config.Clone(), base::ElapsedTimer(), std::move(callback)));
    return;
  }

  std::move(callback).Run(
      global_sets_->ComputeMetadata(site, top_frame_site, config));
}

void FirstPartySetsHandlerImpl::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    const absl::optional<net::SchemefulSite>& top_frame_site,
    const net::FirstPartySetsContextConfig& config,
    const base::ElapsedTimer& timer,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());

  base::UmaHistogramTimes(
      "Cookie.FirstPartySets.EnqueueingDelay.ComputeMetadata2",
      timer.Elapsed());

  std::move(callback).Run(global_sets_->ComputeMetadata(
      site, base::OptionalToPtr(top_frame_site), config));
}

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImpl::GetContextConfigForPolicyInternal(
    const base::Value::Dict& policy,
    const absl::optional<base::ElapsedTimer>& timer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());

  if (timer.has_value()) {
    base::UmaHistogramTimes(
        "Cookie.FirstPartySets.EnqueueingDelay.ContextConfig2",
        timer->Elapsed());
  }

  auto [parsed, warnings] =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);

  return parsed.has_value()
             ? FirstPartySetsHandlerImpl::ComputeEnterpriseContextConfig(
                   global_sets_.value(), parsed.value())
             : net::FirstPartySetsContextConfig();
}

}  // namespace content
