// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

// This macro declares a static variable inside the class that inherits from
// BrwoserUserData. The address of this static variable is used as the key to
// store/retrieve an instance of the class on/from a Browser.
#define BROWSER_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .mm/.cc file to ensure that there is only one instantiation
// of the static variable.
#define BROWSER_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;

// A base class for classes attached to, and scoped to, the lifetime of a
// Browser. For example:
//
// --- in foo_browser_agent.h ---
// class FooBrowserAgent : public BrowserUserData<FooBrowserAgent> {
//  public:
//   ~FooBrowserAgent() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooBrowserAgent(Browser* browser);
//   friend class BrowserUserData<FooBrowserAgent>;
//   BROWSER_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_browser_agent.cc ---
// BROWSER_USER_DATA_KEY_IMPL(FooBrowserAgent)
template <typename T>
class BrowserUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified Browser.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForBrowser(Browser* browser, Args&&... args) {
    DCHECK(browser);
    if (!FromBrowser(browser)) {
      browser->SetUserData(
          UserDataKey(),
          base::WrapUnique(new T(browser, std::forward<Args>(args)...)));
    }
  }

  // Retrieves the instance of type T that was attached to the specified
  // Browser (via CreateForBrowser above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromBrowser(Browser* browser) {
    return static_cast<T*>(browser->GetUserData(UserDataKey()));
  }
  static const T* FromBrowser(const Browser* browser) {
    return static_cast<const T*>(browser->GetUserData(UserDataKey()));
  }

  // Removes the instance attached to the specified Browser.
  static void RemoveFromBrowser(Browser* browser) {
    browser->RemoveUserData(UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_
