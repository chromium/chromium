// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/supervised_user/core/browser/web_content_handler.h"
#import "url/gurl.h"

@protocol ParentAccessCommands;

namespace web {
class WebState;
}

namespace supervised_user {
class UrlFormatter;
}  // namespace supervised_user

// iOS-specific implementation of the web content handler.
class IOSWebContentHandlerImpl : public supervised_user::WebContentHandler {
 public:
  IOSWebContentHandlerImpl(web::WebState* web_state,
                           id<ParentAccessCommands> commands_handler,
                           bool is_main_frame);

  IOSWebContentHandlerImpl(const IOSWebContentHandlerImpl&) = delete;
  IOSWebContentHandlerImpl& operator=(const IOSWebContentHandlerImpl&) = delete;
  ~IOSWebContentHandlerImpl() override;

  // supervised_user::WebContentHandler implementation:
  void RequestLocalApproval(
      const GURL& url,
      const std::u16string& child_display_name,
      const supervised_user::UrlFormatter& url_formatter,
      const supervised_user::FilteringBehaviorReason& filtering_behavior_reason,
      ApprovalRequestInitiatedCallback callback) override;
  bool IsMainFrame() const override;
  void CleanUpInfoBarOnMainFrame() override;
  int64_t GetInterstitialNavigationId() const override;
  void GoBack() override;
  void MaybeCloseLocalApproval() override;

 private:
  friend class ParentAccessTabHelperTest;
  friend class ParentAccessMediatorTest;

  // Processes the outcome of the local approval request.
  void OnLocalApprovalRequestCompleted(
      const GURL& url,
      base::TimeTicks start_time,
      supervised_user::LocalApprovalResult approval_result,
      std::optional<supervised_user::LocalWebApprovalErrorType> error_type);
  // Closes the tab linked to the web_state_.
  void Close();

  const bool is_main_frame_;
  bool is_bottomsheet_shown_ = false;
  raw_ptr<web::WebState> web_state_;
  __weak id<ParentAccessCommands> commands_handler_;
  base::WeakPtrFactory<IOSWebContentHandlerImpl> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_
