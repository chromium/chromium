// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_type.h"
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
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"
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
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using signin_metrics::AccessPoint;

namespace collaboration {

namespace {

// Converts `outcome` between the two enums.
CollaborationControllerDelegate::Outcome ConvertShareKitFlowOutcome(
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

// The size in point of the join group image.
const CGFloat kJoinGroupImageSize = 64.0;

// The minimum size of tab favicons in points.
const CGFloat kFaviconMinimumSize = 8.0;

// The desired size of tab favicons in points.
const CGFloat kFaviconSize = 16.0;

// The opacity of the scrim view.
const CGFloat kScrimOpacity = 0.3;

// The timing to show/hide the scrim view.
const CGFloat kScrimAnimationDelay = 0.5;
const CGFloat kScrimAnimationTiming = 0.25;

// Maximum delay to return preview items.
constexpr base::TimeDelta kFetchPreviewItemsTimeDelay = base::Seconds(5);

}  // namespace

IOSCollaborationControllerDelegateParams
CreateControllerDelegateParamsFromProfile(
    ProfileIOS* profile,
    UIViewController* base_view_controller,
    FlowType flow_type) {
  return IOSCollaborationControllerDelegateParams(
      {TabGroupServiceFactory::GetForProfile(profile),
       ShareKitServiceFactory::GetForProfile(profile),
       IOSChromeFaviconLoaderFactory::GetForProfile(profile),
       tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile),
       SyncServiceFactory::GetForProfile(profile),
       CollaborationServiceFactory::GetForProfile(profile),
       base_view_controller, flow_type});
}

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate(
    Browser* browser,
    IOSCollaborationControllerDelegateParams params)
    : browser_(browser) {
  CHECK(browser_);
  browser_->AddObserver(this);

  tab_group_service_ = params.tab_group_service;
  tab_group_sync_service_ = params.tab_group_sync_service;
  share_kit_service_ = params.share_kit_service;
  sync_service_ = params.sync_service;
  collaboration_service_ = params.collaboration_service;
  favicon_loader_ = params.favicon_loader;
  favicons_grid_configurator_ =
      std::make_unique<TabGroupFaviconsGridConfigurator>(
          tab_group_sync_service_, favicon_loader_);
  flow_type_ = params.flow_type;
  base_view_controller_ = params.base_view_controller;
  CHECK(tab_group_service_);
  CHECK(tab_group_sync_service_);
  CHECK(share_kit_service_);
  CHECK(sync_service_);
  CHECK(collaboration_service_);
  CHECK(favicon_loader_);
  CHECK(favicons_grid_configurator_);
  CHECK(base_view_controller_);
}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {
  if (IsInObserverList()) {
    CHECK(browser_);
    browser_->RemoveObserver(this);
    browser_ = nullptr;
  }
}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::PrepareFlowUI(
    base::OnceCallback<void()> exit_callback,
    ResultCallback result) {
  exit_callback_ = std::move(exit_callback);
  switch (flow_type_) {
    case FlowType::kJoin:
      AddScrimView();
      break;
    case FlowType::kShareOrManage:
    case FlowType::kLeaveOrDelete:
      break;
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowError(const ErrorInfo& error,
                                                   ResultCallback result) {
  if (!browser_) {
    return;
  }

  NSString* title = base::SysUTF8ToNSString(error.error_header);
  NSString* message = base::SysUTF8ToNSString(error.error_body);

  auto alert_action = base::CallbackToBlock(
      base::BindOnce(&IOSCollaborationControllerDelegate::ErrorAccepted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  // Make sure to present it on top of any visible view.
  UIViewController* top_view_controller =
      top_view_controller::TopPresentedViewControllerFrom(
          base_view_controller_);

  alert_coordinator_ =
      [[AlertCoordinator alloc] initWithBaseViewController:top_view_controller
                                                   browser:browser_
                                                     title:title
                                                   message:message];
  [alert_coordinator_
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_ERROR_GOT_IT)
                action:alert_action
                 style:UIAlertActionStyleDefault];
  [alert_coordinator_ start];
}

void IOSCollaborationControllerDelegate::Cancel(ResultCallback result) {
  if (!browser_) {
    return;
  }

  if (dismiss_join_screen_callback_) {
    std::move(dismiss_join_screen_callback_).Run();
  }

  if (session_id_) {
    share_kit_service_->CancelSession(session_id_);
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    FlowType flow_type,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

  // Make sure that the scrim view is added to avoid interaction with the app in
  // between the authentication steps.
  AddScrimView();

  ServiceStatus service_status = collaboration_service_->GetServiceStatus();

  AuthenticationOperation operation;

  switch (service_status.signin_status) {
    case SigninStatus::kNotSignedIn:
      operation = AuthenticationOperation::kSheetSigninAndHistorySync;
      break;

    case SigninStatus::kSigninDisabled:
      // TODO(crbug.com/390153810): Handle the sign in disabled case.
      NOTREACHED();

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

  AccessPoint access_point;
  SigninContextStyle context_style;
  BOOL fullScreenPromo = NO;
  switch (flow_type) {
    case FlowType::kJoin:
      access_point = AccessPoint::kCollaborationJoinTabGroup;
      context_style = SigninContextStyle::kCollaborationJoinTabGroup;
      fullScreenPromo = YES;
      break;
    case FlowType::kShareOrManage:
      access_point = AccessPoint::kCollaborationShareTabGroup;
      context_style = SigninContextStyle::kCollaborationShareTabGroup;
      break;
    case FlowType::kLeaveOrDelete:
      access_point = AccessPoint::kCollaborationLeaveOrDeleteTabGroup;
      context_style = SigninContextStyle::kDefault;
      break;
  }

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:access_point
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:completion_block];

  command.optionalHistorySync = NO;
  command.fullScreenPromo = fullScreenPromo;
  command.contextStyle = context_style;

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
  if (!browser_) {
    return;
  }

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
  if (!browser_) {
    return;
  }

  const TabGroup* tab_group = GetLocalGroup(either_id);
  if (!tab_group) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure,
                          data_sharing::GroupToken());
    return;
  }

  tab_group_service_registration_id_ =
      std::make_optional(tab_group->tab_group_id());
  tab_group_service_->RegisterCollaborationControllerDelegate(
      tab_group_service_registration_id_.value(),
      weak_ptr_factory_.GetWeakPtr());

  auto callback = base::BindOnce(
      &IOSCollaborationControllerDelegate::ConfigureAndShareTabGroup,
      weak_ptr_factory_.GetWeakPtr(), either_id, std::move(result), tab_group);

  favicons_grid_configurator_->FetchFaviconsGrid(tab_group,
                                                 std::move(callback));
}

void IOSCollaborationControllerDelegate::OnUrlReadyToShare(
    const data_sharing::GroupId& group_id,
    const GURL& url,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

  CHECK(link_generation_callback_);
  std::move(link_generation_callback_).Run(url);
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowManageDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

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

void IOSCollaborationControllerDelegate::ShowLeaveDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

  ShowLeaveOrDeleteDialog(either_id, std::move(result));
}

void IOSCollaborationControllerDelegate::ShowDeleteDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

  ShowLeaveOrDeleteDialog(either_id, std::move(result));
}

void IOSCollaborationControllerDelegate::PromoteTabGroup(
    const data_sharing::GroupId& group_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }

  if (dismiss_join_screen_callback_) {
    std::move(dismiss_join_screen_callback_).Run();
  }

  base::Uuid sync_id;
  for (const tab_groups::SavedTabGroup& group :
       tab_group_sync_service_->GetAllGroups()) {
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
  tab_group_sync_service_->OpenTabGroup(
      sync_id,
      std::make_unique<tab_groups::IOSTabGroupActionContext>(browser_));
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::PromoteCurrentScreen() {
  // TODO(crbug.com/399595276): Implement this.
}

void IOSCollaborationControllerDelegate::OnFlowFinished() {
  if (!browser_) {
    return;
  }

  if (tab_group_service_registration_id_) {
    tab_group_service_->UnregisterCollaborationControllerDelegate(
        tab_group_service_registration_id_.value());
  }
  if (dismiss_join_screen_callback_) {
    // The dismissal should be handled before the end of the flow.
    NOTREACHED(base::NotFatalUntil::M140);
    std::move(dismiss_join_screen_callback_).Run();
  }
  RemoveScrimView(/*delayed=*/false);
}

void IOSCollaborationControllerDelegate::ShareGroupAndGenerateLink(
    std::string collaboration_group_id,
    std::string access_token,
    base::OnceCallback<void(GURL)> callback) {
  if (!browser_) {
    return;
  }

  CHECK(share_screen_callback_);
  link_generation_callback_ = std::move(callback);
  data_sharing::GroupToken token(data_sharing::GroupId(collaboration_group_id),
                                 access_token);

  std::move(share_screen_callback_)
      .Run(CollaborationControllerDelegate::Outcome::kSuccess, token);
}

void IOSCollaborationControllerDelegate::SetLeaveOrDeleteConfirmationCallback(
    base::OnceCallback<void(ResultCallback)> callback) {
  leave_or_delete_confirmation_callback_ = std::move(callback);
}

#pragma mark - BrowserObserver

void IOSCollaborationControllerDelegate::BrowserDestroyed(Browser* browser) {
  browser->RemoveObserver(this);
  browser_ = nullptr;
  if (exit_callback_) {
    std::move(exit_callback_).Run();
  }
}

#pragma mark - Private

void IOSCollaborationControllerDelegate::ShowLeaveOrDeleteDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  CHECK(leave_or_delete_confirmation_callback_);

  auto final_result = base::BindOnce(
      [](base::WeakPtr<IOSCollaborationControllerDelegate> weak_this,
         ResultCallback inner_result, Outcome outcome) {
        if (weak_this && outcome == Outcome::kSuccess) {
          weak_this->AddScrimView();
        }
        std::move(inner_result).Run(outcome);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(result));

  std::move(leave_or_delete_confirmation_callback_)
      .Run(std::move(final_result));
}

void IOSCollaborationControllerDelegate::OnAuthenticationComplete(
    ResultCallback result,
    SigninCoordinatorResult sign_in_result,
    id<SystemIdentity> completion_info) {
  if (sign_in_result == SigninCoordinatorResultCanceledByUser) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kCancel);
    return;
  }

  if (sign_in_result != SigninCoordinatorResultSuccess) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  syncer::SyncUserSettings* user_settings = sync_service_->GetUserSettings();

  bool sync_opted_in = user_settings->GetSelectedTypes().HasAll(
      {syncer::UserSelectableType::kHistory,
       syncer::UserSelectableType::kTabs});

  CollaborationControllerDelegate::Outcome outcome =
      sync_opted_in ? CollaborationControllerDelegate::Outcome::kSuccess
                    : CollaborationControllerDelegate::Outcome::kFailure;

  std::move(result).Run(outcome);
}

void IOSCollaborationControllerDelegate::OnCollaborationJoinSuccess(
    void (^dismiss_join_screen)()) {
  RemoveScrimView(/*delayed=*/false);
  dismiss_join_screen_callback_ = base::BindOnce(dismiss_join_screen);
}

void IOSCollaborationControllerDelegate::OnJoinComplete(ResultCallback result,
                                                        Outcome outcome) {
  RemoveScrimView(/*delayed=*/false);
  std::move(result).Run(outcome);
}

void IOSCollaborationControllerDelegate::OnShareFlowComplete(
    ShareKitFlowOutcome outcome) {
  if (!share_screen_callback_) {
    // The screen has already been continued (for example by sharing the link).
    return;
  }
  std::move(share_screen_callback_)
      .Run(ConvertShareKitFlowOutcome(outcome), data_sharing::GroupToken());
}

void IOSCollaborationControllerDelegate::WillUnshareGroup(
    std::optional<tab_groups::LocalTabGroupID> local_id,
    void (^continuation_block)(BOOL)) {
  if (!local_id.has_value()) {
    continuation_block(YES);
  }
  tab_group_sync_service_->AboutToUnShareTabGroup(
      local_id.value(), base::BindOnce(continuation_block, YES));
}

void IOSCollaborationControllerDelegate::DidUnshareGroup(
    std::optional<tab_groups::LocalTabGroupID> local_id,
    NSError* error) {
  if (!local_id.has_value()) {
    return;
  }
  bool success = (error == nil);
  tab_group_sync_service_->OnTabGroupUnShareComplete(local_id.value(), success);
}

void IOSCollaborationControllerDelegate::ErrorAccepted(ResultCallback result) {
  if (dismiss_join_screen_callback_) {
    std::move(dismiss_join_screen_callback_).Run();
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

const TabGroup* IOSCollaborationControllerDelegate::GetLocalGroup(
    const tab_groups::EitherGroupID& either_id) {
  if (!tab_group_sync_service_) {
    return nullptr;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(either_id);
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

  // The scrim will be dismissed on completion.
  auto join_success_completion = base::BindOnce(
      &IOSCollaborationControllerDelegate::OnCollaborationJoinSuccess,
      weak_ptr_factory_.GetWeakPtr());
  config.joinCollaborationGroupSuccessBlock =
      base::CallbackToBlock(std::move(join_success_completion));

  auto completion_block = base::CallbackToBlock(
      base::BindOnce(&IOSCollaborationControllerDelegate::OnJoinComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result)));

  config.completion = ^(ShareKitFlowOutcome outcome) {
    completion_block(ConvertShareKitFlowOutcome(outcome));
  };

  session_id_ = share_kit_service_->JoinTabGroup(config);

  // The scrim will be dismissed on the completion.
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

  share_screen_callback_ = std::move(result);

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = tab_group;
  config.groupImage = faviconsGridImage;
  config.baseViewController = base_view_controller_;
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  config.completion = base::CallbackToBlock(
      base::BindOnce(&IOSCollaborationControllerDelegate::OnShareFlowComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  session_id_ = share_kit_service_->ShareTabGroup(config);

  // Remove the scrim view to avoid having it visible when dismissing the flow.
  RemoveScrimView(/*delayed=*/true);
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

  tab_groups::CollaborationId collaboration_id =
      tab_groups::utils::GetTabGroupCollabID(either_id,
                                             tab_group_sync_service_);
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(either_id);
  if (collaboration_id->empty() || !group.has_value()) {
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
    completion_block(ConvertShareKitFlowOutcome(outcome));
  };
  std::optional<tab_groups::LocalTabGroupID> local_id = group->local_group_id();
  config.willUnshareGroupBlock = base::CallbackToBlock(
      base::BindOnce(&IOSCollaborationControllerDelegate::WillUnshareGroup,
                     weak_ptr_factory_.GetWeakPtr(), local_id));

  config.didUnshareGroupBlock = base::CallbackToBlock(
      base::BindOnce(&IOSCollaborationControllerDelegate::DidUnshareGroup,
                     weak_ptr_factory_.GetWeakPtr(), local_id));

  session_id_ = share_kit_service_->ManageTabGroup(config);

  // Remove the scrim view to avoid having it visible when dismissing the flow.
  RemoveScrimView(/*delayed=*/true);
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

void IOSCollaborationControllerDelegate::AddScrimView() {
  if (scrim_view_) {
    return;
  }
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
}

void IOSCollaborationControllerDelegate::RemoveScrimView(bool delayed) {
  if (!scrim_view_) {
    return;
  }
  UIView* scrim = scrim_view_;
  scrim_view_ = nil;

  auto animation_block = ^{
    [UIView animateWithDuration:kScrimAnimationTiming
        animations:^{
          scrim.alpha = 0;
        }
        completion:^(BOOL finished) {
          [scrim removeFromSuperview];
        }];
  };

  if (delayed) {
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      (int64_t)(kScrimAnimationDelay * NSEC_PER_SEC)),
        dispatch_get_main_queue(), animation_block);
  } else {
    animation_block();
  }
}

}  // namespace collaboration
