// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/main/browser.h"

class ChromeBrowserState;
@class SceneState;
class WebStateList;
class WebStateListDelegate;

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a BrowserList to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class BrowserImpl final : public Browser {
 public:
  // Constructs a BrowserImpl attached to `browser_state`.
  BrowserImpl(ChromeBrowserState* browser_state);

  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;

  ~BrowserImpl() final;

  // Browser.
  ChromeBrowserState* GetBrowserState() final;
  WebStateList* GetWebStateList() final;
  CommandDispatcher* GetCommandDispatcher() final;
  void AddObserver(BrowserObserver* observer) final;
  void RemoveObserver(BrowserObserver* observer) final;
  base::WeakPtr<Browser> AsWeakPtr() final;

 private:
  ChromeBrowserState* browser_state_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  // Needs to be the last member field to ensure all weak pointers are
  // invalidated before the other internal objects are destroyed.
  base::WeakPtrFactory<Browser> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
