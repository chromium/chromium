// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/main/browser.h"

class ChromeBrowserState;
@class SceneState;
class WebStateList;
class WebStateListDelegate;

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a BrowserList to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class BrowserImpl : public Browser {
 public:
  // Constructs a BrowserImpl attached to `browser_state`.
  BrowserImpl(ChromeBrowserState* browser_state);

  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;

  ~BrowserImpl() override;

  // Browser.
  ChromeBrowserState* GetBrowserState() override;
  WebStateList* GetWebStateList() override;
  CommandDispatcher* GetCommandDispatcher() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  ChromeBrowserState* browser_state_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
