// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_USER_DATA_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_USER_DATA_H_

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#import "ios/web/public/web_state/web_state.h"

namespace web {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebState. For example:
//
// --- in foo.h ---
// class Foo : public web::WebStateUserData<Foo> {
//  public:
//   ~Foo() override;
//   // ... more public stuff here ...
//  private:
//   explicit Foo(web::WebState* web_state);
//   friend class web::WebStateUserData<Foo>;
//   // ... more private stuff here ...
// }
//
template <typename T>
class WebStateUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebState.
  // If an instance is already attached, does nothing.
  static void CreateForWebState(WebState* web_state) {
    DCHECK(web_state);
    if (!FromWebState(web_state))
      web_state->SetUserData(UserDataKey(), base::WrapUnique(new T(web_state)));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebState (via CreateForWebState above) and returns it. If no instance
  // of the type was attached, returns null.
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

 protected:
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }
};

// Macro previously used to define the UserDataKey().
// TODO(crbug.com/589840): Remove this once all use have been deleted.
#define DEFINE_WEB_STATE_USER_DATA_KEY(TYPE)

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_USER_DATA_H_
