// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "components/collaboration/public/collaboration_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"

@class AlertCoordinator;
class Browser;
class FaviconLoader;
class ProfileIOS;
enum class ShareKitFlowOutcome;
@class ShareKitPreviewItem;
class ShareKitService;
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);
@protocol SystemIdentity;
class TabGroup;
class TabGroupFaviconsGridConfigurator;
class TabGroupService;

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace syncer {
class SyncService;
}  // namespace syncer

namespace collaboration {

class CollaborationService;
enum class FlowType;

// Structure to hold parameters for IOSCollaborationControllerDelegate.
struct IOSCollaborationControllerDelegateParams {
  raw_ptr<TabGroupService> tab_group_service = nullptr;
  raw_ptr<ShareKitService> share_kit_service = nullptr;
  raw_ptr<FaviconLoader> favicon_loader = nullptr;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service = nullptr;
  raw_ptr<syncer::SyncService> sync_service = nullptr;
  raw_ptr<CollaborationService> collaboration_service = nullptr;

  UIViewController* base_view_controller = nil;
  FlowType flow_type;
};

// Creates IOSCollaborationControllerDelegateParams from `profile`.
IOSCollaborationControllerDelegateParams
CreateControllerDelegateParamsFromProfile(
    ProfileIOS* profile,
    UIViewController* base_view_controller,
    FlowType flow_type);

// iOS implementation of CollaborationControllerDelegate.
class IOSCollaborationControllerDelegate
    : public BrowserObserver,
      public CollaborationControllerDelegate {
 public:
  IOSCollaborationControllerDelegate(
      Browser* browser,
      IOSCollaborationControllerDelegateParams params);

  IOSCollaborationControllerDelegate(
      const IOSCollaborationControllerDelegate&) = delete;
  IOSCollaborationControllerDelegate& operator=(
      const IOSCollaborationControllerDelegate&) = delete;
  ~IOSCollaborationControllerDelegate() override;

  // CollaborationControllerDelegate.
  void PrepareFlowUI(base::OnceCallback<void()> exit_callback,
                     ResultCallback result) override;
  void ShowError(const ErrorInfo& error, ResultCallback result) override;
  void Cancel(ResultCallback result) override;
  void ShowAuthenticationUi(FlowType flow_type, ResultCallback result) override;
  void NotifySignInAndSyncStatusChange() override;
  void ShowJoinDialog(const data_sharing::GroupToken& token,
                      const data_sharing::SharedDataPreview& preview_data,
                      ResultCallback result) override;
  void ShowShareDialog(const tab_groups::EitherGroupID& either_id,
                       ResultWithGroupTokenCallback result) override;
  void OnUrlReadyToShare(const data_sharing::GroupId& group_id,
                         const GURL& url,
                         ResultCallback result) override;
  void ShowManageDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void ShowLeaveDialog(const tab_groups::EitherGroupID& either_id,
                       ResultCallback result) override;
  void ShowDeleteDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void PromoteTabGroup(const data_sharing::GroupId& group_id,
                       ResultCallback result) override;
  void PromoteCurrentScreen() override;
  void OnFlowFinished() override;

  // BrowserObserver.
  void BrowserDestroyed(Browser* browser) override;

  // Shares the tab group this delegate is associated with to the
  // `collaboration_group_id`, then, once the group is shared, generates the
  // share link from `collaboration_group_id` and `access_token` and passes it
  // to `callback`. This can only be called for a "share" flow, after
  // `ShowShareDialog` has been called.
  void ShareGroupAndGenerateLink(std::string collaboration_group_id,
                                 std::string access_token,
                                 base::OnceCallback<void(GURL)> callback);

  // Sets the `callback` used to present the leave or delete confirmation.
  void SetLeaveOrDeleteConfirmationCallback(
      base::OnceCallback<void(ResultCallback)> callback);

 private:
  using PreviewItemsCallBack =
      base::OnceCallback<void(NSArray<ShareKitPreviewItem*>*)>;

  // Common implementation of `ShowLeaveDialog:` and `ShowDeleteDialog:`.
  void ShowLeaveOrDeleteDialog(const tab_groups::EitherGroupID& either_id,
                               ResultCallback result);

  // Called when the authentication ui flow is complete.
  void OnAuthenticationComplete(ResultCallback result,
                                SigninCoordinatorResult sign_in_result,
                                id<SystemIdentity> completion_info);

  // Called when the join flow has successfully joined the collaboration group,
  // but the tab group hasn't been sync'ed yet. `dismiss_join_screen` needs to
  // be called to dismiss the join screen.
  void OnCollaborationJoinSuccess(ProceduralBlock dismiss_join_screen);

  // Called when the join flow is complete.
  void OnJoinComplete(ResultCallback result, Outcome outcome);

  // Called when the share flow is finished with an `outcome`.
  void OnShareFlowComplete(ShareKitFlowOutcome outcome);

  // Called when a group is about to be unshared. The unsharing is blocked until
  // `continuation_block` is called.
  void WillUnshareGroup(std::optional<tab_groups::LocalTabGroupID> local_id,
                        void (^continuation_block)(BOOL));

  // Called when the collaboration group is deleted, making the group unshared.
  void DidUnshareGroup(std::optional<tab_groups::LocalTabGroupID> local_id,
                       NSError* error);

  // Callback called when the user acknowledge the error.
  void ErrorAccepted(ResultCallback result);

  // Returns the local tab group that matches `either_id`.
  const TabGroup* GetLocalGroup(const tab_groups::EitherGroupID& either_id);

  // Fetches preview items for the `tabs` and executes `callback`.
  void FetchPreviewItems(std::vector<data_sharing::TabPreview> tabs,
                         PreviewItemsCallBack callback);

  // Configures the shareKit config for the join flow and starts the flow.
  void ConfigureAndJoinTabGroup(const data_sharing::GroupToken& token,
                                const std::string& group_title,
                                ResultCallback result,
                                NSArray<ShareKitPreviewItem*>* preview_items);

  // Configures the shareKit config for the share flow and starts the flow.
  void ConfigureAndShareTabGroup(const tab_groups::EitherGroupID& either_id,
                                 ResultWithGroupTokenCallback result,
                                 const TabGroup* tab_group,
                                 UIImage* faviconsGridImage);

  // Configures the shareKit config for the manage flow and starts the flow.
  void ConfigureAndManageTabGroup(const tab_groups::EitherGroupID& either_id,
                                  ResultCallback result,
                                  const TabGroup* tab_group,
                                  UIImage* faviconsGridImage);

  // Returns the join group image displayed in the join flow.
  UIImage* JoinGroupImage(NSArray<ShareKitPreviewItem*>* preview_items);

  // Presents the scrim view.
  void AddScrimView();

  // Removes the scrim view if it exists.
  void RemoveScrimView(bool delayed);

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<TabGroupService> tab_group_service_ = nullptr;
  raw_ptr<ShareKitService> share_kit_service_ = nullptr;
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  raw_ptr<collaboration::CollaborationService> collaboration_service_ = nullptr;

  std::unique_ptr<TabGroupFaviconsGridConfigurator> favicons_grid_configurator_;
  __weak UIViewController* base_view_controller_ = nil;
  // Collaboration flow that initiated this delegate.
  FlowType flow_type_;

  NSString* session_id_ = nil;
  AlertCoordinator* alert_coordinator_ = nil;
  // The scrim displayed on top of the base view to let the user know that
  // something is happening and prevent interaction with the rest of the app.
  UIView* scrim_view_ = nil;

  // Callback called to dismiss the join screen.
  base::OnceCallback<void()> dismiss_join_screen_callback_;

  // Callback called when the `browser` is destroyed.
  base::OnceCallback<void()> exit_callback_;

  // The tab group id used to register this delegate to the TabGroupService, if
  // any.
  std::optional<tab_groups::LocalTabGroupID> tab_group_service_registration_id_;

  // Callback that needs to be called to continue the share flow. This is set
  // when the "Share" screen is actually presented. Calling it with success
  // shares the group associated with this delegate and allows link generation.
  ResultWithGroupTokenCallback share_screen_callback_;

  // The callback to generate the link and continue the share flow (present the
  // share sheet).
  base::OnceCallback<void(GURL)> link_generation_callback_;

  // The callback called to present the leave or delete confirmation.
  base::OnceCallback<void(ResultCallback)>
      leave_or_delete_confirmation_callback_;

  base::WeakPtrFactory<IOSCollaborationControllerDelegate> weak_ptr_factory_{
      this};
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
