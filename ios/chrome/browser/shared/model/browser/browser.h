// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_H_

#import <memory>

#import "base/memory/weak_ptr.h"
#import "base/supports_user_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BrowserObserver;
@class CommandDispatcher;
@class SceneState;
class WebStateList;

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a Coordinator to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class Browser : public base::SupportsUserData {
 public:
  // Different type of browsers.
  enum class Type {
    kRegular,
    kIncognito,
    kInactive,
    kTemporary,
  };

  // Creates a new Browser attached to `profile` and to `scene_state`.
  static std::unique_ptr<Browser> Create(ProfileIOS* profile,
                                         SceneState* scene_state);

  // Creates a new temporary Browser attached to `profile`. It should
  // not be presented to the user but can be used when there is a need to
  // store WebStates while supporting BrowserAgent (i.e. recording metrics,
  // reporting recently closed tabs, ...).
  static std::unique_ptr<Browser> CreateTemporary(ProfileIOS* profile);

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  ~Browser() override {}

  // Returns the type of this browser.
  virtual Type type() const = 0;

  // Accessor for the owning Profile.
  virtual ProfileIOS* GetProfile() = 0;

  // Accessor for the WebStateList.
  virtual WebStateList* GetWebStateList() = 0;

  // Accessor for the CommandDispatcher.
  virtual CommandDispatcher* GetCommandDispatcher() = 0;

  // Accessor for the SceneState.
  virtual SceneState* GetSceneState() = 0;

  // Adds and removes observers.
  virtual void AddObserver(BrowserObserver* observer) = 0;
  virtual void RemoveObserver(BrowserObserver* observer) = 0;

  // Returns a weak pointer to the Browser.
  virtual base::WeakPtr<Browser> AsWeakPtr() = 0;

  // Returns true if this browser is the inactive one. This means this browser
  // contains only tabs that have not been opened for a defined amount of time.
  virtual bool IsInactive() const = 0;

  // Get the associated browser, if any. Returns itself if the active state
  // matches.
  virtual Browser* GetActiveBrowser() = 0;
  virtual Browser* GetInactiveBrowser() = 0;

  // Creates the associated inactive browser. `IsInactive` must be false, and
  // this function must be called at most once. This can only be called on a
  // regular browser.
  virtual Browser* CreateInactiveBrowser() = 0;

  // Destroys the associated inactive browser. `IsInactive` must be false.
  virtual void DestroyInactiveBrowser() = 0;

 protected:
  Browser() {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_H_
