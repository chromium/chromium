// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_IMPL_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#include "ios/chrome/browser/shared/model/browser/browser.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class ChromeBrowserState;
@class SceneState;

// BrowserImpl is the concrete implementation of the Browser interface.
class BrowserImpl final : public Browser, public BrowserWebStateListDelegate {
 public:
  // Constructs an instance attached to `browser_state`, `scene_state`. If
  // `active_browser` is not null, then the Browser is an inactive Browser
  // and is considered to be attached to it. The `insertion_policy` and
  // `activation_policy` are passed to BrowserWebStateListDelegate constructor.
  BrowserImpl(ChromeBrowserState* browser_state,
              SceneState* scene_state,
              BrowserImpl* active_browser,
              InsertionPolicy insertion_policy,
              ActivationPolicy activation_policy);

  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;

  ~BrowserImpl() final;

  // Browser.
  ChromeBrowserState* GetBrowserState() final;
  WebStateList* GetWebStateList() final;
  CommandDispatcher* GetCommandDispatcher() final;
  SceneState* GetSceneState() final;
  void AddObserver(BrowserObserver* observer) final;
  void RemoveObserver(BrowserObserver* observer) final;
  base::WeakPtr<Browser> AsWeakPtr() final;
  bool IsInactive() const final;
  Browser* GetActiveBrowser() final;
  Browser* GetInactiveBrowser() final;
  Browser* CreateInactiveBrowser() final;
  void DestroyInactiveBrowser() final;

 private:
  // The ChromeBrowserState this Browser is attached to. Must not be null.
  raw_ptr<ChromeBrowserState> const browser_state_;

  // The owned WebStateList.
  WebStateList web_state_list_;

  // The CommandDispatcher and the SceneState.
  // Will both be nil for a temporary Browser.
  __weak SceneState* scene_state_;
  __strong CommandDispatcher* command_dispatcher_;

  // Used to maintain the relationship between the regular and the inactive
  // Browser. For a regular Browser, `active_browser_` will be set to `this`.
  raw_ptr<Browser> const active_browser_;
  std::unique_ptr<Browser> inactive_browser_;

  // The list of observers.
  base::ObserverList<BrowserObserver, /*check_empty=*/true> observers_;

  // Needs to be the last member field to ensure all weak pointers are
  // invalidated before the other internal objects are destroyed.
  base::WeakPtrFactory<Browser> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_IMPL_H_
