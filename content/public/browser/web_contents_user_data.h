// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

namespace content {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebContents. For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public content::WebContentsUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooTabHelper(content::WebContents* contents);
//   friend class content::WebContentsUserData<FooTabHelper>;
//   WEB_CONTENTS_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// WEB_CONTENTS_USER_DATA_KEY_IMPL(FooTabHelper)
template <typename T>
class WebContentsUserData : public base::SupportsUserData::Data {
 public:
  // Creates an object of type T, and attaches it to the specified WebContents.
  // If an instance is already attached, does nothing.
  static void CreateForWebContents(WebContents* contents) {
    DCHECK(contents);
    if (!FromWebContents(contents))
      contents->SetUserData(UserDataKey(), base::WrapUnique(new T(contents)));
  }

  // Retrieves the instance of type T that was attached to the specified
  // WebContents (via CreateForWebContents above) and returns it. If no instance
  // of the type was attached, returns nullptr.
  static T* FromWebContents(WebContents* contents) {
    DCHECK(contents);
    return static_cast<T*>(contents->GetUserData(UserDataKey()));
  }
  static const T* FromWebContents(const WebContents* contents) {
    DCHECK(contents);
    return static_cast<const T*>(contents->GetUserData(UserDataKey()));
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

// This macro declares a static variable inside the class that inherits from
// WebContentsUserData The address of this static variable is used as the key to
// store/retrieve an instance of the class on/from a WebState.
#define WEB_CONTENTS_USER_DATA_KEY_DECL() static constexpr int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define WEB_CONTENTS_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey;

// We tried using the address of a static local variable in UserDataKey() as a
// key instead of the address of a member variable. That solution allowed us to
// get rid of the macros above. Unfortately, each dynamic library that accessed
// UserDataKey() had its own instantiation of the method, resulting in different
// keys for the same WebContentsUserData type. Because of that, the solution was
// reverted. https://crbug.com/589840#c16

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
