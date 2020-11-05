// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RulesMonitorService>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

bool RulesetInfoCompareByID(const RulesetInfo& lhs, const RulesetInfo& rhs) {
  return lhs.source().id() < rhs.source().id();
}

void LogLoadRulesetResult(LoadRulesetResult result) {
  UMA_HISTOGRAM_ENUMERATION(kLoadRulesetResultHistogram, result);
}

}  // namespace

// Helper to bridge tasks to FileSequenceHelper. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_helper_(std::make_unique<FileSequenceHelper>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_helper_));
  }

  void LoadRulesets(
      LoadRequestData load_data,
      FileSequenceHelper::LoadRulesetsUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_helper_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceHelper::LoadRulesets,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

  void UpdateDynamicRules(
      LoadRequestData load_data,
      std::vector<int> rule_ids_to_remove,
      std::vector<dnr_api::Rule> rules_to_add,
      FileSequenceHelper::UpdateDynamicRulesUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |update_dynamic_rules_task| is run.
    base::OnceClosure update_dynamic_rules_task =
        base::BindOnce(&FileSequenceHelper::UpdateDynamicRules,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(rule_ids_to_remove),
                       std::move(rules_to_add), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE,
                                std::move(update_dynamic_rules_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceHelper> file_sequence_helper_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceBridge);
};

// static
BrowserContextKeyedAPIFactory<RulesMonitorService>*
RulesMonitorService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
std::unique_ptr<RulesMonitorService>
RulesMonitorService::CreateInstanceForTesting(
    content::BrowserContext* context) {
  return base::WrapUnique(new RulesMonitorService(context));
}

// static
RulesMonitorService* RulesMonitorService::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<RulesMonitorService>::Get(
      browser_context);
}

void RulesMonitorService::UpdateDynamicRules(
    const Extension& extension,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    DynamicRuleUpdateUICallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  ExecuteOrQueueAPICall(
      extension,
      base::BindOnce(&RulesMonitorService::UpdateDynamicRulesInternal,
                     weak_factory_.GetWeakPtr(), extension.id(),
                     std::move(rule_ids_to_remove), std::move(rules_to_add),
                     std::move(callback)));
}

void RulesMonitorService::UpdateEnabledStaticRulesets(
    const Extension& extension,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    UpdateEnabledRulesetsUICallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  ExecuteOrQueueAPICall(
      extension,
      base::BindOnce(&RulesMonitorService::UpdateEnabledStaticRulesetsInternal,
                     weak_factory_.GetWeakPtr(), extension.id(),
                     std::move(ids_to_disable), std::move(ids_to_enable),
                     std::move(callback)));
}

RulesMonitorService::RulesMonitorService(
    content::BrowserContext* browser_context)
    : file_sequence_bridge_(std::make_unique<FileSequenceBridge>()),
      prefs_(ExtensionPrefs::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      context_(browser_context),
      ruleset_manager_(browser_context),
      action_tracker_(browser_context),
      global_rules_tracker_(prefs_, extension_registry_) {
  // Don't monitor extension lifecycle if the API is not available. This is
  // useful since we base some of our actions (like loading dynamic ruleset on
  // extension load) on the presence of certain extension prefs. These may still
  // be remaining from an earlier install on which the feature was available.
  if (IsAPIAvailable())
    registry_observer_.Add(extension_registry_);
}

RulesMonitorService::~RulesMonitorService() = default;

/* Description of thread hops for various scenarios:

   On ruleset load success:
      - UI -> File -> UI.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset load failure:
      - UI -> File -> UI.
      - The File sequence might reindex the ruleset while parsing JSON OOP.

   On ruleset unload:
      - UI.

   On dynamic rules update.
      - UI -> File -> UI -> IPC to extension
*/

void RulesMonitorService::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (!is_update || Manifest::IsUnpackedLocation(extension->location()))
    return;

  if (!base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules))
    return;

  // Allow the extension to retain its pre-update allocation during the next
  // extension load. This can allow the extension to enable some
  // non-manifest-enabled rulesets and to retain much of its pre-update
  // behavior. The preference is set in OnExtensionWillBeInstalled instead of
  // OnExtensionInstalled because OnExtensionInstalled is called after
  // OnExtensionLoaded.
  prefs_->SetDNRKeepExcessAllocation(extension->id(), true);
}

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK_EQ(context_, browser_context);

  LoadRequestData load_data(extension->id());
  int expected_ruleset_checksum;

  // Static rulesets.
  {
    std::vector<RulesetSource> sources =
        RulesetSource::CreateStatic(*extension);

    base::Optional<std::set<RulesetID>> prefs_enabled_rulesets =
        prefs_->GetDNREnabledStaticRulesets(extension->id());

    bool ruleset_failed_to_load = false;
    for (auto& source : sources) {
      bool enabled = prefs_enabled_rulesets
                         ? base::Contains(*prefs_enabled_rulesets, source.id())
                         : source.enabled_by_default();

      bool ignored =
          prefs_->ShouldIgnoreDNRRuleset(extension->id(), source.id());

      if (!enabled || ignored)
        continue;

      if (!prefs_->GetDNRStaticRulesetChecksum(extension->id(), source.id(),
                                               &expected_ruleset_checksum)) {
        // This might happen on prefs corruption.
        LogLoadRulesetResult(LoadRulesetResult::kErrorChecksumNotFound);
        ruleset_failed_to_load = true;
        continue;
      }

      RulesetInfo static_ruleset(std::move(source));
      static_ruleset.set_expected_checksum(expected_ruleset_checksum);
      load_data.rulesets.push_back(std::move(static_ruleset));
    }

    if (ruleset_failed_to_load) {
      warning_service_->AddWarnings(
          {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
    }
  }

  // Dynamic ruleset
  if (prefs_->GetDNRDynamicRulesetChecksum(extension->id(),
                                           &expected_ruleset_checksum)) {
    RulesetInfo dynamic_ruleset(
        RulesetSource::CreateDynamic(browser_context, extension->id()));
    dynamic_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(dynamic_ruleset));
  }

  if (load_data.rulesets.empty()) {
    if (test_observer_)
      test_observer_->OnRulesetLoadComplete(extension->id());

    return;
  }

  // Add an entry for the extension in |tasks_pending_on_load_| to indicate that
  // it's loading its rulesets.
  bool inserted =
      tasks_pending_on_load_
          .emplace(extension->id(), std::make_unique<base::OneShotEvent>())
          .second;
  DCHECK(inserted);

  auto load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnInitialRulesetsLoaded,
                     weak_factory_.GetWeakPtr());
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(context_, browser_context);

  // If the extension is unloaded for any reason other than an update, the
  // unused rule allocation should not be kept for this extension the next time
  // its rulesets are loaded, as it is no longer "the first load after an
  // update".
  if (base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules) &&
      reason != UnloadedExtensionReason::UPDATE) {
    prefs_->SetDNRKeepExcessAllocation(extension->id(), false);
  }

  // Return early if the extension does not have an active indexed ruleset.
  if (!ruleset_manager_.GetMatcherForExtension(extension->id()))
    return;

  RemoveCompositeMatcher(extension->id());
}

void RulesMonitorService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(context_, browser_context);

  // Skip if the extension will be reinstalled soon.
  if (reason == UNINSTALL_REASON_REINSTALL)
    return;

  if (base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules))
    global_rules_tracker_.ClearExtensionAllocation(extension->id());

  // Skip if the extension doesn't have a dynamic ruleset.
  int dynamic_checksum;
  if (!prefs_->GetDNRDynamicRulesetChecksum(extension->id(),
                                            &dynamic_checksum)) {
    return;
  }

  // Cleanup the dynamic rules directory for the extension.
  // TODO(karandeepb): It's possible that this task fails, e.g. during shutdown.
  // Make this more robust.
  RulesetSource source =
      RulesetSource::CreateDynamic(browser_context, extension->id());
  DCHECK_EQ(source.json_path().DirName(), source.indexed_path().DirName());
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(base::GetDeleteFileCallback(),
                                source.json_path().DirName()));
}

void RulesMonitorService::UpdateDynamicRulesInternal(
    const ExtensionId& extension_id,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    DynamicRuleUpdateUICallback callback) {
  if (!extension_registry_->enabled_extensions().Contains(extension_id)) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related book-keeping is
    // done.
    std::move(callback).Run(base::nullopt /* error */);
    return;
  }

  LoadRequestData data(extension_id);

  // We are updating the indexed ruleset. Don't set the expected checksum since
  // it'll change.
  data.rulesets.emplace_back(
      RulesetSource::CreateDynamic(context_, extension_id));

  auto update_rules_callback =
      base::BindOnce(&RulesMonitorService::OnDynamicRulesUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  file_sequence_bridge_->UpdateDynamicRules(
      std::move(data), std::move(rule_ids_to_remove), std::move(rules_to_add),
      std::move(update_rules_callback));
}

void RulesMonitorService::UpdateEnabledStaticRulesetsInternal(
    const ExtensionId& extension_id,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    UpdateEnabledRulesetsUICallback callback) {
  const Extension* extension = extension_registry_->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related book-keeping is
    // done.
    std::move(callback).Run(base::nullopt /* error */);
    return;
  }

  LoadRequestData load_data(extension_id);

  // Don't short-circuit the case of |ids_to_enable| being empty by calling
  // OnNewStaticRulesetsLoaded directly. This can interfere with the expected
  // FIFO ordering of updateEnabledRulesets calls.

  int expected_ruleset_checksum = -1;
  for (const RulesetID& id_to_enable : ids_to_enable) {
    if (!prefs_->GetDNRStaticRulesetChecksum(extension_id, id_to_enable,
                                             &expected_ruleset_checksum)) {
      // This might happen on prefs corruption.
      LogLoadRulesetResult(LoadRulesetResult::kErrorChecksumNotFound);
      std::move(callback).Run(kInternalErrorUpdatingEnabledRulesets);
      return;
    }

    const DNRManifestData::RulesetInfo& info =
        DNRManifestData::GetRuleset(*extension, id_to_enable);
    RulesetInfo static_ruleset(RulesetSource::CreateStatic(*extension, info));
    static_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(static_ruleset));
  }

  auto load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnNewStaticRulesetsLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ids_to_disable), std::move(ids_to_enable));
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::OnInitialRulesetsLoaded(LoadRequestData load_data) {
  DCHECK(!load_data.rulesets.empty());

  // Signal ruleset load completion.
  {
    auto it = tasks_pending_on_load_.find(load_data.extension_id);
    DCHECK(it != tasks_pending_on_load_.end());
    DCHECK(!it->second->is_signaled());
    it->second->Signal();
    tasks_pending_on_load_.erase(it);
  }

  if (test_observer_)
    test_observer_->OnRulesetLoadComplete(load_data.extension_id);

  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    return;
  }

  // Sort by ruleset IDs. This would ensure the dynamic ruleset comes first
  // followed by static rulesets, which would be in the order in which they were
  // defined in the manifest.
  std::sort(load_data.rulesets.begin(), load_data.rulesets.end(),
            &RulesetInfoCompareByID);

  // Build the CompositeMatcher for the extension. Also enforce rules limit
  // across the enabled static rulesets. Note: we don't enforce the rules limit
  // at install time (by raising a hard error) to maintain forwards
  // compatibility. Since we iterate based on the order of ruleset ID, we'll
  // give more preference to rulesets occurring first in the manifest.
  CompositeMatcher::MatcherList matchers;
  matchers.reserve(load_data.rulesets.size());
  size_t static_rules_count = 0;
  size_t static_regex_rules_count = 0;
  bool notify_ruleset_failed_to_load = false;
  bool global_rule_limit_exceeded = false;

  bool global_rules_enabled =
      base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules);

  size_t static_rule_limit = global_rules_enabled
                                 ? global_rules_tracker_.GetAvailableAllocation(
                                       load_data.extension_id) +
                                       GetStaticGuaranteedMinimumRuleCount()
                                 : GetStaticRuleLimit();

  for (RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.did_load_successfully()) {
      notify_ruleset_failed_to_load = true;
      continue;
    }

    std::unique_ptr<RulesetMatcher> matcher = ruleset.TakeMatcher();

    // Per-ruleset limits should have been enforced during
    // indexing/installation.
    DCHECK_LE(matcher->GetRegexRulesCount(),
              static_cast<size_t>(GetRegexRuleLimit()));
    DCHECK_LE(matcher->GetRulesCount(), ruleset.source().rule_count_limit());

    if (ruleset.source().is_dynamic_ruleset()) {
      matchers.push_back(std::move(matcher));
      continue;
    }

    size_t new_rules_count = static_rules_count + matcher->GetRulesCount();
    if (new_rules_count > static_rule_limit) {
      global_rule_limit_exceeded = global_rules_enabled;
      continue;
    }

    size_t new_regex_rules_count =
        static_regex_rules_count + matcher->GetRegexRulesCount();
    if (new_regex_rules_count > static_cast<size_t>(GetRegexRuleLimit())) {
      continue;
    }

    static_rules_count = new_rules_count;
    static_regex_rules_count = new_regex_rules_count;
    matchers.push_back(std::move(matcher));
  }

  if (notify_ruleset_failed_to_load) {
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
  }

  if (global_rule_limit_exceeded) {
    warning_service_->AddWarnings(
        {Warning::CreateEnabledRuleCountExceededWarning(
            load_data.extension_id)});
  }

  if (global_rules_enabled) {
    bool allocation_updated = global_rules_tracker_.OnExtensionRuleCountUpdated(
        load_data.extension_id, static_rules_count);
    DCHECK(allocation_updated);
  }

  if (matchers.empty())
    return;

  AddCompositeMatcher(load_data.extension_id,
                      std::make_unique<CompositeMatcher>(std::move(matchers)));
}

void RulesMonitorService::OnNewStaticRulesetsLoaded(
    UpdateEnabledRulesetsUICallback callback,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    LoadRequestData load_data) {
  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // It's possible that the extension has been disabled since the initial
  // request. If it's disabled, return early.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    // Still dispatch the |callback|, even though it's probably a no-op.
    std::move(callback).Run(base::nullopt /* error */);
    return;
  }

  size_t static_rules_count = 0;
  size_t static_regex_rules_count = 0;
  bool global_rules_enabled =
      base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules);
  CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(load_data.extension_id);
  if (matcher) {
    // Iterate over the existing matchers to compute |static_rules_count| and
    // |static_regex_rules_count|.
    for (const std::unique_ptr<RulesetMatcher>& matcher : matcher->matchers()) {
      // Exclude since we are only including static rulesets.
      if (matcher->id() == kDynamicRulesetID)
        continue;

      // Exclude since we'll be removing this |matcher|.
      if (base::Contains(ids_to_disable, matcher->id()))
        continue;

      // Exclude to prevent double counting. This will be a part of
      // |new_matchers| below.
      if (base::Contains(ids_to_enable, matcher->id()))
        continue;

      static_rules_count += matcher->GetRulesCount();
      static_regex_rules_count += matcher->GetRegexRulesCount();
    }
  }

  CompositeMatcher::MatcherList new_matchers;
  new_matchers.reserve(load_data.rulesets.size());
  for (RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.did_load_successfully()) {
      std::move(callback).Run(kInternalErrorUpdatingEnabledRulesets);
      return;
    }

    std::unique_ptr<RulesetMatcher> matcher = ruleset.TakeMatcher();

    // Per-ruleset limits should have been enforced during
    // indexing/installation.
    DCHECK_LE(matcher->GetRegexRulesCount(),
              static_cast<size_t>(GetRegexRuleLimit()));
    DCHECK_LE(matcher->GetRulesCount(), ruleset.source().rule_count_limit());

    static_rules_count += matcher->GetRulesCount();
    static_regex_rules_count += matcher->GetRegexRulesCount();
    new_matchers.push_back(std::move(matcher));
  }

  if (!global_rules_enabled &&
      static_rules_count > static_cast<size_t>(GetStaticRuleLimit())) {
    std::move(callback).Run(kEnabledRulesetsRuleCountExceeded);
    return;
  }

  if (static_regex_rules_count > static_cast<size_t>(GetRegexRuleLimit())) {
    std::move(callback).Run(kEnabledRulesetsRegexRuleCountExceeded);
    return;
  }

  // If global rules are enabled, attempt to update the extension's extra rule
  // count. If this update cannot be completed without exceeding the global
  // limit, then the update is not applied and an error is returned.
  if (global_rules_enabled &&
      !global_rules_tracker_.OnExtensionRuleCountUpdated(load_data.extension_id,
                                                         static_rules_count)) {
    std::move(callback).Run(kEnabledRulesetsRuleCountExceeded);
    return;
  }

  if (!matcher) {
    // The extension didn't have any existing rulesets. Hence just add a new
    // CompositeMatcher with |new_matchers|.
    AddCompositeMatcher(
        load_data.extension_id,
        std::make_unique<CompositeMatcher>(std::move(new_matchers)));
    std::move(callback).Run(base::nullopt);
    return;
  }

  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();

  matcher->RemoveRulesetsWithIDs(ids_to_disable);
  matcher->AddOrUpdateRulesets(std::move(new_matchers));

  prefs_->SetDNREnabledStaticRulesets(load_data.extension_id,
                                      matcher->ComputeStaticRulesetIDs());

  AdjustExtraHeaderListenerCountIfNeeded(had_extra_headers_matcher);

  std::move(callback).Run(base::nullopt);
}

void RulesMonitorService::ExecuteOrQueueAPICall(const Extension& extension,
                                                base::OnceClosure task) {
  auto it = tasks_pending_on_load_.find(extension.id());
  if (it != tasks_pending_on_load_.end()) {
    // The ruleset is still loading in response to OnExtensionLoaded(). Wait
    // till the ruleset loading is complete to prevent a race.
    DCHECK(!it->second->is_signaled());
    it->second->Post(FROM_HERE, std::move(task));
    return;
  }

  // The extension's initial rulesets are fully loaded; dispatch |task|
  // immediately.
  std::move(task).Run();
}

void RulesMonitorService::OnDynamicRulesUpdated(
    DynamicRuleUpdateUICallback callback,
    LoadRequestData load_data,
    base::Optional<std::string> error) {
  DCHECK_EQ(1u, load_data.rulesets.size());

  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // Respond to the extension.
  std::move(callback).Run(std::move(error));

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK_EQ(dynamic_ruleset.did_load_successfully(), !error.has_value());

  if (!dynamic_ruleset.did_load_successfully())
    return;

  DCHECK(dynamic_ruleset.new_checksum());

  // It's possible that the extension has been disabled since the initial update
  // rule request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id)) {
    return;
  }

  // Update the dynamic ruleset.
  UpdateRulesetMatcher(load_data.extension_id, dynamic_ruleset.TakeMatcher());
}

void RulesMonitorService::RemoveCompositeMatcher(
    const ExtensionId& extension_id) {
  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  ruleset_manager_.RemoveRuleset(extension_id);
  action_tracker_.ClearExtensionData(extension_id);
  AdjustExtraHeaderListenerCountIfNeeded(had_extra_headers_matcher);
}

void RulesMonitorService::AddCompositeMatcher(
    const ExtensionId& extension_id,
    std::unique_ptr<CompositeMatcher> matcher) {
  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  ruleset_manager_.AddRuleset(extension_id, std::move(matcher));
  AdjustExtraHeaderListenerCountIfNeeded(had_extra_headers_matcher);
}

void RulesMonitorService::UpdateRulesetMatcher(
    const ExtensionId& extension_id,
    std::unique_ptr<RulesetMatcher> ruleset_matcher) {
  CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(extension_id);

  // The extension didn't have a corresponding CompositeMatcher.
  if (!matcher) {
    CompositeMatcher::MatcherList matchers;
    matchers.push_back(std::move(ruleset_matcher));
    AddCompositeMatcher(
        extension_id, std::make_unique<CompositeMatcher>(std::move(matchers)));
    return;
  }

  bool had_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  matcher->AddOrUpdateRuleset(std::move(ruleset_matcher));
  AdjustExtraHeaderListenerCountIfNeeded(had_extra_headers_matcher);
}

void RulesMonitorService::AdjustExtraHeaderListenerCountIfNeeded(
    bool had_extra_headers_matcher) {
  bool has_extra_headers_matcher = ruleset_manager_.HasAnyExtraHeadersMatcher();
  if (had_extra_headers_matcher == has_extra_headers_matcher)
    return;
  if (has_extra_headers_matcher) {
    ExtensionWebRequestEventRouter::GetInstance()
        ->IncrementExtraHeadersListenerCount(context_);
  } else {
    ExtensionWebRequestEventRouter::GetInstance()
        ->DecrementExtraHeadersListenerCount(context_);
  }
}

void RulesMonitorService::LogMetricsAndUpdateChecksumsIfNeeded(
    const LoadRequestData& load_data) {
  for (const RulesetInfo& ruleset : load_data.rulesets) {
    // The |load_ruleset_result()| might be empty if CreateVerifiedMatcher
    // wasn't called on the ruleset.
    if (ruleset.load_ruleset_result())
      LogLoadRulesetResult(*ruleset.load_ruleset_result());
  }

  // The extension may have been uninstalled by this point. Return early if
  // that's the case.
  if (!extension_registry_->GetInstalledExtension(load_data.extension_id))
    return;

  // Update checksums for all rulesets.
  // Note: We also do this for a non-enabled extension. The ruleset on the disk
  // has already been modified at this point. So we do want to update the
  // checksum for it to be in sync with what's on disk.
  for (const RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.new_checksum())
      continue;

    if (ruleset.source().is_dynamic_ruleset()) {
      prefs_->SetDNRDynamicRulesetChecksum(load_data.extension_id,
                                           *(ruleset.new_checksum()));
    } else {
      prefs_->SetDNRStaticRulesetChecksum(load_data.extension_id,
                                          ruleset.source().id(),
                                          *(ruleset.new_checksum()));
    }
  }
}

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::
    DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(WarningServiceFactory::GetInstance());
  DependsOn(PermissionHelper::GetFactoryInstance());
}

}  // namespace extensions
