// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"

#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

IOSWebContentHandlerImpl::IOSWebContentHandlerImpl(web::WebState* web_state,
                                                   bool is_main_frame)
    : is_main_frame_(is_main_frame), web_state_(web_state) {}

IOSWebContentHandlerImpl::~IOSWebContentHandlerImpl() = default;

void IOSWebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const supervised_user::UrlFormatter& url_formatter,
    ApprovalRequestInitiatedCallback callback) {
  // Method unsupported on iOS.
  NOTREACHED();
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
