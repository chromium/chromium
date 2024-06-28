// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl_instance.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/browser/first_party_sets/first_party_sets_overrides_policy.h"
#include "content/browser/first_party_sets/first_party_sets_site_data_remover.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "net/base/features.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "net/first_party_sets/sets_mutation.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

namespace {

constexpr base::FilePath::CharType kFirstPartySetsDatabase[] =
    FILE_PATH_LITERAL("first_party_sets.db");

// Global FirstPartySetsHandler instance for testing. This should be preferred
// by tests when possible.
FirstPartySetsHandler* g_test_instance = nullptr;

// Global FirstPartySetsHandlerImpl instance for testing. This is mainly useful
// for tests that need to know about content-internal details.
FirstPartySetsHandlerImpl* g_impl_test_instance = nullptr;

base::TaskPriority GetTaskPriority() {
  return base::FeatureList::IsEnabled(net::features::kWaitForFirstPartySetsInit)
             ? base::TaskPriority::USER_BLOCKING
             : base::TaskPriority::BEST_EFFORT;
}

void RecordSitesToClearCount(int count) {
  base::UmaHistogramCounts1000(
      "FirstPartySets.Initialization.SitesToClear.Count", count);
}

}  // namespace

// static
void FirstPartySetsHandler::SetInstanceForTesting(
    FirstPartySetsHandler* test_instance) {
  g_test_instance = test_instance;
}

// static
void FirstPartySetsHandlerImpl::SetInstanceForTesting(
    FirstPartySetsHandlerImpl* test_instance) {
  g_impl_test_instance = test_instance;
}

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  if (g_test_instance) {
    return g_test_instance;
  }

  return FirstPartySetsHandlerImpl::GetInstance();
}

// static
FirstPartySetsHandlerImpl* FirstPartySetsHandlerImpl::GetInstance() {
  static base::NoDestructor<FirstPartySetsHandlerImplInstance> instance(
      GetContentClient()->browser()->IsFirstPartySetsEnabled(),
      GetContentClient()->browser()->WillProvidePublicFirstPartySets());
  if (g_impl_test_instance) {
    return g_impl_test_instance;
  }
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
FirstPartySetsHandlerImplInstance
FirstPartySetsHandlerImplInstance::CreateForTesting(
    bool enabled,
    bool embedder_will_provide_public_sets) {
  return FirstPartySetsHandlerImplInstance(enabled,
                                           embedder_will_provide_public_sets);
}

void FirstPartySetsHandlerImplInstance::GetContextConfigForPolicy(
    const base::Value::Dict* policy,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!policy) {
    std::move(callback).Run(net::FirstPartySetsContextConfig());
    return;
  }
  if (global_sets_.has_value()) {
    std::move(callback).Run(
        GetContextConfigForPolicyInternal(*policy, std::nullopt));
    return;
  }
  // Add to the deque of callbacks that will be processed once the list
  // of First-Party Sets has been fully initialized.
  EnqueuePendingTask(
      base::BindOnce(
          &FirstPartySetsHandlerImplInstance::GetContextConfigForPolicyInternal,
          // base::Unretained(this) is safe here because this is a static
          // singleton.
          base::Unretained(this), policy->Clone(), base::ElapsedTimer())
          .Then(std::move(callback)));
}

FirstPartySetsHandlerImplInstance::FirstPartySetsHandlerImplInstance(
    bool enabled,
    bool embedder_will_provide_public_sets)
    : enabled_(enabled) {
  if (enabled) {
    on_sets_ready_callbacks_ =
        std::make_unique<base::circular_deque<base::OnceClosure>>();
    sets_loader_ = std::make_unique<FirstPartySetsLoader>(
        base::BindOnce(&FirstPartySetsHandlerImplInstance::SetCompleteSets,
                       // base::Unretained(this) is safe here because
                       // this is a static singleton.
                       base::Unretained(this)));
    if (!embedder_will_provide_public_sets) {
      sets_loader_->SetComponentSets(base::Version(), base::File());
    }
  } else {
    SetCompleteSets(net::GlobalFirstPartySets());
    CHECK(global_sets_.has_value());
  }
}

FirstPartySetsHandlerImplInstance::~FirstPartySetsHandlerImplInstance() =
    default;

std::optional<net::GlobalFirstPartySets>
FirstPartySetsHandlerImplInstance::GetSets(
    base::OnceCallback<void(net::GlobalFirstPartySets)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (global_sets_.has_value()) {
    return global_sets_->Clone();
  }

  if (!callback.is_null()) {
    // base::Unretained(this) is safe here because this is a static singleton.
    EnqueuePendingTask(
        base::BindOnce(&FirstPartySetsHandlerImplInstance::GetGlobalSetsSync,
                       base::Unretained(this))
            .Then(std::move(callback)));
  }

  return std::nullopt;
}

void FirstPartySetsHandlerImplInstance::Init(
    const base::FilePath& user_data_dir,
    const net::LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_) {
    return;
  }

  initialized_ = true;
  SetDatabase(user_data_dir);

  if (sets_loader_) {
    sets_loader_->SetManuallySpecifiedSet(local_set);
  }
}

bool FirstPartySetsHandlerImplInstance::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImplInstance::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sets_loader_) {
    FirstPartySetsLoader::DisposeFile(std::move(sets_file));
    return;
  }

  // TODO(crbug.com/40186153): Use the version to compute sets diff.
  sets_loader_->SetComponentSets(version, std::move(sets_file));
}

void FirstPartySetsHandlerImplInstance::GetPersistedSetsForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<void(
        std::optional<std::pair<net::GlobalFirstPartySets,
                                net::FirstPartySetsContextConfig>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (db_helper_.is_null()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::
                     GetGlobalSetsAndConfigForTesting)  // IN-TEST
      .WithArgs(browser_context_id)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImplInstance::HasBrowserContextClearedForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<void(std::optional<bool>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!browser_context_id.empty());
  if (db_helper_.is_null()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::
                     HasEntryInBrowserContextsClearedForTesting)  // IN-TEST
      .WithArgs(browser_context_id)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImplInstance::SetCompleteSets(
    net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());
  global_sets_ = std::move(sets);
  sets_loader_.reset();

  InvokePendingQueries();
}

void FirstPartySetsHandlerImplInstance::SetDatabase(
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

void FirstPartySetsHandlerImplInstance::EnqueuePendingTask(
    base::OnceClosure run_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!global_sets_.has_value());
  CHECK(on_sets_ready_callbacks_);

  if (!first_async_task_timer_.has_value()) {
    first_async_task_timer_ = base::ElapsedTimer();
  }

  on_sets_ready_callbacks_->push_back(std::move(run_task));
}

void FirstPartySetsHandlerImplInstance::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::circular_deque<base::OnceClosure> queue;
  if (on_sets_ready_callbacks_) {
    queue.swap(*on_sets_ready_callbacks_);
  }

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
  on_sets_ready_callbacks_.reset();
}

std::optional<net::FirstPartySetEntry>
FirstPartySetsHandlerImplInstance::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    return std::nullopt;
  }
  return global_sets_->FindEntry(site, config);
}

net::GlobalFirstPartySets FirstPartySetsHandlerImplInstance::GetGlobalSetsSync()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());
  return global_sets_->Clone();
}

void FirstPartySetsHandlerImplInstance::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_) {
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
      &FirstPartySetsHandlerImplInstance::
          ClearSiteDataOnChangedSetsForContextInternal,
      base::Unretained(this), browser_context_getter, browser_context_id,
      std::move(context_config), std::move(callback)));
}

void FirstPartySetsHandlerImplInstance::
    ClearSiteDataOnChangedSetsForContextInternal(
        base::RepeatingCallback<BrowserContext*()> browser_context_getter,
        const std::string& browser_context_id,
        net::FirstPartySetsContextConfig context_config,
        base::OnceCallback<void(net::FirstPartySetsContextConfig,
                                net::FirstPartySetsCacheFilter)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());
  CHECK(!browser_context_id.empty());
  CHECK(enabled_);

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
  base::OnceCallback<void(
      std::optional<std::pair<std::vector<net::SchemefulSite>,
                              net::FirstPartySetsCacheFilter>>)>
      on_get_sites_to_clear = base::BindOnce(
          &FirstPartySetsHandlerImplInstance::OnGetSitesToClear,
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

void FirstPartySetsHandlerImplInstance::OnGetSitesToClear(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback,
    std::optional<std::pair<std::vector<net::SchemefulSite>,
                            net::FirstPartySetsCacheFilter>> sites_to_clear)
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sites_to_clear.has_value()) {
    std::move(callback).Run(std::move(context_config),
                            net::FirstPartySetsCacheFilter());
    return;
  }

  RecordSitesToClearCount(sites_to_clear->first.size());

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
      std::move(sites_to_clear->first),
      base::BindOnce(&FirstPartySetsHandlerImplInstance::
                         DidClearSiteDataOnChangedSetsForContext,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this), browser_context_id,
                     std::move(context_config),
                     std::move(sites_to_clear->second), std::move(callback)));
}

void FirstPartySetsHandlerImplInstance::DidClearSiteDataOnChangedSetsForContext(
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

void FirstPartySetsHandlerImplInstance::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const net::FirstPartySetsContextConfig& config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    EnqueuePendingTask(base::BindOnce(
        &FirstPartySetsHandlerImplInstance::
            ComputeFirstPartySetMetadataInternal,
        base::Unretained(this), site, base::OptionalFromPtr(top_frame_site),
        config.Clone(), base::ElapsedTimer(), std::move(callback)));
    return;
  }

  std::move(callback).Run(
      global_sets_->ComputeMetadata(site, top_frame_site, config));
}

void FirstPartySetsHandlerImplInstance::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    const std::optional<net::SchemefulSite>& top_frame_site,
    const net::FirstPartySetsContextConfig& config,
    const base::ElapsedTimer& timer,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());

  base::UmaHistogramTimes(
      "Cookie.FirstPartySets.EnqueueingDelay.ComputeMetadata3",
      timer.Elapsed());

  std::move(callback).Run(global_sets_->ComputeMetadata(
      site, base::OptionalToPtr(top_frame_site), config));
}

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImplInstance::GetContextConfigForPolicyInternal(
    const base::Value::Dict& policy,
    const std::optional<base::ElapsedTimer>& timer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(global_sets_.has_value());

  if (timer.has_value()) {
    base::UmaHistogramTimes(
        "Cookie.FirstPartySets.EnqueueingDelay.ContextConfig2",
        timer->Elapsed());
  }

  if (!enabled_) {
    return net::FirstPartySetsContextConfig();
  }

  auto [parsed, warnings] =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);

  if (!parsed.has_value()) {
    return global_sets_->ComputeConfig(net::SetsMutation());
  }

  FirstPartySetsOverridesPolicy& policy_result = parsed.value();
  return global_sets_->ComputeConfig(std::move(policy_result.mutation()));
}

bool FirstPartySetsHandlerImplInstance::ForEachEffectiveSetEntry(
    const net::FirstPartySetsContextConfig& config,
    base::FunctionRef<bool(const net::SchemefulSite&,
                           const net::FirstPartySetEntry&)> f) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!global_sets_.has_value()) {
    return false;
  }
  return global_sets_->ForEachEffectiveSetEntry(config, f);
}

}  // namespace content
