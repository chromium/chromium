// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_
#define IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "ios/web/common/features.h"
#include "ios/web/public/web_state.h"

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
//   // ... more private stuff here ...
// };
//
template <typename T>
class WebStateUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebState.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForWebState(WebState* web_state, Args&&... args) {
    CHECK(web_state);
    CHECK(!web_state->IsBeingDestroyed());

    // Fail if a tab helper is created for an unrealized WebState and
    // the feature kCreateTabHelperOnlyForRealizedWebStates is enabled.
    // If this CHECK(...) fails, the issue is in the code creating the
    // tab helper, not in the WebStateUserData<T> implementation (i.e.
    // look at the caller of this method to determine who should debug
    // this crash).
    if (web::features::CreateTabHelperOnlyForRealizedWebStates()) {
      CHECK(web_state->IsRealized(), base::NotFatalUntil::M160);
    }

    if (!FromWebState(web_state)) {
      web_state->SetUserData(UserDataKey(),
                             T::Create(web_state, std::forward<Args>(args)...));
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

  // The key under which to store the user data.
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }

 private:
  // Default factory for T that invoke T's constructor. Can be overloaded
  // by sub-class if they want to create a sub-class of T instead.
  template <typename... Args>
  static std::unique_ptr<T> Create(WebState* web_state, Args&&... args) {
    return base::WrapUnique(new T(web_state, std::forward<Args>(args)...));
  }
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_USER_DATA_H_
