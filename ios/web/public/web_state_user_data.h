// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_
#define IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#import "ios/web/public/web_state.h"

// This macro declares a static variable inside the class that inherits from
// WebStateUserData. The address of this static variable is used as the key to
// store/retrieve an instance of the class on/from a WebState.
#define WEB_STATE_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .mm/.cc file to ensure that there is only one instantiation
// of the static variable.
#define WEB_STATE_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;

namespace web {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebState. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public web::WebStateUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(web::WebState* web_state);
//   friend class web::WebStateUserData<FooTabHelper>;
//   WEB_STATE_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// WEB_STATE_USER_DATA_KEY_IMPL(FooTabHelper)
template <typename T>
class WebStateUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebState.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForWebState(WebState* web_state, Args&&... args) {
    CHECK(web_state, base::NotFatalUntil::M131);
    CHECK(!web_state->IsBeingDestroyed(), base::NotFatalUntil::M131);
    if (!FromWebState(web_state)) {
      web_state->SetUserData(
          UserDataKey(),
          base::WrapUnique(new T(web_state, std::forward<Args>(args)...)));
    }
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebState (via CreateForWebState above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromWebState(WebState* web_state) {
    return static_cast<T*>(web_state->GetUserData(UserDataKey()));
  }
  static const T* FromWebState(const WebState* web_state) {
    return static_cast<const T*>(web_state->GetUserData(UserDataKey()));
  }

  // Removes the instance attached to the specified WebState.
  static void RemoveFromWebState(WebState* web_state) {
    web_state->RemoveUserData(UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_
