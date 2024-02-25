// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_MANAGER_IMPL_H_

#import "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// Associates a Tab to an InfoBarManager and manages its lifetime.
// It responds to navigation events.
class InfoBarManagerImpl : public infobars::InfoBarManager,
                           public web::WebStateObserver,
                           public web::WebStateUserData<InfoBarManagerImpl> {
 public:
  InfoBarManagerImpl(const InfoBarManagerImpl&) = delete;
  InfoBarManagerImpl& operator=(const InfoBarManagerImpl&) = delete;

  ~InfoBarManagerImpl() override;

  // Returns the `web_state_` tied to this InfobarManager.
  web::WebState* web_state() const { return web_state_; }

 private:
  friend class web::WebStateUserData<InfoBarManagerImpl>;

  explicit InfoBarManagerImpl(web::WebState* web_state);

  // InfoBarManager implementation.
  int GetActiveEntryID() override;

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Opens a URL according to the specified `disposition`.
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_MANAGER_IMPL_H_
