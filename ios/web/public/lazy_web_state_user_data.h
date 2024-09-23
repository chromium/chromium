// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_
#define IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/web/public/web_state.h"

// This macro declares a static variable inside the class that inherits from
// LazyWebStateUserData. The address of this static variable is used as the key
// to store/retrieve an instance of the class on/from a WebState.
#ifndef WEB_STATE_USER_DATA_KEY_DECL
#define WEB_STATE_USER_DATA_KEY_DECL() static const int kUserDataKey = 0
#endif

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .mm/.cc file to ensure that there is only one instantiation
// of the static variable.
#ifndef WEB_STATE_USER_DATA_KEY_IMPL
#define WEB_STATE_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;
#endif

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
//   WEB_STATE_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// WEB_STATE_USER_DATA_KEY_IMPL(FooTabHelper)
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
    CHECK(web_state, base::NotFatalUntil::M131);
    if (!FromWebState(web_state)) {
      CHECK(!web_state->IsBeingDestroyed(), base::NotFatalUntil::M131);
      web_state->SetUserData(
          UserDataKey(),
          base::WrapUnique(new T(web_state, std::forward<Args>(args)...)));
    }

    return FromWebState(web_state);
  }

  // Removes the instance attached to the specified WebState.
  static void RemoveFromWebState(WebState* web_state) {
    web_state->RemoveUserData(UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }

 private:
  // Retrieves the instance of type T that was attached to the specified
  // WebState (via GetOrCreateForWebState above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromWebState(WebState* web_state) {
    return static_cast<T*>(web_state->GetUserData(UserDataKey()));
  }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_LAZY_WEB_STATE_USER_DATA_H_
