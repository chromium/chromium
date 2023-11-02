// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

// This class can be used to store data for an in-progress navigation.
// NavigationHandleUserData is created when a user of an API inherits this class
// and calls CreateForCurrentNavigation.
//
// NavigationHandleUserData is cleared when either:
// - NavigationHandle is deleted, or
// - DeleteForCurrentNavigation is called.
//
// This is similar to DocumentUserData but attached to a navigation
// instead. This class can be used before there's a document assigned for this
// navigation. Example usage of NavigationHandleUserData:
//
// --- in foo_data.h ---
// class FooData : public content::NavigationHandleUserData<FooData> {
//  public:
//   ~FooData() override;
//
//   // ... more public stuff here ...
//
//  private:
//   explicit FooData(content::NavigationHandle& navigation_handle);
//
//   friend NavigationHandleUserData;
//   NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_data.cc ---
// NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(FooData)

template <typename T>
class NavigationHandleUserData : public base::SupportsUserData::Data {
 public:
  template <typename... Args>
  static void CreateForNavigationHandle(NavigationHandle& navigation_handle,
                                        Args&&... args) {
    if (!GetForNavigationHandle(navigation_handle)) {
      navigation_handle.SetUserData(
          UserDataKey(), base::WrapUnique(new T(navigation_handle,
                                                std::forward<Args>(args)...)));
    }
  }

  static T* GetForNavigationHandle(NavigationHandle& navigation_handle) {
    return static_cast<T*>(navigation_handle.GetUserData(UserDataKey()));
  }

  static T* GetOrCreateForNavigationHandle(
      NavigationHandle& navigation_handle) {
    if (!GetForNavigationHandle(navigation_handle)) {
      CreateForNavigationHandle(navigation_handle);
    }
    return GetForNavigationHandle(navigation_handle);
  }

  static void DeleteForNavigationHandle(NavigationHandle& navigation_handle) {
    DCHECK(GetForNavigationHandle(navigation_handle));
    navigation_handle.RemoveUserData(UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// NavigationHandleUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class.
#define NAVIGATION_HANDLE_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
