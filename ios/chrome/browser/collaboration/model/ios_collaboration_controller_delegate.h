// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/collaboration/public/collaboration_controller_delegate.h"

@class AlertCoordinator;
class Browser;
class FaviconLoader;
@class ShareKitPreviewItem;
class ShareKitService;
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);
@protocol SystemIdentity;
class TabGroup;
class TabGroupFaviconsGridConfigurator;

namespace collaboration {

// iOS implementation of CollaborationControllerDelegate.
class IOSCollaborationControllerDelegate
    : public CollaborationControllerDelegate {
 public:
  IOSCollaborationControllerDelegate(Browser* browser,
                                     UIViewController* base_view_controller);

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
  void ShowAuthenticationUi(ResultCallback result) override;
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
  void PromoteTabGroup(const data_sharing::GroupId& group_id,
                       ResultCallback result) override;
  void PromoteCurrentScreen() override;
  void OnFlowFinished() override;

 private:
  using PreviewItemsCallBack =
      base::OnceCallback<void(NSArray<ShareKitPreviewItem*>*)>;

  // Called when the authentication ui flow is complete.
  void OnAuthenticationComplete(ResultCallback result,
                                SigninCoordinatorResult sign_in_result,
                                id<SystemIdentity> completion_info);

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

  raw_ptr<ShareKitService> share_kit_service_;
  raw_ptr<FaviconLoader> favicon_loader_;
  raw_ptr<Browser> browser_;
  std::unique_ptr<TabGroupFaviconsGridConfigurator> favicons_grid_configurator_;

  __weak UIViewController* base_view_controller_;
  NSString* session_id_ = nil;
  AlertCoordinator* alert_coordinator_ = nil;
  // The scrim displayed on top of the base view to let the user know that
  // something is happening and prevent interaction with the rest of the app.
  UIView* scrim_view_ = nil;

  base::WeakPtrFactory<IOSCollaborationControllerDelegate> weak_ptr_factory_{
      this};
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
