// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_

#import <string>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ProfileManagerObserverIOS;

// TODO(crbug.com/359492423): Remove this forward declaration and typedef when
// no usage of BrowserStateInfoCache remains.
class ProfileAttributesStorageIOS;
using BrowserStateInfoCache = ProfileAttributesStorageIOS;

// TODO(crbug.com/358356195): Remove this forward declaration and typedef when
// no usage of ChromeBrowserStateManager remains.
class ProfileManagerIOS;
using ChromeBrowserStateManager = ProfileManagerIOS;

// Provides methods that allow for various ways of creating non-incognito
// ChromeBrowserState instances. Owns all instances that it creates.
class ProfileManagerIOS {
 public:
  // Callback invoked when a ChromeBrowserState has been loaded asynchronously.
  using ChromeBrowserStateLoadedCallback =
      base::OnceCallback<void(ChromeBrowserState*)>;

  ProfileManagerIOS(const ProfileManagerIOS&) = delete;
  ProfileManagerIOS& operator=(const ProfileManagerIOS&) = delete;

  virtual ~ProfileManagerIOS() {}

  // Registers/unregisters observers.
  virtual void AddObserver(ProfileManagerObserverIOS* observer) = 0;
  virtual void RemoveObserver(ProfileManagerObserverIOS* observer) = 0;

  // Loads the last active browser states. *Deprecated*.
  virtual void LoadBrowserStates() = 0;

  // Returns the ChromeBrowserState that was last used. Only use this method for
  // the very specific purpose of finding which of the several available browser
  // states was used last. Do *not* use it as a singleton getter to fetch "the"
  // browser state. Always assume there could be multiple browser states and
  // use GetLoadedBrowserStates() instead.
  virtual ChromeBrowserState* GetLastUsedBrowserStateDeprecatedDoNotUse() = 0;

  // Returns the ChromeBrowserState known by `name` or nullptr if there is
  // no loaded ChromeBrowserState with that `name`.
  virtual ChromeBrowserState* GetBrowserStateByName(std::string_view name) = 0;

  // Returns the list of loaded ChromeBrowserStates. The order is arbitrary.
  virtual std::vector<ChromeBrowserState*> GetLoadedBrowserStates() = 0;

  // Asynchronously loads a ChromeBrowserState known by `name` if it exists. The
  // `created_callback` will be called with the ChromeBrowserState when it has
  // been created (but not yet initialised) and `initialised_callback` will be
  // called once the ChromeBrowserState is fully initialised. Returns true if
  // the ChromeBrowserState exists, false otherwise.
  //
  // In case of failure, `initialized_callback` is invoked with nullptr. The
  // `created_callback` will only be called if the ChromeBrowserState is
  // created, and thus will never receive nullptr but may never be called if
  // the creation is disallowed.
  virtual bool LoadBrowserStateAsync(
      std::string_view name,
      ChromeBrowserStateLoadedCallback initialized_callback,
      ChromeBrowserStateLoadedCallback created_callback = {}) = 0;

  // Asynchronously creates or loads a ChromeBrowserState known by `name`. The
  // `create_callback` will be called with the ChromeBrowserState when it has
  // been created (but not yet initialised) and `initialised_callback` will be
  // called once the ChromeBrowserState is fully initialised. Returns true if
  // the ChromeBrowserState exists or can be created, false otherwise.
  //
  // In case of failure, `initialized_callback` is invoked with nullptr. The
  // `created_callback` will only be called if the ChromeBrowserState is
  // created, and thus will never receive nullptr but may never be called if
  // the creation is disallowed.
  virtual bool CreateBrowserStateAsync(
      std::string_view name,
      ChromeBrowserStateLoadedCallback initialized_callback,
      ChromeBrowserStateLoadedCallback created_callback = {}) = 0;

  // Loads the ChromeBrowserState known by `name` and returns it. As this
  // method is synchronous, it may block the application so it should only be
  // used during the initialisation when blocking is possible or for tests.
  // Returns null if loading the ChromeBrowserState failed.
  virtual ChromeBrowserState* LoadBrowserState(std::string_view name) = 0;

  // Creates or loads the ChromeBrowserState known by `name` and returns it.
  // As this method is synchronous, it may block the application so it should
  // only be used during the initialisation when blocking is possible or for
  // tests. Returns null if loading or creating the ChromeBrowserState failed.
  virtual ChromeBrowserState* CreateBrowserState(std::string_view name) = 0;

  // Returns the BrowserStateInfoCache associated with this manager.
  virtual BrowserStateInfoCache* GetBrowserStateInfoCache() = 0;

 protected:
  ProfileManagerIOS() {}
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_MANAGER_IOS_H_
