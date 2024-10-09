// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_TEST_BROWSER_H_

#include <CoreFoundation/CoreFoundation.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/shared/model/browser/browser.h"

class WebStateListDelegate;

class TestBrowser final : public Browser {
 public:
  // Constructor that takes a WebStateListDelegate.
  TestBrowser(ProfileIOS* profile,
              SceneState* scene_state,
              std::unique_ptr<WebStateListDelegate> web_state_list_delegate,
              Type type = Type::kRegular);

  // Constructor that takes only a Profile and a SceneState; a fake
  // WebStateListDelegate will be used.
  TestBrowser(ProfileIOS* profile, SceneState* scene_state);

  // Constructor that takes a ProfileIOS and WebStateListDelegate;
  // SceneState will be nil.
  TestBrowser(ProfileIOS* profile,
              std::unique_ptr<WebStateListDelegate> web_state_list_delegate);

  // Constructor that takes only a Profile; a fake WebStateListDelegate
  // will be used. SceneState will be nil.
  explicit TestBrowser(ProfileIOS* profile);

  TestBrowser(const TestBrowser&) = delete;
  TestBrowser& operator=(const TestBrowser&) = delete;

  ~TestBrowser() final;

  // Browser.
  Type type() const override;
  ProfileIOS* GetProfile() final;
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
  const Type type_;
  raw_ptr<ProfileIOS> profile_ = nullptr;
  __weak SceneState* scene_state_ = nil;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_ = nil;
  std::unique_ptr<TestBrowser> inactive_browser_;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  // Needs to be the last member field to ensure all weak pointers are
  // invalidated before the other internal objects are destroyed.
  base::WeakPtrFactory<Browser> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_TEST_TEST_BROWSER_H_
