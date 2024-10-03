// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"

#import <vector>

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notimplemented.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/public/utils.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_action_context.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

using tab_groups::utils::GetLocalTabGroupInfo;
using tab_groups::utils::LocalTabGroupInfo;

namespace tab_groups {
namespace {

class ScopedLocalObservationPauserImpl : public ScopedLocalObservationPauser {
 public:
  explicit ScopedLocalObservationPauserImpl(
      TabGroupLocalUpdateObserver* local_observer);
  ~ScopedLocalObservationPauserImpl() override;

  // Disallow copy/assign.
  ScopedLocalObservationPauserImpl(const ScopedLocalObservationPauserImpl&) =
      delete;
  ScopedLocalObservationPauserImpl& operator=(
      const ScopedLocalObservationPauserImpl&) = delete;

 private:
  raw_ptr<TabGroupLocalUpdateObserver> local_observer_;
};

ScopedLocalObservationPauserImpl::ScopedLocalObservationPauserImpl(
    TabGroupLocalUpdateObserver* local_observer)
    : local_observer_(local_observer) {
  local_observer_->SetSyncUpdatePaused(/*paused=*/true);
}

ScopedLocalObservationPauserImpl::~ScopedLocalObservationPauserImpl() {
  local_observer_->SetSyncUpdatePaused(/*paused=*/false);
}

}  // namespace

IOSTabGroupSyncDelegate::IOSTabGroupSyncDelegate(
    BrowserList* browser_list,
    TabGroupSyncService* sync_service,
    std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer)
    : browser_list_(browser_list),
      sync_service_(sync_service),
      local_update_observer_(std::move(local_update_observer)) {
  CHECK(local_update_observer_);
}

IOSTabGroupSyncDelegate::~IOSTabGroupSyncDelegate() {}

void IOSTabGroupSyncDelegate::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  IOSTabGroupActionContext* ios_context =
      static_cast<IOSTabGroupActionContext*>(context.get());
  const auto saved_tab_group = sync_service_->GetGroup(sync_tab_group_id);
  Browser* origin_browser = ios_context->browser;

  if (!saved_tab_group || !origin_browser) {
    // The group doesn't exist or there is no origin browser.
    return;
  }

  Browser* target_browser = origin_browser;

  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, *saved_tab_group);
  const TabGroup* group = tab_group_info.tab_group;
  if (group) {
    if (!tab_group_info.browser) {
      return;
    }
    target_browser = tab_group_info.browser;

    if (target_browser != origin_browser) {
      base::RecordAction(
          base::UserMetricsAction("MobileOpenGroupOpenInOtherBrowser"));
      // The group is in another window.
      SceneState* target_scene_state = target_browser->GetSceneState();
      UISceneActivationRequestOptions* options =
          [[UISceneActivationRequestOptions alloc] init];
      options.requestingScene = origin_browser->GetSceneState().scene;

      if (@available(iOS 17, *)) {
        UISceneSessionActivationRequest* request =
            [UISceneSessionActivationRequest
                requestWithSession:target_scene_state.scene.session];
        request.options = options;
        [[UIApplication sharedApplication]
            activateSceneSessionForRequest:request
                              errorHandler:^(NSError* error) {
                                LOG(ERROR) << base::SysNSStringToUTF8(
                                    error.localizedDescription);
                                NOTREACHED();
                              }];

      } else {
        [[UIApplication sharedApplication]
            requestSceneSessionActivation:target_scene_state.scene.session
                             userActivity:nil
                                  options:options
                             errorHandler:^(NSError* error) {
                               LOG(ERROR) << base::SysNSStringToUTF8(
                                   error.localizedDescription);
                               NOTREACHED();
                             }];
      }

      if (!target_scene_state.UIEnabled) {
        return;
      }

      CommandDispatcher* dispatcher = target_browser->GetCommandDispatcher();
      id<ApplicationCommands> applicationHandler =
          HandlerForProtocol(dispatcher, ApplicationCommands);
      [applicationHandler displayTabGridInMode:TabGridOpeningMode::kRegular];
      id<TabGroupsCommands> tabGroupsHandler =
          HandlerForProtocol(dispatcher, TabGroupsCommands);
      [tabGroupsHandler showTabGroup:group];

      return;
    }
    base::RecordAction(base::UserMetricsAction("MobileOpenGroupOpenInBrowser"));
  } else {
    base::RecordAction(base::UserMetricsAction("MobileOpenGroupClosed"));

    std::optional<LocalTabGroupID> tab_group_id =
        CreateLocalTabGroupImpl(*saved_tab_group, origin_browser);
    if (!tab_group_id) {
      return;
    }
    LocalTabGroupInfo new_tab_group_info =
        GetLocalTabGroupInfo(browser_list_, tab_group_id.value());
    group = new_tab_group_info.tab_group;
  }

  CommandDispatcher* dispatcher = target_browser->GetCommandDispatcher();
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  [applicationHandler displayTabGridInMode:TabGridOpeningMode::kRegular];

  id<TabGroupsCommands> tabGroupsHandler =
      HandlerForProtocol(dispatcher, TabGroupsCommands);
  [tabGroupsHandler showTabGroup:group];

  // Moves back to the grid containing the group after it has been opened.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        id<TabGridCommands> tabGridHandler =
            HandlerForProtocol(dispatcher, TabGridCommands);
        [tabGridHandler bringGroupIntoView:group animated:NO];
      });
}

std::unique_ptr<ScopedLocalObservationPauser>
IOSTabGroupSyncDelegate::CreateScopedLocalObserverPauser() {
  return std::make_unique<ScopedLocalObservationPauserImpl>(
      local_update_observer_.get());
}

void IOSTabGroupSyncDelegate::CreateLocalTabGroup(
    const SavedTabGroup& saved_tab_group) {
  CreateLocalTabGroupImpl(saved_tab_group, nullptr);
}

void IOSTabGroupSyncDelegate::CloseLocalTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  auto lock = CreateScopedLocalObserverPauser();

  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, local_tab_group_id);
  if (!tab_group_info.tab_group) {
    // The group is closed locally.
    return;
  }

  CloseAllWebStatesInGroup(*tab_group_info.web_state_list,
                           tab_group_info.tab_group,
                           WebStateList::CLOSE_NO_FLAGS);
}

void IOSTabGroupSyncDelegate::DisconnectLocalTabGroup(
    const LocalTabGroupID& local_id) {
  NOTIMPLEMENTED();
}

void IOSTabGroupSyncDelegate::UpdateLocalTabGroup(
    const SavedTabGroup& saved_tab_group) {
  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, saved_tab_group);
  if (!tab_group_info.tab_group) {
    // The group is closed locally.
    return;
  }
  auto lock = CreateScopedLocalObserverPauser();

  const TabGroup* tab_group = tab_group_info.tab_group;
  const TabGroupRange& tab_group_range = tab_group->range();
  WebStateList* web_state_list = tab_group_info.web_state_list;

  // Start a batch operation.
  WebStateList::ScopedBatchOperation observer_lock =
      web_state_list->StartBatchOperation();

  // Update the visual data.
  UpdateLocalGroupVisualData(tab_group_info, saved_tab_group);

  // Loop on each `saved_tabs` entry to synchronize local tabs, this involves:
  // - Matching and updating existing local tabs.
  // - Creating new local tabs for missing saved entries.
  // - Removing local tabs that are no longer present in the `saved_tab_group`.
  std::vector<SavedTabGroupTab> saved_tabs = saved_tab_group.saved_tabs();
  for (size_t index = 0; index < saved_tabs.size(); index++) {
    const SavedTabGroupTab& saved_tab = saved_tabs[index];
    // The local index of the `saved_tab`.
    int local_web_state_index = tab_group_range.range_begin() + index;

    // Check if the computed `local_web_state_index` exists locally.
    bool local_web_state_index_exists =
        saved_tab.local_tab_id().has_value() &&
        tab_group_range.contains(local_web_state_index);
    if (local_web_state_index_exists) {
      // Retreive the `WebState` at the `local_web_state_index`.
      const web::WebStateID local_web_state_id =
          web::WebStateID::FromSerializedValue(
              saved_tab.local_tab_id().value());
      web::WebState* local_web_state =
          web_state_list->GetWebStateAt(local_web_state_index);

      // Check if the `saved_tab` id is the same as the `local_web_state` id.
      // If true, update the `local_web_state` and continue.
      if (local_web_state_id == local_web_state->GetUniqueIdentifier()) {
        UpdateLocalWebState(local_web_state_index, local_web_state,
                            tab_group_info, saved_tab);
        continue;
      }

      // Otherwise, check if there is a local tab in the group that matches the
      // `saved_tab` id.
      // If true :
      //   - move the local webState to `local_web_state_index`.
      //   - update it and continue.
      int source_web_state_index =
          GetWebStateIndex(web_state_list, WebStateSearchCriteria{
                                               .identifier = local_web_state_id,
                                           });
      if (tab_group_range.contains(source_web_state_index)) {
        web_state_list->MoveWebStateAt(source_web_state_index,
                                       local_web_state_index);

        UpdateLocalWebState(
            local_web_state_index,
            web_state_list->GetWebStateAt(local_web_state_index),
            tab_group_info, saved_tab);
        continue;
      }
    }

    // If the `saved_tab` does not match any local webState, add a new local
    // webState and move it to the group.
    TabInsertionBrowserAgent* tab_insertion_browser_agent =
        TabInsertionBrowserAgent::FromBrowser(tab_group_info.browser);
    web::WebState* local_web_state =
        InsertDistantTab(saved_tab, tab_insertion_browser_agent,
                         local_web_state_index, tab_group);

    // Do the association on the server.
    UpdateLocalTabId(local_web_state, tab_group, saved_tab);
  }

  // If there are more tabs in the local group, that means some tabs have been
  // deleted. Remove them.
  int tabs_to_delete = tab_group_range.count() -
                       static_cast<int>(saved_tab_group.saved_tabs().size());
  CHECK(tabs_to_delete >= 0);
  for (int count = 0; count < tabs_to_delete; count++) {
    web_state_list->CloseWebStateAt(tab_group_range.range_end() - 1,
                                    WebStateList::CLOSE_NO_FLAGS);
  }
}

std::vector<LocalTabGroupID> IOSTabGroupSyncDelegate::GetLocalTabGroupIds() {
  std::vector<LocalTabGroupID> local_tab_group_ids;
  for (Browser* browser :
       browser_list_->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (const TabGroup* group : web_state_list->GetGroups()) {
      local_tab_group_ids.emplace_back(group->tab_group_id());
    }
  }

  return local_tab_group_ids;
}

std::vector<LocalTabID> IOSTabGroupSyncDelegate::GetLocalTabIdsForTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  std::vector<LocalTabID> local_tab_ids;

  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, local_tab_group_id);
  if (!tab_group_info.tab_group) {
    // The group is closed locally.
    return local_tab_ids;
  }

  for (int i : tab_group_info.tab_group->range()) {
    LocalTabID local_tab_id = tab_group_info.web_state_list->GetWebStateAt(i)
                                  ->GetUniqueIdentifier()
                                  .identifier();
    local_tab_ids.emplace_back(local_tab_id);
  }

  return local_tab_ids;
}

void IOSTabGroupSyncDelegate::CreateRemoteTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  if (sync_service_->GetGroup(local_tab_group_id)) {
    // The group already exists.
    return;
  }

  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, local_tab_group_id);
  if (!tab_group_info.tab_group) {
    // This group doesn't exists locally.
    return;
  }

  const TabGroup* tab_group = tab_group_info.tab_group;
  WebStateList* web_state_list = tab_group_info.web_state_list;
  const TabGroupRange& tab_group_range = tab_group->range();

  auto lock = CreateScopedLocalObserverPauser();

  // Generate and id for the synced tab group.
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  // Create a vector of `saved_tabs` based on local tabs.
  std::vector<SavedTabGroupTab> saved_tabs;
  for (int index = 0; index < tab_group_range.count(); ++index) {
    int web_state_index = tab_group_range.range_begin() + index;
    web::WebState* web_state = web_state_list->GetWebStateAt(web_state_index);

    SavedTabGroupTab saved_tab(
        web_state->GetVisibleURL(), web_state->GetTitle(), saved_tab_group_id,
        std::make_optional(index), /*position=*/std::nullopt,
        web_state->GetUniqueIdentifier().identifier());
    saved_tabs.push_back(saved_tab);
  }

  SavedTabGroup saved_group(base::SysNSStringToUTF16(tab_group->GetRawTitle()),
                            tab_group->visual_data().color(), saved_tabs,
                            /*position=*/std::nullopt, saved_tab_group_id,
                            tab_group->tab_group_id());
  sync_service_->AddGroup(saved_group);
}

Browser* IOSTabGroupSyncDelegate::GetMostActiveSceneBrowser() {
  std::set<Browser*> all_browsers =
      browser_list_->BrowsersOfType(BrowserList::BrowserType::kRegular);

  Browser* browser = nullptr;
  for (Browser* browser_to_check : all_browsers) {
    // The pointer to the scene state is weak, so it could be nil. In that case,
    // the activation level will be 0 (lowest).
    if (browser && browser->GetSceneState().activationLevel >=
                       browser_to_check->GetSceneState().activationLevel) {
      continue;
    }
    browser = browser_to_check;
    if (browser_to_check->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      break;
    }
  }
  return browser;
}

web::WebState* IOSTabGroupSyncDelegate::InsertDistantTab(
    const SavedTabGroupTab& tab,
    TabInsertionBrowserAgent* tab_insertion_browser_agent,
    int web_state_index,
    const TabGroup* tab_group) {
  GURL url_to_open = tab.url();
  std::u16string title = tab.title();
  if (!IsURLValidForSavedTabGroups(url_to_open)) {
    url_to_open = GetDefaultUrlAndTitle().first;
    title = GetDefaultUrlAndTitle().second;
  }

  web::NavigationManager::WebLoadParams web_params(url_to_open);
  TabInsertion::Params tab_insertion_params;
  tab_insertion_params.index = web_state_index;
  tab_insertion_params.in_background = true;
  tab_insertion_params.instant_load = false;
  tab_insertion_params.placeholder_title = title;
  if (tab_group) {
    tab_insertion_params.insert_in_group = true;
    tab_insertion_params.tab_group = tab_group->GetWeakPtr();
  }
  web::WebState* web_state = tab_insertion_browser_agent->InsertWebState(
      web_params, tab_insertion_params);
  local_update_observer_->IgnoreNavigationForWebState(web_state);
  return web_state;
}

void IOSTabGroupSyncDelegate::UpdateLocalWebState(
    int web_state_index,
    web::WebState* web_state,
    LocalTabGroupInfo tab_group_info,
    const SavedTabGroupTab& saved_tab) {
  // Early return if URLs are the same.
  if (saved_tab.url() == web_state->GetVisibleURL()) {
    return;
  }

  // Dont navigate to the new URL if its not valid for sync. We allow local
  // state to differ from sync in this case, especially since we want to honor
  // the local URL after restarts.
  if (!IsURLValidForSavedTabGroups(saved_tab.url())) {
    return;
  }

  WebStateList* web_state_list = tab_group_info.web_state_list;

  // If the `web_state` is the active index, open and load the updated URL.
  if (web_state_list->active_index() == web_state_index) {
    local_update_observer_->IgnoreNavigationForWebState(web_state);

    web_state->OpenURL(web::WebState::OpenURLParams(
        saved_tab.url(), web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_GENERATED, /*is_renderer_initiated=*/false));
    return;
  }

  // Otherwise, in order to update the URL without loading it, replace the
  // current `web_state` with a new one that has not been loaded.
  // To avoid accidentally closing a group with only one tab, add the new tab to
  // the group before removing the old one.
  TabInsertionBrowserAgent* tab_insertion_browser_agent =
      TabInsertionBrowserAgent::FromBrowser(tab_group_info.browser);
  web::WebState* local_web_state =
      InsertDistantTab(saved_tab, tab_insertion_browser_agent, web_state_index,
                       tab_group_info.tab_group);
  web_state_list->CloseWebStateAt(web_state_index + 1,
                                  WebStateList::CLOSE_NO_FLAGS);

  // Do the association on the server.
  UpdateLocalTabId(local_web_state, tab_group_info.tab_group, saved_tab);
}

void IOSTabGroupSyncDelegate::UpdateLocalTabId(
    web::WebState* web_state,
    const TabGroup* tab_group,
    const SavedTabGroupTab& saved_tab) {
  sync_service_->UpdateLocalTabId(
      tab_group->tab_group_id(), saved_tab.saved_tab_guid(),
      web_state->GetUniqueIdentifier().identifier());
}

// Updates the visual data of the local `tab_group` to match the
// `SavedTabGroup`.
void IOSTabGroupSyncDelegate::UpdateLocalGroupVisualData(
    utils::LocalTabGroupInfo tab_group_info,
    const SavedTabGroup& saved_tab_group) {
  const TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      saved_tab_group.title(), saved_tab_group.color(),
      tab_group_info.tab_group->visual_data().is_collapsed());
  tab_group_info.web_state_list->UpdateGroupVisualData(tab_group_info.tab_group,
                                                       visual_data);
}

std::optional<LocalTabGroupID> IOSTabGroupSyncDelegate::CreateLocalTabGroupImpl(
    const SavedTabGroup& saved_tab_group,
    Browser* browser) {
  if (saved_tab_group.saved_tabs().size() == 0) {
    return std::nullopt;
  }

  LocalTabGroupInfo tab_group_info =
      GetLocalTabGroupInfo(browser_list_, saved_tab_group);
  if (tab_group_info.tab_group) {
    // This group already exists locally.
    return std::nullopt;
  }

  // If no browser was passed, get the most active one.
  browser = browser ? browser : GetMostActiveSceneBrowser();

  if (!browser) {
    return std::nullopt;
  }

  auto lock = CreateScopedLocalObserverPauser();
  WebStateList* web_state_list = browser->GetWebStateList();

  TabInsertionBrowserAgent* tab_insertion_browser_agent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  int insertion_index = web_state_list->count();
  std::set<int> inserted_indexes;
  // To do the mapping on the service, the local group ID is necessary. Keep a
  // temporary mapping until the group is created.
  std::map<const base::Uuid, const LocalTabID> sync_to_local_tab_mapping;

  for (const SavedTabGroupTab& tab : saved_tab_group.saved_tabs()) {
    web::WebState* web_state =
        InsertDistantTab(tab, tab_insertion_browser_agent, insertion_index,
                         /*web_state_index=*/nil);
    sync_to_local_tab_mapping.insert(
        {tab.saved_tab_guid(), web_state->GetUniqueIdentifier().identifier()});
    inserted_indexes.insert(insertion_index);
    insertion_index++;
  }

  TabGroupVisualData visual_data = {saved_tab_group.title(),
                                    saved_tab_group.color()};
  TabGroupId local_group_id = TabGroupId::GenerateNew();

  // Do the association on the server before creating it in the WebStateList to
  // avoid creating another group in the service.
  sync_service_->UpdateLocalTabGroupMapping(saved_tab_group.saved_guid(),
                                            local_group_id,
                                            OpeningSource::kAutoOpenedFromSync);
  for (auto const& [sync_tab_id, local_tab_id] : sync_to_local_tab_mapping) {
    sync_service_->UpdateLocalTabId(local_group_id, sync_tab_id, local_tab_id);
  }

  web_state_list->CreateGroup(inserted_indexes, visual_data, local_group_id);

  return std::make_optional(local_group_id);
}
}  // namespace tab_groups
