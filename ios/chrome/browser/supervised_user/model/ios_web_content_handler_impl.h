// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/supervised_user/core/browser/web_content_handler.h"
#import "url/gurl.h"

namespace web {
class WebState;
}

namespace supervised_user {
class UrlFormatter;
}  // namespace supervised_user

// iOS-specific implementation of the web content handler.
class IOSWebContentHandlerImpl : public supervised_user::WebContentHandler {
 public:
  IOSWebContentHandlerImpl(web::WebState* web_state, bool is_main_frame);

  IOSWebContentHandlerImpl(const IOSWebContentHandlerImpl&) = delete;
  IOSWebContentHandlerImpl& operator=(const IOSWebContentHandlerImpl&) = delete;
  ~IOSWebContentHandlerImpl() override;

  // supervised_user::WebContentHandler implementation:
  void RequestLocalApproval(const GURL& url,
                            const std::u16string& child_display_name,
                            const supervised_user::UrlFormatter& url_formatter,
                            ApprovalRequestInitiatedCallback callback) override;
  bool IsMainFrame() const override;
  void CleanUpInfoBarOnMainFrame() override;
  int64_t GetInterstitialNavigationId() const override;
  void GoBack() override;

 private:
  // Closes the tab linked to the web_state_.
  void Close();

  const bool is_main_frame_;
  raw_ptr<web::WebState> web_state_;
  base::WeakPtrFactory<IOSWebContentHandlerImpl> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_IOS_WEB_CONTENT_HANDLER_IMPL_H_
