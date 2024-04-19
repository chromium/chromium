// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/supports_user_data.h"
#include "content/public/browser/page.h"

namespace content {

// A base class for classes attached to, and scoped to, the lifetime of a
// content::Page.

// PageUserData is created when a user of an API inherits this class and calls
// CreateForPage.
//
// PageUserData is similar to DocumentUserData, but is attached to the
// page (1:1 with main document) instead of any document. Prefer using
// PageUserData for main-document-only data.
//
// Example usage of PageUserData:
//
// --- in foo_page_helper.h ---
// class FooPageHelper : public content::PageUserData<FooPageHelper> {
//  public:
//   ~FooPageHelper() override;
//
//   // ... more public stuff here ...
//
//  private:
//   explicit FooPageHelper(content::Page& page);
//
//   friend PageUserData;
//   PAGE_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_page_helper.cc ---
// PAGE_USER_DATA_KEY_IMPL(FooPageHelper);
//
// FooPageHelper::FooPageHelper(content::Page& page)
//     : PageUserData(page) {}
//
// FooPageHelper::~FooPageHelper() {}
//
template <typename T>
class PageUserData : public base::SupportsUserData::Data {
 public:
  template <typename... Args>
  static void CreateForPage(Page& page, Args&&... args) {
    if (!GetForPage(page)) {
      T* data = new T(page, std::forward<Args>(args)...);
      page.SetUserData(UserDataKey(), base::WrapUnique(data));
    }
  }

  // TODO(crbug.com/40847334): Due to a unresolved bug, this can return nullptr
  // even after CreateForPage() or GetOrCreateForPage() has been called.
  static T* GetForPage(Page& page) {
    return static_cast<T*>(page.GetUserData(UserDataKey()));
  }

  static T* GetOrCreateForPage(Page& page) {
    if (auto* data = GetForPage(page)) {
      return data;
    }

    CreateForPage(page);
    return GetForPage(page);
  }

  static void DeleteForPage(Page& page) {
    DCHECK(GetForPage(page));
    page.RemoveUserData(UserDataKey());
  }

  Page& page() const { return *page_; }

  static const void* UserDataKey() { return &T::kUserDataKey; }

 protected:
  explicit PageUserData(Page& page) : page_(page) {}

 private:
  // Page associated with subclass which inherits this PageUserData.
  const raw_ref<Page> page_;
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// PageUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class.
#define PAGE_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define PAGE_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_USER_DATA_H_
