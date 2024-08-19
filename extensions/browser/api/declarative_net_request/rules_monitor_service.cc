// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_event_router_factory.h"
#include "extensions/browser/disable_reason.h"
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
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "tools/json_schema_compiler/util.h"

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

// Returns whether the extension's allocation should be released. This would
// return true for cases where we expect the extension to be unloaded for a
// while or if the extension directory's contents changed in a reload.
bool ShouldReleaseAllocationOnUnload(const ExtensionPrefs* prefs,
                                     const Extension& extension,
                                     UnloadedExtensionReason reason) {
  if (reason == UnloadedExtensionReason::DISABLE) {
    static constexpr int kReleaseAllocationDisableReasons =
        disable_reason::DISABLE_BLOCKED_BY_POLICY |
        disable_reason::DISABLE_USER_ACTION;

    // Release allocation on reload of an unpacked extension and treat it as a
    // new install since the extension directory's contents may have changed.
    bool is_unpacked_reload =
        Manifest::IsUnpackedLocation(extension.location()) &&
        prefs->HasDisableReason(extension.id(), disable_reason::DISABLE_RELOAD);

    return is_unpacked_reload || (prefs->GetDisableReasons(extension.id()) &
                                  kReleaseAllocationDisableReasons) != 0;
  }

  return reason == UnloadedExtensionReason::BLOCKLIST;
}

// Helper to create a RulesetMatcher for the session-scoped ruleset
// corresponding to the given |rules|. On failure, null is returned and |error|
// is populated.
std::unique_ptr<RulesetMatcher> CreateSessionScopedMatcher(
    const ExtensionId& extension_id,
    std::vector<api::declarative_net_request::Rule> rules,
    std::string* error) {
  DCHECK(error);
  RulesetSource source(kSessionRulesetID, GetSessionRuleLimit(), extension_id,
                       /*enabled=*/true);

  auto parse_flags = RulesetSource::kRaiseErrorOnInvalidRules |
                     RulesetSource::kRaiseErrorOnLargeRegexRules;
  ParseInfo info = source.IndexRules(std::move(rules), parse_flags);
  if (info.has_error()) {
    *error = info.error();
    return nullptr;
  }

  base::span<const uint8_t> buffer = info.GetBuffer();
  std::unique_ptr<RulesetMatcher> matcher;
  LoadRulesetResult result = source.CreateVerifiedMatcher(
      std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()),
      &matcher);

  // Creating a verified matcher for session scoped rules should never result in
  // an error, since these are not persisted to disk and are not affected by
  // related corruption and verification issues.
  DCHECK_EQ(LoadRulesetResult::kSuccess, result)
      << "Loading session scoped ruleset failed unexpectedly "
      << static_cast<int>(result);

  return matcher;
}

HostPermissionsAlwaysRequired GetHostPermissionsAlwaysRequired(
    const Extension& extension) {
  DCHECK(HasAnyDNRPermission(extension));
  const PermissionsData* permissions = extension.permissions_data();

  if (permissions->HasAPIPermission(
          mojom::APIPermissionID::kDeclarativeNetRequest)) {
    return HostPermissionsAlwaysRequired::kFalse;
  }

  // Else the extension only has the kDeclarativeNetRequestWithHostAccess
  // permission.
  return HostPermissionsAlwaysRequired::kTrue;
}

}  // namespace

// Helper to bridge tasks to FileSequenceHelper. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_helper_(std::make_unique<FileSequenceHelper>()) {}

  FileSequenceBridge(const FileSequenceBridge&) = delete;
  FileSequenceBridge& operator=(const FileSequenceBridge&) = delete;

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
      const RuleCounts& rule_limit,
      FileSequenceHelper::UpdateDynamicRulesUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |update_dynamic_rules_task| is run.
    base::OnceClosure update_dynamic_rules_task = base::BindOnce(
        &FileSequenceHelper::UpdateDynamicRules,
        base::Unretained(file_sequence_helper_.get()), std::move(load_data),
        std::move(rule_ids_to_remove), std::move(rules_to_add), rule_limit,
        std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE,
                                std::move(update_dynamic_rules_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceHelper> file_sequence_helper_;
};

// Helps to ensure FIFO ordering of api calls and that only a single api call
// proceeds at a time.
class RulesMonitorService::ApiCallQueue {
 public:
  ApiCallQueue() = default;
  ~ApiCallQueue() = default;
  ApiCallQueue(const ApiCallQueue&) = delete;
  ApiCallQueue& operator=(const ApiCallQueue&) = delete;
  ApiCallQueue(ApiCallQueue&&) = delete;
  ApiCallQueue& operator=(ApiCallQueue&&) = delete;

  // Signals to start executing API calls. Unless signaled so, the ApiCallQueue
  // will queue api calls for future execution.
  // Note that this can start running a queued api call synchronously.
  void SetReadyToExecuteApiCalls() {
    DCHECK(!ready_to_execute_api_calls_);
    DCHECK(!executing_api_call_);
    ready_to_execute_api_calls_ = true;
    ExecuteApiCallIfNecessary();
  }

  // Executes the api call or queues it for execution if the ApiCallQueue is not
  // ready or there is an existing api call in progress.
  // `unbound_api_call` will be invoked when the queue is ready, and is
  // responsible for invoking `api_callback` upon its completion. Following
  // this, `ApiCallQueue::OnApiCallCompleted()` will be called in the next event
  // cycle, triggering the next call (if any).
  template <typename ApiCallbackType>
  void ExecuteOrQueueApiCall(
      base::OnceCallback<void(ApiCallbackType)> unbound_api_call,
      ApiCallbackType api_callback) {
    // Wrap the `api_callback` in a synthetic callback to ensure
    // `OnApiCallCompleted()` is run after each api call. Note we schedule
    // `OnApiCallCompleted()` to run in the next event cycle to ensure any
    // side-effects from the last run api call are "committed" by the time the
    // next api call executes.
    auto post_async = [](base::OnceClosure async_task) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(async_task));
    };
    base::OnceClosure async_task = base::BindOnce(
        &ApiCallQueue::OnApiCallCompleted, weak_factory_.GetWeakPtr());
    ApiCallbackType wrapped_callback =
        std::move(api_callback)
            .Then(base::BindOnce(post_async, std::move(async_task)));

    base::OnceClosure api_call = base::BindOnce(std::move(unbound_api_call),
                                                std::move(wrapped_callback));
    api_call_queue_.push(std::move(api_call));
    if (!ready_to_execute_api_calls_ || executing_api_call_) {
      return;
    }

    DCHECK_EQ(1u, api_call_queue_.size());
    ExecuteApiCallIfNecessary();
  }

 private:
  // Signals that the last posted api call has completed.
  void OnApiCallCompleted() {
    DCHECK(executing_api_call_);
    executing_api_call_ = false;
    ExecuteApiCallIfNecessary();
  }

  // Executes the api call at the front of the queue if there is one.
  void ExecuteApiCallIfNecessary() {
    DCHECK(!executing_api_call_);
    DCHECK(ready_to_execute_api_calls_);
    if (api_call_queue_.empty()) {
      return;
    }

    executing_api_call_ = true;
    base::OnceClosure api_call = std::move(api_call_queue_.front());
    api_call_queue_.pop();
    std::move(api_call).Run();
  }

  bool executing_api_call_ = false;
  bool ready_to_execute_api_calls_ = false;
  base::queue<base::OnceClosure> api_call_queue_;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details.
  base::WeakPtrFactory<ApiCallQueue> weak_factory_{this};
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
    ApiCallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  update_dynamic_or_session_rules_queue_map_[extension.id()]
      .ExecuteOrQueueApiCall(
          base::BindOnce(&RulesMonitorService::UpdateDynamicRulesInternal,
                         weak_factory_.GetWeakPtr(), extension.id(),
                         std::move(rule_ids_to_remove),
                         std::move(rules_to_add)),
          std::move(callback));
}

void RulesMonitorService::UpdateEnabledStaticRulesets(
    const Extension& extension,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    ApiCallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  update_enabled_rulesets_queue_map_[extension.id()].ExecuteOrQueueApiCall(
      base::BindOnce(&RulesMonitorService::UpdateEnabledStaticRulesetsInternal,
                     weak_factory_.GetWeakPtr(), extension.id(),
                     std::move(ids_to_disable), std::move(ids_to_enable)),
      std::move(callback));
}

void RulesMonitorService::UpdateStaticRules(const Extension& extension,
                                            RulesetID ruleset_id,
                                            RuleIdsToUpdate rule_ids_to_update,
                                            ApiCallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  update_enabled_rulesets_queue_map_[extension.id()].ExecuteOrQueueApiCall(
      base::BindOnce(&RulesMonitorService::UpdateStaticRulesInternal,
                     weak_factory_.GetWeakPtr(), extension.id(),
                     std::move(ruleset_id), std::move(rule_ids_to_update)),
      std::move(callback));
}

void RulesMonitorService::GetDisabledRuleIds(
    const Extension& extension,
    RulesetID ruleset_id,
    ApiCallbackToGetDisabledRuleIds callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  update_enabled_rulesets_queue_map_[extension.id()].ExecuteOrQueueApiCall(
      base::BindOnce(&RulesMonitorService::GetDisabledRuleIdsInternal,
                     weak_factory_.GetWeakPtr(), extension.id(),
                     std::move(ruleset_id)),
      std::move(callback));
}

const base::Value::List& RulesMonitorService::GetSessionRulesValue(
    const ExtensionId& extension_id) const {
  static const base::NoDestructor<base::Value::List> empty_rules;
  auto it = session_rules_.find(extension_id);
  return it == session_rules_.end() ? *empty_rules : it->second;
}

std::vector<api::declarative_net_request::Rule>
RulesMonitorService::GetSessionRules(const ExtensionId& extension_id) const {
  std::vector<api::declarative_net_request::Rule> result;
  std::u16string error;
  bool populate_result = json_schema_compiler::util::PopulateArrayFromList(
      GetSessionRulesValue(extension_id), result, error);
  DCHECK(populate_result);
  DCHECK(error.empty());
  return result;
}

void RulesMonitorService::UpdateSessionRules(
    const Extension& extension,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    ApiCallback callback) {
  // Sanity check that this is only called for an enabled extension.
  DCHECK(extension_registry_->enabled_extensions().Contains(extension.id()));

  update_dynamic_or_session_rules_queue_map_[extension.id()]
      .ExecuteOrQueueApiCall(
          base::BindOnce(&RulesMonitorService::UpdateSessionRulesInternal,
                         weak_factory_.GetWeakPtr(), extension.id(),
                         std::move(rule_ids_to_remove),
                         std::move(rules_to_add)),
          std::move(callback));
}

RuleCounts RulesMonitorService::GetRuleCounts(const ExtensionId& extension_id,
                                              RulesetID id) const {
  const CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(extension_id);
  if (!matcher) {
    return RuleCounts();
  }

  const RulesetMatcher* ruleset_matcher = matcher->GetMatcherWithID(id);
  if (!ruleset_matcher) {
    return RuleCounts();
  }

  return ruleset_matcher->GetRuleCounts();
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
  registry_observation_.Observe(extension_registry_.get());
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
  if (!HasAnyDNRPermission(*extension)) {
    return;
  }

  if (!is_update || Manifest::IsUnpackedLocation(extension->location())) {
    return;
  }

  // Allow the extension to retain its pre-update allocation during the next
  // extension load. This can allow the extension to enable some
  // non-manifest-enabled rulesets and to retain much of its pre-update
  // behavior. The preference is set in OnExtensionWillBeInstalled instead of
  // OnExtensionInstalled because OnExtensionInstalled is called after
  // OnExtensionLoaded.
  PrefsHelper helper(*prefs_);
  helper.SetKeepExcessAllocation(extension->id(), true);
}

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK_EQ(context_, browser_context);

  if (!HasAnyDNRPermission(*extension)) {
    return;
  }

  LoadRequestData load_data(extension->id(), extension->version());
  int expected_ruleset_checksum;

  PrefsHelper helper(*prefs_);
  // Static rulesets.
  {
    std::vector<FileBackedRulesetSource> sources =
        FileBackedRulesetSource::CreateStatic(
            *extension, FileBackedRulesetSource::RulesetFilter::kIncludeAll);

    std::optional<std::set<RulesetID>> prefs_enabled_rulesets =
        helper.GetEnabledStaticRulesets(extension->id());

    bool ruleset_failed_to_load = false;
    for (auto& source : sources) {
      bool enabled = prefs_enabled_rulesets
                         ? base::Contains(*prefs_enabled_rulesets, source.id())
                         : source.enabled_by_default();

      bool ignored = helper.ShouldIgnoreRuleset(extension->id(), source.id());

      if (!enabled || ignored) {
        continue;
      }

      if (!helper.GetStaticRulesetChecksum(extension->id(), source.id(),
                                           expected_ruleset_checksum)) {
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
  if (helper.GetDynamicRulesetChecksum(extension->id(),
                                       expected_ruleset_checksum)) {
    RulesetInfo dynamic_ruleset(FileBackedRulesetSource::CreateDynamic(
        browser_context, extension->id()));
    dynamic_ruleset.set_expected_checksum(expected_ruleset_checksum);
    load_data.rulesets.push_back(std::move(dynamic_ruleset));
  }

  if (load_data.rulesets.empty()) {
    OnInitialRulesetsLoadedFromDisk(std::move(load_data));
    return;
  }

  auto load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnInitialRulesetsLoadedFromDisk,
                     weak_factory_.GetWeakPtr());
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(context_, browser_context);

  if (!HasAnyDNRPermission(*extension)) {
    return;
  }

  // If the extension is unloaded for any reason other than an update, the
  // unused rule allocation should not be kept for this extension the next
  // time its rulesets are loaded, as it is no longer "the first load after an
  // update".
  if (reason != UnloadedExtensionReason::UPDATE) {
    PrefsHelper helper(*prefs_);
    helper.SetKeepExcessAllocation(extension->id(), false);
  }

  if (ShouldReleaseAllocationOnUnload(prefs_, *extension, reason)) {
    global_rules_tracker_.ClearExtensionAllocation(extension->id());
  }

  // Erase the api call queues for the extension. Any un-executed api calls
  // should just be ignored now given the extension is being unloaded.
  update_enabled_rulesets_queue_map_.erase(extension->id());
  update_dynamic_or_session_rules_queue_map_.erase(extension->id());

  // Return early if the extension does not have an active indexed ruleset.
  if (!ruleset_manager_.GetMatcherForExtension(extension->id())) {
    return;
  }

  RemoveCompositeMatcher(extension->id());
}

void RulesMonitorService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(context_, browser_context);

  if (!HasAnyDNRPermission(*extension)) {
    return;
  }

  session_rules_.erase(extension->id());

  // Skip if the extension will be reinstalled soon.
  if (reason == UNINSTALL_REASON_REINSTALL) {
    return;
  }

  global_rules_tracker_.ClearExtensionAllocation(extension->id());

  // Skip if the extension doesn't have a dynamic ruleset.
  PrefsHelper helper(*prefs_);
  int dynamic_checksum;
  if (!helper.GetDynamicRulesetChecksum(extension->id(), dynamic_checksum)) {
    return;
  }

  // Cleanup the dynamic rules directory for the extension.
  // TODO(karandeepb): It's possible that this task fails, e.g. during shutdown.
  // Make this more robust.
  FileBackedRulesetSource source =
      FileBackedRulesetSource::CreateDynamic(browser_context, extension->id());
  DCHECK_EQ(source.json_path().DirName(), source.indexed_path().DirName());
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(source.json_path().DirName()));
}

void RulesMonitorService::UpdateDynamicRulesInternal(
    const ExtensionId& extension_id,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    ApiCallback callback) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related bookkeeping is
    // done.
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  LoadRequestData data(extension_id, extension->version());

  // Calculate available shared rule limits. These limits won't be affected by
  // another simultaneous api call since we ensure that for a given extension,
  // only up to 1 updateDynamicRules/updateSessionRules call is in progress. See
  // the usage of `ApiCallQueue`.
  RuleCounts session_rules_count =
      GetRuleCounts(extension_id, kSessionRulesetID);
  RuleCounts available_limit(
      GetDynamicRuleLimit(), GetUnsafeDynamicRuleLimit(),
      GetRegexRuleLimit() - session_rules_count.regex_rule_count);

  // We are updating the indexed ruleset. Don't set the expected checksum since
  // it'll change.
  data.rulesets.emplace_back(
      FileBackedRulesetSource::CreateDynamic(context_, extension_id));

  auto update_rules_callback =
      base::BindOnce(&RulesMonitorService::OnDynamicRulesUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  file_sequence_bridge_->UpdateDynamicRules(
      std::move(data), std::move(rule_ids_to_remove), std::move(rules_to_add),
      available_limit, std::move(update_rules_callback));
}

void RulesMonitorService::UpdateSessionRulesInternal(
    const ExtensionId& extension_id,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    ApiCallback callback) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related bookkeeping is
    // done.
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  std::vector<api::declarative_net_request::Rule> new_rules =
      GetSessionRules(extension_id);

  std::set<int> ids_to_remove(rule_ids_to_remove.begin(),
                              rule_ids_to_remove.end());
  std::erase_if(new_rules, [&ids_to_remove](const dnr_api::Rule& rule) {
    return base::Contains(ids_to_remove, rule.id);
  });

  new_rules.insert(new_rules.end(),
                   std::make_move_iterator(rules_to_add.begin()),
                   std::make_move_iterator(rules_to_add.end()));

  // Check if the update would exceed shared rule limits.
  {
    RuleCounts dynamic_rule_count =
        GetRuleCounts(extension_id, kDynamicRulesetID);
    RuleCounts available_limit(
        GetSessionRuleLimit(), GetUnsafeSessionRuleLimit(),
        GetRegexRuleLimit() - dynamic_rule_count.regex_rule_count);

    if (new_rules.size() > available_limit.rule_count) {
      std::move(callback).Run(kSessionRuleCountExceeded);
      return;
    }

    if (base::FeatureList::IsEnabled(
            extensions_features::kDeclarativeNetRequestSafeRuleLimits)) {
      size_t unsafe_rule_count = base::ranges::count_if(
          new_rules,
          [](const dnr_api::Rule& rule) { return !IsRuleSafe(rule); });
      if (unsafe_rule_count > available_limit.unsafe_rule_count) {
        std::move(callback).Run(kSessionUnsafeRuleCountExceeded);
        return;
      }
    }

    size_t regex_rule_count =
        base::ranges::count_if(new_rules, [](const dnr_api::Rule& rule) {
          return !!rule.condition.regex_filter;
        });
    if (regex_rule_count > available_limit.regex_rule_count) {
      std::move(callback).Run(kSessionRegexRuleCountExceeded);
      return;
    }
  }

  base::Value::List new_rules_value =
      json_schema_compiler::util::CreateValueFromArray(new_rules);

  std::string error;
  std::unique_ptr<RulesetMatcher> matcher =
      CreateSessionScopedMatcher(extension_id, std::move(new_rules), &error);
  if (!matcher) {
    std::move(callback).Run(std::move(error));
    return;
  }

  session_rules_[extension_id] = std::move(new_rules_value);
  UpdateRulesetMatcher(*extension, std::move(matcher));
  std::move(callback).Run(std::nullopt /* error */);
}

void RulesMonitorService::UpdateEnabledStaticRulesetsInternal(
    const ExtensionId& extension_id,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    ApiCallback callback) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related bookkeeping is
    // done.
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  LoadRequestData load_data(extension_id, extension->version());
  int expected_ruleset_checksum = -1;
  PrefsHelper helper(*prefs_);
  for (const RulesetID& id_to_enable : ids_to_enable) {
    const DNRManifestData::RulesetInfo& info =
        DNRManifestData::GetRuleset(*extension, id_to_enable);
    RulesetInfo static_ruleset(
        FileBackedRulesetSource::CreateStatic(*extension, info));

    // Take note of the expected checksum if this ruleset has been indexed in
    // the past.
    if (helper.GetStaticRulesetChecksum(extension_id, id_to_enable,
                                        expected_ruleset_checksum)) {
      static_ruleset.set_expected_checksum(expected_ruleset_checksum);
    }

    load_data.rulesets.push_back(std::move(static_ruleset));
  }

  auto load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnNewStaticRulesetsLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ids_to_disable), std::move(ids_to_enable));
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
                                      std::move(load_ruleset_callback));
}

void RulesMonitorService::UpdateStaticRulesInternal(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    RuleIdsToUpdate rule_ids_to_update,
    ApiCallback callback) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related bookkeeping is
    // done.
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  auto result = PrefsHelper(*prefs_).UpdateDisabledStaticRules(
      extension_id, ruleset_id, rule_ids_to_update);

  if (result.error) {
    std::move(callback).Run(result.error);
    return;
  }

  if (!result.changed) {
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  if (CompositeMatcher* matcher =
          ruleset_manager_.GetMatcherForExtension(extension->id())) {
    for (const auto& ruleset_matcher : matcher->matchers()) {
      if (ruleset_matcher->id() != ruleset_id) {
        continue;
      }

      ruleset_matcher->SetDisabledRuleIds(
          std::move(result.disabled_rule_ids_after_update));
      break;
    }
  }

  std::move(callback).Run(std::nullopt /* error */);
}

void RulesMonitorService::GetDisabledRuleIdsInternal(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    ApiCallbackToGetDisabledRuleIds callback) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // There is no enabled extension to respond to. While this is probably a
    // no-op, still dispatch the callback to ensure any related bookkeeping is
    // done.
    std::move(callback).Run({});
    return;
  }

  base::flat_set<int> disabled_rule_ids =
      PrefsHelper(*prefs_).GetDisabledStaticRuleIds(extension->id(),
                                                    ruleset_id);

  std::move(callback).Run(
      std::vector<int>(disabled_rule_ids.begin(), disabled_rule_ids.end()));
}

void RulesMonitorService::OnInitialRulesetsLoadedFromDisk(
    LoadRequestData load_data) {
  if (test_observer_) {
    test_observer_->OnRulesetLoadComplete(load_data.extension_id);
  }

  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // It's possible that the extension has been disabled since the initial load
  // ruleset request, or the extension was updated to a new version while the
  // ruleset for the old version was still loading (and is thus stale). In
  // either case, do nothing.
  // TODO(crbug.com/1493992, crbug.com/1386010): Add a test which will cause
  // this block to be hit when the extension updates.
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(load_data.extension_id);
  if (!extension || (load_data.extension_version != extension->version())) {
    return;
  }

  // Load session-scoped ruleset.
  std::vector<api::declarative_net_request::Rule> session_rules =
      GetSessionRules(load_data.extension_id);

  // Allocate one additional space for the session-scoped ruleset if needed.
  CompositeMatcher::MatcherList matchers;
  matchers.reserve(load_data.rulesets.size() + (session_rules.empty() ? 0 : 1));

  if (!session_rules.empty()) {
    std::string error;
    std::unique_ptr<RulesetMatcher> session_matcher =
        CreateSessionScopedMatcher(load_data.extension_id,
                                   std::move(session_rules), &error);
    DCHECK(session_matcher)
        << "Loading session scoped ruleset failed unexpectedly: " << error;
    matchers.push_back(std::move(session_matcher));
  }

  // Sort by ruleset IDs. This will ensure that the static rulesets are in the
  // order in which they are defined in the manifest.
  std::sort(load_data.rulesets.begin(), load_data.rulesets.end(),
            &RulesetInfoCompareByID);

  // Build the CompositeMatcher for the extension. Also enforce rules limit
  // across the enabled static rulesets. Note: we don't enforce the rules limit
  // at install time (by raising a hard error) to maintain forwards
  // compatibility. Since we iterate based on the order of ruleset ID, we'll
  // give more preference to rulesets occurring first in the manifest.
  RuleCounts static_rule_count;
  bool notify_ruleset_failed_to_load = false;
  bool global_rule_limit_exceeded = false;

  RuleCounts static_rule_limit(
      global_rules_tracker_.GetAvailableAllocation(load_data.extension_id) +
          GetStaticGuaranteedMinimumRuleCount(),
      /*unsafe_rule_count=*/std::nullopt, GetRegexRuleLimit());

  for (RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.did_load_successfully()) {
      notify_ruleset_failed_to_load = true;
      continue;
    }

    std::unique_ptr<RulesetMatcher> matcher = ruleset.TakeMatcher();

    RuleCounts matcher_count = matcher->GetRuleCounts();

    // Per-ruleset limits should have been enforced during
    // indexing/installation.
    DCHECK_LE(matcher_count.regex_rule_count,
              static_cast<size_t>(GetRegexRuleLimit()));
    DCHECK_LE(matcher_count.rule_count, ruleset.source().rule_count_limit());

    if (ruleset.source().is_dynamic_ruleset()) {
      matchers.push_back(std::move(matcher));
      continue;
    }

    RuleCounts new_ruleset_count = static_rule_count + matcher_count;
    if (new_ruleset_count.rule_count > static_rule_limit.rule_count) {
      global_rule_limit_exceeded = true;
      continue;
    }

    if (new_ruleset_count.regex_rule_count >
        static_rule_limit.regex_rule_count) {
      continue;
    }

    static_rule_count = new_ruleset_count;

    matcher->SetDisabledRuleIds(PrefsHelper(*prefs_).GetDisabledStaticRuleIds(
        extension->id(), matcher->id()));

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

  bool allocation_updated = global_rules_tracker_.OnExtensionRuleCountUpdated(
      load_data.extension_id, static_rule_count.rule_count);
  DCHECK(allocation_updated);

  AddCompositeMatcher(*extension, std::move(matchers));

  // Start processing api calls now that the initial ruleset load has completed.
  update_enabled_rulesets_queue_map_[load_data.extension_id]
      .SetReadyToExecuteApiCalls();
  update_dynamic_or_session_rules_queue_map_[load_data.extension_id]
      .SetReadyToExecuteApiCalls();
}

void RulesMonitorService::OnNewStaticRulesetsLoaded(
    ApiCallback callback,
    std::set<RulesetID> ids_to_disable,
    std::set<RulesetID> ids_to_enable,
    LoadRequestData load_data) {
  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // It's possible that the extension has been disabled since the initial
  // request. If it's disabled, return early.
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(load_data.extension_id);
  if (!extension) {
    // Still dispatch the |callback|, even though it's probably a no-op.
    std::move(callback).Run(std::nullopt /* error */);
    return;
  }

  int static_ruleset_count = 0;
  RuleCounts static_rule_count;
  CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(load_data.extension_id);
  if (matcher) {
    // Iterate over the existing matchers to compute `static_rule_count` and
    // `static_ruleset_count`.
    for (const std::unique_ptr<RulesetMatcher>& ruleset_matcher :
         matcher->matchers()) {
      // Exclude since we are only including static rulesets.
      if (ruleset_matcher->id() == kDynamicRulesetID) {
        continue;
      }

      // Exclude since we'll be removing this |matcher|.
      if (base::Contains(ids_to_disable, ruleset_matcher->id())) {
        continue;
      }

      // Exclude to prevent double counting. This will be a part of
      // |new_matchers| below.
      if (base::Contains(ids_to_enable, ruleset_matcher->id())) {
        continue;
      }

      static_ruleset_count += 1;
      static_rule_count += ruleset_matcher->GetRuleCounts();
    }
  }

  PrefsHelper helper(*prefs_);
  CompositeMatcher::MatcherList new_matchers;
  new_matchers.reserve(load_data.rulesets.size());
  for (RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.did_load_successfully()) {
      std::move(callback).Run(kInternalErrorUpdatingEnabledRulesets);
      return;
    }

    std::unique_ptr<RulesetMatcher> ruleset_matcher = ruleset.TakeMatcher();

    RuleCounts matcher_count = ruleset_matcher->GetRuleCounts();

    // Per-ruleset limits should have been enforced during
    // indexing/installation.
    DCHECK_LE(matcher_count.regex_rule_count,
              static_cast<size_t>(GetRegexRuleLimit()));
    DCHECK_LE(matcher_count.rule_count, ruleset.source().rule_count_limit());

    static_ruleset_count += 1;
    static_rule_count += matcher_count;

    ruleset_matcher->SetDisabledRuleIds(helper.GetDisabledStaticRuleIds(
        extension->id(), ruleset_matcher->id()));

    new_matchers.push_back(std::move(ruleset_matcher));
  }

  if (static_ruleset_count > dnr_api::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS) {
    std::move(callback).Run(
        declarative_net_request::kEnabledRulesetCountExceeded);
    return;
  }

  if (static_rule_count.regex_rule_count >
      static_cast<size_t>(GetRegexRuleLimit())) {
    std::move(callback).Run(kEnabledRulesetsRegexRuleCountExceeded);
    return;
  }

  // Attempt to update the extension's extra rule count. If this update cannot
  // be completed without exceeding the global limit, then the update is not
  // applied and an error is returned.
  if (!global_rules_tracker_.OnExtensionRuleCountUpdated(
          load_data.extension_id, static_rule_count.rule_count)) {
    std::move(callback).Run(kEnabledRulesetsRuleCountExceeded);
    return;
  }

  if (matcher) {
    matcher->RemoveRulesetsWithIDs(ids_to_disable);
    matcher->AddOrUpdateRulesets(std::move(new_matchers));
  } else {
    // The extension didn't have any existing rulesets. Hence just add a new
    // CompositeMatcher with |new_matchers|. Note, this also updates the
    // extra header listener count.
    AddCompositeMatcher(*extension, std::move(new_matchers));
    matcher = ruleset_manager_.GetMatcherForExtension(load_data.extension_id);
  }

  // matcher still can be null if the extension didn't have any existing
  // rulesets and the OnNewStaticRulesetsLoaded() is called without any rulesets
  // in load_data.rulesets (means, ids_to_enable is empty).
  // In this case, we don't need to update the DNREnabledStaticRulesets since
  // it will not be changed. (It was empty list and it is still empty)
  if (matcher) {
    helper.SetEnabledStaticRulesets(load_data.extension_id,
                                    matcher->ComputeStaticRulesetIDs());
  }

  std::move(callback).Run(std::nullopt);
}

void RulesMonitorService::OnDynamicRulesUpdated(
    ApiCallback callback,
    LoadRequestData load_data,
    std::optional<std::string> error) {
  DCHECK_EQ(1u, load_data.rulesets.size());

  const bool has_error = error.has_value();

  LogMetricsAndUpdateChecksumsIfNeeded(load_data);

  // Respond to the extension.
  std::move(callback).Run(std::move(error));

  // It's possible that the extension has been disabled since the initial update
  // rule request. If it's disabled, do nothing.
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(load_data.extension_id);
  if (!extension) {
    return;
  }

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK_EQ(dynamic_ruleset.did_load_successfully(), !has_error);

  if (!dynamic_ruleset.did_load_successfully()) {
    return;
  }

  DCHECK(dynamic_ruleset.new_checksum());

  // Update the dynamic ruleset.
  UpdateRulesetMatcher(*extension, dynamic_ruleset.TakeMatcher());
}

void RulesMonitorService::RemoveCompositeMatcher(
    const ExtensionId& extension_id) {
  ruleset_manager_.RemoveRuleset(extension_id);
  action_tracker_.ClearExtensionData(extension_id);
}

void RulesMonitorService::AddCompositeMatcher(
    const Extension& extension,
    CompositeMatcher::MatcherList matchers) {
  if (matchers.empty()) {
    return;
  }

  auto matcher = std::make_unique<CompositeMatcher>(
      std::move(matchers), extension.id(),
      GetHostPermissionsAlwaysRequired(extension));
  ruleset_manager_.AddRuleset(extension.id(), std::move(matcher));
}

void RulesMonitorService::UpdateRulesetMatcher(
    const Extension& extension,
    std::unique_ptr<RulesetMatcher> ruleset_matcher) {
  CompositeMatcher* matcher =
      ruleset_manager_.GetMatcherForExtension(extension.id());

  // The extension didn't have a corresponding CompositeMatcher.
  if (!matcher) {
    CompositeMatcher::MatcherList matchers;
    matchers.push_back(std::move(ruleset_matcher));
    AddCompositeMatcher(extension, std::move(matchers));
    return;
  }

  matcher->AddOrUpdateRuleset(std::move(ruleset_matcher));
}

void RulesMonitorService::LogMetricsAndUpdateChecksumsIfNeeded(
    const LoadRequestData& load_data) {
  for (const RulesetInfo& ruleset : load_data.rulesets) {
    // The |load_ruleset_result()| might be empty if CreateVerifiedMatcher
    // wasn't called on the ruleset.
    if (ruleset.load_ruleset_result()) {
      LogLoadRulesetResult(*ruleset.load_ruleset_result());
    }
  }

  // The extension may have been uninstalled by this point. Return early if
  // that's the case.
  if (!extension_registry_->GetInstalledExtension(load_data.extension_id)) {
    return;
  }

  // Update checksums for all rulesets.
  // Note: We also do this for a non-enabled extension. The ruleset on the disk
  // has already been modified at this point. So we do want to update the
  // checksum for it to be in sync with what's on disk.
  PrefsHelper helper(*prefs_);
  for (const RulesetInfo& ruleset : load_data.rulesets) {
    if (!ruleset.new_checksum()) {
      continue;
    }

    if (ruleset.source().is_dynamic_ruleset()) {
      helper.SetDynamicRulesetChecksum(load_data.extension_id,
                                       *ruleset.new_checksum());
    } else {
      helper.SetStaticRulesetChecksum(load_data.extension_id,
                                      ruleset.source().id(),
                                      *ruleset.new_checksum());
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
  DependsOn(WebRequestEventRouterFactory::GetInstance());
}

}  // namespace extensions
