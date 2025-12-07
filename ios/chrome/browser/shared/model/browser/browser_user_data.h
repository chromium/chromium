// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "ios/chrome/browser/shared/model/browser/browser.h"

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
//   // ... more private stuff here ...
// };
template <typename T>
class BrowserUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified Browser.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForBrowser(Browser* browser, Args&&... args) {
    DCHECK(browser);
    if (!FromBrowser(browser)) {
      browser->SetUserData(UserDataKey(),
                           T::Create(browser, std::forward<Args>(args)...));
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

  // The key under which to store the user data.
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }

 protected:
  explicit BrowserUserData(Browser* browser) : browser_(browser) {
    CHECK(browser_);
  }

  // The owning Browser.
  const raw_ptr<Browser> browser_;

 private:
  // Default factory for T that invoke T's constructor. Can be overloaded
  // by sub-class if they want to create a sub-class of T instead.
  template <typename... Args>
  static std::unique_ptr<T> Create(Browser* browser, Args&&... args) {
    return base::WrapUnique(new T(browser, std::forward<Args>(args)...));
  }
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_USER_DATA_H_
