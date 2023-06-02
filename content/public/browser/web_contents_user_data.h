// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"

namespace content {

// A base class for classes attached to, and scoped to, the lifetime of a
// WebContents.
//
// When considering using this class, please carefully consider the intended
// lifetime of the data. There are other UserData classes which may more
// precisely match the intended lifetime. For example, DocumentUserData scopes
// the data to a document, NavigationHandleUserData to a navigation, etc. It is
// preferable to use a more specific UserData class, rather than storing
// non-WebContents state as a WebContentsUserData combined with using a
// WebContentsObserver to manually reset the state.
//
// For example:
//
// --- in foo_tab_helper.h ---
// class FooTabHelper : public content::WebContentsUserData<FooTabHelper> {
//  public:
//   ~FooTabHelper() override;
//
//   // ... more public stuff here ...
//
//  private:
//   explicit FooTabHelper(content::WebContents* contents);
//
//   friend WebContentsUserData;
//   WEB_CONTENTS_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_tab_helper.cc ---
// WEB_CONTENTS_USER_DATA_KEY_IMPL(FooTabHelper)
template <typename T>
class WebContentsUserData : public base::SupportsUserData::Data {
 public:
  explicit WebContentsUserData(WebContents& web_contents)
      : web_contents_(&web_contents) {}

  // Creates an object of type T, and attaches it to the specified WebContents.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForWebContents(WebContents* contents, Args&&... args) {
    DCHECK(contents);
    if (!FromWebContents(contents)) {
      contents->SetUserData(
          UserDataKey(),
          base::WrapUnique(new T(contents, std::forward<Args>(args)...)));
    }
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

  // Returns the WebContents associated with `this` object of a subclass
  // which inherits from WebContentsUserData.
  //
  // The returned `WebContents` is guaranteed to live as long as `this`
  // WebContentsUserData (due to how UserData works - WebContents
  // owns `this` UserData).
  content::WebContents& GetWebContents() { return *web_contents_; }
  const content::WebContents& GetWebContents() const { return *web_contents_; }

 private:
  // This is a pointer (rather than a reference) to ensure that go/miracleptr
  // can cover this field (see also //base/memory/raw_ptr.md).
  const raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ =
      nullptr;
};

// This macro declares a static variable inside the class that inherits from
// WebContentsUserData The address of this static variable is used as the key to
// store/retrieve an instance of the class on/from a WebState.
#define WEB_CONTENTS_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define WEB_CONTENTS_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

// We tried using the address of a static local variable in UserDataKey() as a
// key instead of the address of a member variable. That solution allowed us to
// get rid of the macros above. Unfortately, each dynamic library that accessed
// UserDataKey() had its own instantiation of the method, resulting in different
// keys for the same WebContentsUserData type. Because of that, the solution was
// reverted. https://crbug.com/589840#c16

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_USER_DATA_H_
