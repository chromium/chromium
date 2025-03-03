// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/service_status.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_action_context.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_flow_outcome.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_preview_item.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace collaboration {

namespace {

// The size in point of the join group image.
const CGFloat kJoinGroupImageSize = 64.0;

// The minimum size of tab favicons in points.
const CGFloat kFaviconMinimumSize = 8.0;

// The desired size of tab favicons in points.
const CGFloat kFaviconSize = 16.0;

// The opacity of the scrim view.
const CGFloat kScrimOpacity = 0.3;

// Maximum delay to return preview items.
constexpr base::TimeDelta kFetchPreviewItemsTimeDelay = base::Seconds(15);

// Converts `outcome` between the two enums.
CollaborationControllerDelegate::Outcome ConvertOutcome(
    ShareKitFlowOutcome outcome) {
  switch (outcome) {
    case ShareKitFlowOutcome::kSuccess:
      return CollaborationControllerDelegate::Outcome::kSuccess;
    case ShareKitFlowOutcome::kFailure:
      return CollaborationControllerDelegate::Outcome::kFailure;
    case ShareKitFlowOutcome::kCancel:
      return CollaborationControllerDelegate::Outcome::kCancel;
  }
}

}  // namespace

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate(
    Browser* browser,
    UIViewController* base_view_controller)
    : browser_(browser), base_view_controller_(base_view_controller) {
  CHECK(browser_);
  CHECK(base_view_controller_);
  ProfileIOS* profile = browser_->GetProfile();

  share_kit_service_ = ShareKitServiceFactory::GetForProfile(profile);
  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  favicon_loader_ = IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  favicons_grid_configurator_ =
      std::make_unique<TabGroupFaviconsGridConfigurator>(tab_group_sync_service,
                                                         favicon_loader_);
  CHECK(share_kit_service_);
  CHECK(favicon_loader_);
  CHECK(favicons_grid_configurator_);
}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::PrepareFlowUI(
    base::OnceCallback<void()> exit_callback,
    ResultCallback result) {
  // TODO(crbug.com/399584431): Improve the design of the spinner/scrim.
  scrim_view_ = [[UIView alloc] init];
  scrim_view_.backgroundColor = [UIColor colorWithWhite:0 alpha:kScrimOpacity];
  UIActivityIndicatorView* activity_view = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  activity_view.translatesAutoresizingMaskIntoConstraints = NO;
  [scrim_view_ addSubview:activity_view];
  AddSameCenterConstraints(scrim_view_, activity_view);
  [activity_view startAnimating];
  scrim_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [base_view_controller_.view addSubview:scrim_view_];
  AddSameConstraints(base_view_controller_.view, scrim_view_);
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowError(const ErrorInfo& error,
                                                   ResultCallback result) {
  NSString* title = base::SysUTF8ToNSString(error.error_header);
  NSString* message = base::SysUTF8ToNSString(error.error_body);

  auto result_block = base::CallbackToBlock(std::move(result));
  alert_coordinator_ =
      [[AlertCoordinator alloc] initWithBaseViewController:base_view_controller_
                                                   browser:browser_
                                                     title:title
                                                   message:message];
  [alert_coordinator_
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_ERROR_GOT_IT)
                action:^{
                  result_block(
                      CollaborationControllerDelegate::Outcome::kSuccess);
                }
                 style:UIAlertActionStyleDefault];
  [alert_coordinator_ start];
}

void IOSCollaborationControllerDelegate::Cancel(ResultCallback result) {
  if (session_id_) {
    share_kit_service_->CancelSession(session_id_);
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    ResultCallback result) {
  CollaborationService* collaboration_service =
      CollaborationServiceFactory::GetForProfile(browser_->GetProfile());
  ServiceStatus service_status = collaboration_service->GetServiceStatus();

  AuthenticationOperation operation;

  switch (service_status.signin_status) {
    case SigninStatus::kNotSignedIn:
      operation = AuthenticationOperation::kSheetSigninAndHistorySync;
      break;

    case SigninStatus::kSignedInPaused:
      // TODO(crbug.com/390153810): Handle the sign in paused.
      NOTREACHED();

    case SigninStatus::kSignedIn:
      operation = AuthenticationOperation::kHistorySync;
      break;
  }

  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(base::BindOnce(
      &IOSCollaborationControllerDelegate::OnAuthenticationComplete,
      weak_ptr_factory_.GetWeakPtr(), std::move(result)));

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::kCollaborationTabGroup
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:completion_block];

  command.optionalHistorySync = NO;

  [application_handler showSignin:command
               baseViewController:base_view_controller_];
}

void IOSCollaborationControllerDelegate::NotifySignInAndSyncStatusChange() {
  share_kit_service_->PrimaryAccountChanged();
}

void IOSCollaborationControllerDelegate::ShowJoinDialog(
    const data_sharing::GroupToken& token,
    const data_sharing::SharedDataPreview& preview_data,
    ResultCallback result) {
  const auto& tab_group_preview = preview_data.shared_tab_group_preview;

  std::string group_title = tab_group_preview ? tab_group_preview->title : "";
  auto callback = base::BindOnce(
      &IOSCollaborationControllerDelegate::ConfigureAndJoinTabGroup,
      weak_ptr_factory_.GetWeakPtr(), token, group_title, std::move(result));

  // Check if preview data contains shared tab group preview information.
  if (tab_group_preview) {
    // If a preview is available, fetch the favicons for the tabs in the
    // preview. Once favicons are fetched and ShareKitPreviewItems are created,
    // the callback block is executed.
    FetchPreviewItems(tab_group_preview->tabs, std::move(callback));
  } else {
    // If no preview is available, immediately execute the callback
    // block with an empty array of preview items.
    std::move(callback).Run(@[]);
  }
}

void IOSCollaborationControllerDelegate::ShowShareDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultWithGroupTokenCallback result) {
  const TabGroup* tab_group = GetLocalGroup(either_id);
  if (!tab_group) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure,
                          data_sharing::GroupToken());
    return;
  }

  auto callback = base::BindOnce(
      &IOSCollaborationControllerDelegate::ConfigureAndShareTabGroup,
      weak_ptr_factory_.GetWeakPtr(), either_id, std::move(result), tab_group);

  favicons_grid_configurator_->FetchFaviconsGrid(tab_group,
                                                 std::move(callback));
}

void IOSCollaborationControllerDelegate::OnUrlReadyToShare(
    const data_sharing::GroupId& group_id,
    const GURL& url,
    ResultCallback result) {}

void IOSCollaborationControllerDelegate::ShowManageDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  const TabGroup* tab_group = GetLocalGroup(either_id);
  if (!tab_group) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  auto callback = base::BindOnce(
      &IOSCollaborationControllerDelegate::ConfigureAndManageTabGroup,
      weak_ptr_factory_.GetWeakPtr(), either_id, std::move(result), tab_group);

  favicons_grid_configurator_->FetchFaviconsGrid(tab_group,
                                                 std::move(callback));
}

void IOSCollaborationControllerDelegate::PromoteTabGroup(
    const data_sharing::GroupId& group_id,
    ResultCallback result) {
  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  base::Uuid sync_id;
  for (const tab_groups::SavedTabGroup& group :
       tab_group_sync_service->GetAllGroups()) {
    if (!group.collaboration_id().has_value()) {
      continue;
    }
    if (group.collaboration_id().value().value() == group_id.value()) {
      sync_id = group.saved_guid();
      if (sync_id.is_valid()) {
        break;
      }
    }
  }

  if (!sync_id.is_valid()) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
  }
  tab_group_sync_service->OpenTabGroup(
      sync_id,
      std::make_unique<tab_groups::IOSTabGroupActionContext>(browser_));
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::PromoteCurrentScreen() {
  // TODO(crbug.com/399595276): Implement this.
}

void IOSCollaborationControllerDelegate::OnFlowFinished() {
  [scrim_view_ removeFromSuperview];
}

void IOSCollaborationControllerDelegate::OnAuthenticationComplete(
    ResultCallback result,
    SigninCoordinatorResult sign_in_result,
    id<SystemIdentity> completion_info) {
  if (sign_in_result != SigninCoordinatorResultSuccess) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(browser_->GetProfile());
  syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();

  bool sync_opted_in = user_settings->GetSelectedTypes().HasAll(
      {syncer::UserSelectableType::kHistory,
       syncer::UserSelectableType::kTabs});

  CollaborationControllerDelegate::Outcome outcome =
      sync_opted_in ? CollaborationControllerDelegate::Outcome::kSuccess
                    : CollaborationControllerDelegate::Outcome::kFailure;

  std::move(result).Run(outcome);
}

const TabGroup* IOSCollaborationControllerDelegate::GetLocalGroup(
    const tab_groups::EitherGroupID& either_id) {
  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  if (!tab_group_sync_service) {
    return nullptr;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service->GetGroup(either_id);
  if (!saved_group.has_value()) {
    return nullptr;
  }

  const TabGroup* tab_group = nullptr;
  for (const TabGroup* group : browser_->GetWebStateList()->GetGroups()) {
    if (group->tab_group_id() == saved_group->local_group_id()) {
      tab_group = group;
      break;
    }
  }
  return tab_group;
}

void IOSCollaborationControllerDelegate::FetchPreviewItems(
    std::vector<data_sharing::TabPreview> tabs,
    PreviewItemsCallBack callback) {
  const int tabs_count = static_cast<int>(tabs.size());

  __block NSMutableArray<ShareKitPreviewItem*>* preview_items =
      [NSMutableArray arrayWithCapacity:tabs_count];

  // Init `preview_items` with default ShareKitPreviewItems.
  for (int i = 0; i < tabs_count; ++i) {
    ShareKitPreviewItem* preview_item = [[ShareKitPreviewItem alloc] init];
    preview_item.title = base::SysUTF8ToNSString(tabs[i].url.host());
    preview_item.image = SymbolWithPalette(
        DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kFaviconSize),
        @[ [UIColor colorNamed:kGrey400Color] ]);
    [preview_items addObject:preview_item];
  }

  __block int completed_count = 0;
  __block auto completion_block = base::CallbackToBlock(std::move(callback));
  __block bool completion_block_executed = false;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               kFetchPreviewItemsTimeDelay.InNanoseconds()),
                 dispatch_get_main_queue(), ^{
                   if (!completion_block_executed) {
                     completion_block_executed = true;
                     completion_block(preview_items);
                   }
                 });

  for (int i = 0; i < tabs_count; ++i) {
    const auto& tab = tabs[i];
    // Asynchronously load the favicon for the tab's URL.
    favicon_loader_->FaviconForPageUrl(
        tab.url, kFaviconSize, kFaviconMinimumSize,
        /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
          // Skip synchronously returned default favicon.
          if (completion_block_executed || attributes.usesDefaultImage) {
            return;
          }
          if (attributes.faviconImage) {
            ShareKitPreviewItem* item = preview_items[i];
            item.image = attributes.faviconImage;
          }
          completed_count++;

          // Check if all items have been fetched.
          if (completed_count == tabs_count) {
            completion_block_executed = true;
            completion_block(preview_items);
          }
        });
  }
}

void IOSCollaborationControllerDelegate::ConfigureAndJoinTabGroup(
    const data_sharing::GroupToken& token,
    const std::string& group_title,
    ResultCallback result,
    NSArray<ShareKitPreviewItem*>* preview_items) {
  ShareKitJoinConfiguration* config = [[ShareKitJoinConfiguration alloc] init];
  config.token = token;
  config.baseViewController = base_view_controller_;
  NSString* group_title_objc = base::SysUTF8ToNSString(group_title);
  group_title_objc =
      group_title_objc.length > 0
          ? group_title_objc
          : l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER,
                                          preview_items.count);
  config.displayName = group_title_objc;
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  config.previewItems = preview_items;
  config.previewImage = JoinGroupImage(preview_items);

  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completion = ^(ShareKitFlowOutcome outcome) {
    completion_block(ConvertOutcome(outcome));
  };

  session_id_ = share_kit_service_->JoinTabGroup(config);
}

void IOSCollaborationControllerDelegate::ConfigureAndShareTabGroup(
    const tab_groups::EitherGroupID& either_id,
    ResultWithGroupTokenCallback result,
    const TabGroup* tab_group,
    UIImage* faviconsGridImage) {
  if (!tab_group || !faviconsGridImage) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure,
                          data_sharing::GroupToken());
    return;
  }

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = tab_group;
  config.groupImage = faviconsGridImage;
  config.baseViewController = base_view_controller_;
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completion = ^(ShareKitFlowOutcome outcome) {
    completion_block(ConvertOutcome(outcome), data_sharing::GroupToken());
  };

  session_id_ = share_kit_service_->ShareTabGroup(config);
}

void IOSCollaborationControllerDelegate::ConfigureAndManageTabGroup(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result,
    const TabGroup* tab_group,
    UIImage* faviconsGridImage) {
  if (!tab_group || !faviconsGridImage) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  tab_groups::CollaborationId collaboration_id =
      tab_groups::utils::GetTabGroupCollabID(either_id, tab_group_sync_service);
  if (collaboration_id->empty()) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  ShareKitManageConfiguration* config =
      [[ShareKitManageConfiguration alloc] init];
  config.baseViewController = base_view_controller_;
  config.tabGroup = tab_group;
  config.groupImage = faviconsGridImage;
  config.collabID = base::SysUTF8ToNSString(collaboration_id.value());
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completion = ^(ShareKitFlowOutcome outcome) {
    completion_block(ConvertOutcome(outcome));
  };

  session_id_ = share_kit_service_->ManageTabGroup(config);
}

UIImage* IOSCollaborationControllerDelegate::JoinGroupImage(
    NSArray<ShareKitPreviewItem*>* preview_items) {
  CGRect frame = CGRectMake(0, 0, kJoinGroupImageSize, kJoinGroupImageSize);
  TabGroupFaviconsGrid* favicons_grid =
      [[TabGroupFaviconsGrid alloc] initWithFrame:frame];
  favicons_grid.translatesAutoresizingMaskIntoConstraints = NO;
  favicons_grid.numberOfTabs = [preview_items count];
  favicons_grid_configurator_->ConfigureFaviconsGrid(favicons_grid,
                                                     preview_items);
  [favicons_grid layoutIfNeeded];
  return ImageFromView(favicons_grid, nil, UIEdgeInsetsZero);
}

}  // namespace collaboration
