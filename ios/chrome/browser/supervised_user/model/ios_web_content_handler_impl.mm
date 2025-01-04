// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"

#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/web_content_handler.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

IOSWebContentHandlerImpl::IOSWebContentHandlerImpl(
    web::WebState* web_state,
    id<ParentAccessCommands> commands_handler,
    bool is_main_frame)
    : is_main_frame_(is_main_frame),
      web_state_(web_state),
      commands_handler_(commands_handler) {
  CHECK(commands_handler_);
}

IOSWebContentHandlerImpl::~IOSWebContentHandlerImpl() = default;

void IOSWebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const supervised_user::UrlFormatter& url_formatter,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(base::FeatureList::IsEnabled(supervised_user::kLocalWebApprovals));

  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  GURL target_url = url_formatter.FormatUrl(url);
  base::OnceCallback<void(supervised_user::LocalApprovalResult)>
      completion_callback = base::BindOnce(
          &IOSWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
          weak_factory_.GetWeakPtr(), std::ref(*settings_service), target_url,
          base::TimeTicks::Now());

  // The command handler must stay alive after initialization.
  CHECK(commands_handler_);
  [commands_handler_
      showParentAccessBottomSheetForWebState:web_state_
                                  completion:base::CallbackToBlock(std::move(
                                                 completion_callback))];

  // Runs the `callback` to inform the caller that the flow initiation was
  // successful.
  std::move(callback).Run(true);
}

bool IOSWebContentHandlerImpl::IsMainFrame() const {
  return is_main_frame_;
}

void IOSWebContentHandlerImpl::CleanUpInfoBarOnMainFrame() {
  // There are no sticky infobars for iOS, infobars are implemented
  // as self-dismissed messages.
}

int64_t IOSWebContentHandlerImpl::GetInterstitialNavigationId() const {
  // Method not currently used on iOS. If needed, this
  // infomation is available in `chrome_web_client` and can be set here.
  NOTREACHED();
}

void IOSWebContentHandlerImpl::GoBack() {
  // GoBack only for main frame.
  CHECK(IsMainFrame());
  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (navigation_manager->CanGoBack()) {
    navigation_manager->GoBack();
  } else {
    // If we can't go back, close the tab asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&IOSWebContentHandlerImpl::Close,
                                  weak_factory_.GetWeakPtr()));
  }
}

void IOSWebContentHandlerImpl::Close() {
  CHECK(web_state_);
  web_state_->CloseWebState();
}

void IOSWebContentHandlerImpl::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    supervised_user::LocalApprovalResult approval_result) {
  WebContentHandler::OnLocalApprovalRequestCompleted(
      settings_service, url, start_time, approval_result);
}
