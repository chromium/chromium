// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_
#define IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/web/public/web_state.h"

namespace web {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebState. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public web::LazyWebStateUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(web::WebState* web_state);
//   friend class web::LazyWebStateUserData<FooTabHelper>;
//   // ... more private stuff here ...
// };
//
template <typename T>
class LazyWebStateUserData : public base::SupportsUserData::Data {
 public:
  // If not created yet, creates an object of type T, and attaches it to the
  // specified WebState. Returns the existing or newly created instance.
  //
  // In order to properly defer objects that inherit this class, avoid using
  // this method for:
  // 1. Objects that expect observer methods to be called before the class is
  // otherwise used. Classes which are observers will only be registered once
  // the class is created. For this, please inherit from WebStateUserData.
  // 2. Callsites that are triggered when a WebState is added. Adding this
  // method to these callsites would be no different from attaching the object
  // to the WebState non-lazily.
  //
  // Note: Attaching an object to a WebState non-lazily wouldn't break anything,
  // but it may contribute to an increase in startup latency. Attaching an
  // object to an inserted WebState takes time to initialize. If the object is
  // unused, this initialization can be deferred to when the object is called
  // for the first time, for example, if the object isn't used until a user
  // action is triggered.
  template <typename... Args>
  static T* GetOrCreateForWebState(WebState* web_state, Args&&... args) {
    CHECK(web_state);
    if (!FromWebState(web_state)) {
      CHECK(!web_state->IsBeingDestroyed());
      web_state->SetUserData(UserDataKey(),
                             T::Create(web_state, std::forward<Args>(args)...));
    }

    return FromWebState(web_state);
  }

  // Removes the instance attached to the specified WebState.
  static void RemoveFromWebState(WebState* web_state) {
    web_state->RemoveUserData(UserDataKey());
  }

  // The key under which to store the user data.
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }

 private:
  // Retrieves the instance of type T that was attached to the specified
  // WebState (via GetOrCreateForWebState above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromWebState(WebState* web_state) {
    return static_cast<T*>(web_state->GetUserData(UserDataKey()));
  }

  // Default factory for T that invoke T's constructor. Can be overloaded
  // by sub-class if they want to create a sub-class of T instead.
  template <typename... Args>
  static std::unique_ptr<T> Create(WebState* web_state, Args&&... args) {
    return base::WrapUnique(new T(web_state, std::forward<Args>(args)...));
  }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_
