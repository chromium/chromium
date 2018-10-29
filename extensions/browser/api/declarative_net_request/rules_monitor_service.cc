// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {
namespace declarative_net_request {

namespace {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RulesMonitorService>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

void LoadRulesetOnIOThread(ExtensionId extension_id,
                           std::unique_ptr<RulesetMatcher> ruleset_matcher,
                           URLPatternSet allowed_pages,
                           InfoMap* info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);
  info_map->GetRulesetManager()->AddRuleset(
      extension_id, std::move(ruleset_matcher), std::move(allowed_pages));
}

void UnloadRulesetOnIOThread(ExtensionId extension_id, InfoMap* info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);
  info_map->GetRulesetManager()->RemoveRuleset(extension_id);
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<RulesMonitorService>*
RulesMonitorService::GetFactoryInstance() {
  return g_factory.Pointer();
}

bool RulesMonitorService::HasAnyRegisteredRulesets() const {
  return !extensions_with_rulesets_.empty();
}

bool RulesMonitorService::HasRegisteredRuleset(
    const ExtensionId& extension_id) const {
  return extensions_with_rulesets_.find(extension_id) !=
         extensions_with_rulesets_.end();
}

void RulesMonitorService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RulesMonitorService::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// Helper to pass information related to the ruleset being loaded.
struct RulesMonitorService::LoadRulesetInfo {
  LoadRulesetInfo(scoped_refptr<const Extension> extension,
                  int expected_ruleset_checksum,
                  URLPatternSet allowed_pages)
      : extension(std::move(extension)),
        expected_ruleset_checksum(expected_ruleset_checksum),
        allowed_pages(std::move(allowed_pages)) {}
  ~LoadRulesetInfo() = default;
  LoadRulesetInfo(LoadRulesetInfo&&) = default;
  LoadRulesetInfo& operator=(LoadRulesetInfo&&) = default;

  scoped_refptr<const Extension> extension;
  int expected_ruleset_checksum;
  URLPatternSet allowed_pages;

  // True in case the checksum of the indexed ruleset changed. If true,
  // |expected_ruleset_checksum| contains the updated checksum. This can only
  // happen in case of an incorrect indexed ruleset format version.
  bool checksum_updated_due_to_version_mismatch = false;

  DISALLOW_COPY_AND_ASSIGN(LoadRulesetInfo);
};

// Maintains state needed on |file_task_runner_|. Created on the UI thread, but
// should only be accessed on the extension file task runner.
class RulesMonitorService::FileSequenceState {
 public:
  FileSequenceState()
      : connector_(content::ServiceManagerConnection::GetForProcess()
                       ->GetConnector()
                       ->Clone()),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~FileSequenceState() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  }

  using LoadRulesetUICallback =
      base::OnceCallback<void(LoadRulesetInfo,
                              std::unique_ptr<RulesetMatcher>)>;
  // Loads ruleset for |info|. Invokes |ui_callback| with the RulesetMatcher
  // instance created, passing null on failure.
  void LoadRuleset(LoadRulesetInfo info,
                   LoadRulesetUICallback ui_callback) const {
    LoadRulesetInternal(std::move(info), std::move(ui_callback),
                        LoadFailedAction::kReindex);
  }

 private:
  // Describes the action to take if ruleset loading fails.
  enum class LoadFailedAction {
    kReindex,        // Reindexes the JSON ruleset.
    kSignalFailure,  // Signals failure on the UI thread.
  };

  // Internal helper to load the ruleset for |info|.
  void LoadRulesetInternal(LoadRulesetInfo info,
                           LoadRulesetUICallback ui_callback,
                           LoadFailedAction failed_action) const {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    std::unique_ptr<RulesetMatcher> matcher;
    RulesetMatcher::LoadRulesetResult result =
        RulesetMatcher::CreateVerifiedMatcher(
            file_util::GetIndexedRulesetPath(info.extension->path()),
            info.expected_ruleset_checksum, &matcher);
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.DeclarativeNetRequest.LoadRulesetResult", result,
        RulesetMatcher::kLoadResultMax);

    // |matcher| is valid only on success.
    DCHECK_EQ(result == RulesetMatcher::kLoadSuccess, !!matcher);

    const bool reindex_ruleset = result != RulesetMatcher::kLoadSuccess &&
                                 failed_action == LoadFailedAction::kReindex;

    if (!reindex_ruleset) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(std::move(ui_callback), std::move(info),
                         std::move(matcher)));
      return;
    }

    // Attempt to reindex the extension ruleset.

    // Store the extension pointer since |info| will subsequently be moved.
    const Extension* extension = info.extension.get();

    // Using a weak pointer here is safe since |ruleset_reindexed_callback| will
    // be called on this sequence itself.
    IndexAndPersistRulesCallback ruleset_reindexed_callback = base::BindOnce(
        &FileSequenceState::OnRulesetReindexed, weak_factory_.GetWeakPtr(),
        std::move(info), result, std::move(ui_callback));
    IndexAndPersistRules(connector_.get(), nullptr /*identity*/, *extension,
                         std::move(ruleset_reindexed_callback));
  }

  // Callback invoked when the JSON ruleset is reindexed.
  void OnRulesetReindexed(
      LoadRulesetInfo info,
      RulesetMatcher::LoadRulesetResult initial_failure_reason,
      LoadRulesetUICallback ui_callback,
      IndexAndPersistRulesResult result) const {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    // In case of updates to the ruleset version, the ruleset checksum can
    // change.
    if (result.success &&
        initial_failure_reason ==
            RulesetMatcher::LoadRulesetResult::kLoadErrorVersionMismatch) {
      info.expected_ruleset_checksum = result.ruleset_checksum;
      info.checksum_updated_due_to_version_mismatch = true;
    }

    // The checksum of the reindexed ruleset should have been the same as the
    // expected checksum obtained from prefs, in all cases except when the
    // ruleset version changes. If this is not the case, then there is some
    // other issue (like the JSON rules file has been modified from the one used
    // during installation or preferences are corrupted). But taking care of
    // these is beyond our scope here, so simply signal a failure.
    bool reindexing_success =
        result.success &&
        info.expected_ruleset_checksum == result.ruleset_checksum;
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
        reindexing_success);
    if (!reindexing_success) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(std::move(ui_callback), std::move(info),
                         nullptr /* matcher */));
      return;
    }

    // We already reindexed the extension ruleset once and it succeeded. If the
    // ruleset load fails again, there is some other issue. To prevent a cycle,
    // don't reindex on failure again.
    LoadRulesetInternal(std::move(info), std::move(ui_callback),
                        LoadFailedAction::kSignalFailure);
  }

  const std::unique_ptr<service_manager::Connector> connector_;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details. Mutable to allow GetWeakPtr() usage from const methods.
  mutable base::WeakPtrFactory<FileSequenceState> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceState);
};

// Helper to bridge tasks to FileSequenceState. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_state_(std::make_unique<FileSequenceState>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_state_));
  }

  void LoadRuleset(
      LoadRulesetInfo info,
      FileSequenceState::LoadRulesetUICallback load_ruleset_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceState::LoadRuleset,
                       base::Unretained(file_sequence_state_.get()),
                       std::move(info), std::move(load_ruleset_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceState> file_sequence_state_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceBridge);
};

RulesMonitorService::RulesMonitorService(
    content::BrowserContext* browser_context)
    : registry_observer_(this),
      file_sequence_bridge_(std::make_unique<FileSequenceBridge>()),
      info_map_(ExtensionSystem::Get(browser_context)->info_map()),
      prefs_(ExtensionPrefs::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      weak_factory_(this) {
  registry_observer_.Add(extension_registry_);
}

RulesMonitorService::~RulesMonitorService() = default;

/* Description of thread hops for various scenarios:
   On ruleset load success:
      UI -> File -> UI -> IO.
   On ruleset load failure:
      UI -> File -> UI.
   On ruleset unload:
      UI -> IO.
*/

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  int expected_ruleset_checksum;
  if (!prefs_->GetDNRRulesetChecksum(extension->id(),
                                     &expected_ruleset_checksum)) {
    return;
  }

  DCHECK(IsAPIAvailable());

  LoadRulesetInfo info(base::WrapRefCounted(extension),
                       expected_ruleset_checksum,
                       prefs_->GetDNRAllowedPages(extension->id()));

  FileSequenceState::LoadRulesetUICallback load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnRulesetLoaded,
                     weak_factory_.GetWeakPtr());

  file_sequence_bridge_->LoadRuleset(std::move(info),
                                     std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Return early if the extension does not have an active indexed ruleset.
  if (!extensions_with_rulesets_.erase(extension->id()))
    return;

  DCHECK(IsAPIAvailable());

  base::OnceClosure unload_ruleset_on_io_task = base::BindOnce(
      &UnloadRulesetOnIOThread, extension->id(), base::RetainedRef(info_map_));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(unload_ruleset_on_io_task));
}

void RulesMonitorService::OnRulesetLoaded(
    LoadRulesetInfo info,
    std::unique_ptr<RulesetMatcher> matcher) {
  // Update the ruleset checksum if needed.
  if (info.checksum_updated_due_to_version_mismatch) {
    prefs_->SetDNRRulesetChecksum(info.extension->id(),
                                  info.expected_ruleset_checksum);
  }

  if (!matcher) {
    // The ruleset failed to load. Notify the user.
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(info.extension->id())});
    return;
  }

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(info.extension->id()))
    return;

  extensions_with_rulesets_.insert(info.extension->id());
  for (auto& observer : observers_)
    observer.OnRulesetLoaded();

  base::OnceClosure load_ruleset_on_io = base::BindOnce(
      &LoadRulesetOnIOThread, info.extension->id(), std::move(matcher),
      std::move(info.allowed_pages), base::RetainedRef(info_map_));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(load_ruleset_on_io));
}

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::
    DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(WarningServiceFactory::GetInstance());
}

}  // namespace extensions
