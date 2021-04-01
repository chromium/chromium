// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_DOCUMENT_HOST_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_DOCUMENT_HOST_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

CONTENT_EXPORT base::SupportsUserData::Data* GetRenderDocumentHostUserData(
    const RenderFrameHost* rfh,
    const void* key);

CONTENT_EXPORT void SetRenderDocumentHostUserData(
    RenderFrameHost* rfh,
    const void* key,
    std::unique_ptr<base::SupportsUserData::Data> data);

CONTENT_EXPORT void RemoveRenderDocumentHostUserData(RenderFrameHost* rfh,
                                                     const void* key);

// This class approximates the lifetime of a single blink::Document in the
// browser process. At the moment RenderFrameHost can correspond to multiple
// blink::Documents (when RenderFrameHost is reused for same-process
// navigation). RenderDocumentHostUserData is created when a user of an API
// inherits this class and calls CreateForCurrentDocument.
//
// RenderDocumentHostUserData is cleared when either:
// - RenderFrameHost is deleted, or
// - A cross-document non-bfcached navigation is committed in the same
// RenderFrameHost i.e., RenderDocumentHostUserData persists when a document is
// put in the BackForwardCache. It will still be present when the user navigates
// back to the document.
//
// RenderDocumentHostUserData is assumed to be associated with the document in
// the RenderFrameHost. It can be associated even before RenderFrameHost commits
// i.e., on speculative RFHs and gets destroyed along with speculative RFHs if
// it ends up never committing.
//
// Note: RenderFrameHost is being replaced with RenderDocumentHost
// [https://crbug.com/936696]. After this is completed, every
// RenderDocumentHostUserData object will be 1:1 with RenderFrameHost. Also
// RenderFrameHost/RenderDocument would start inheriting directly from
// SupportsUserData then we wouldn't need the use of
// GetRenderDocumentHostUserData/SetRenderDocumentHostUserData anymore.
//
// This is similar to WebContentsUserData but attached to a document instead.
// Example usage of RenderDocumentHostUserData:
//
// --- in foo_document_helper.h ---
// class FooDocumentHelper : public
// content::RenderDocumentHostUserData<FooDocumentHelper> {
//  public:
//   ~FooDocumentHelper() override;
//   // ... more public stuff here ...
//  private:
//   explicit FooDocumentHelper(content::RenderFrameHost* rfh);
//   friend class content::RenderDocumentHostUserData<FooDocumentHelper>;
//   RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
//   // ... more private stuff here ...
// };
//
// --- in foo_document_helper.cc ---
// RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(FooDocumentHelper)

template <typename T>
class RenderDocumentHostUserData : public base::SupportsUserData::Data {
 public:
  static void CreateForCurrentDocument(RenderFrameHost* rfh) {
    DCHECK(rfh);
    if (!GetForCurrentDocument(rfh)) {
      T* data = new T(rfh);
      SetRenderDocumentHostUserData(rfh, UserDataKey(), base::WrapUnique(data));
    }
  }

  static T* GetForCurrentDocument(RenderFrameHost* rfh) {
    DCHECK(rfh);
    return static_cast<T*>(GetRenderDocumentHostUserData(rfh, UserDataKey()));
  }

  static T* GetOrCreateForCurrentDocument(RenderFrameHost* rfh) {
    DCHECK(rfh);
    if (auto* data = GetForCurrentDocument(rfh)) {
      return data;
    }

    CreateForCurrentDocument(rfh);
    return GetForCurrentDocument(rfh);
  }

  static void DeleteForCurrentDocument(RenderFrameHost* rfh) {
    DCHECK(rfh);
    DCHECK(GetForCurrentDocument(rfh));
    RemoveRenderDocumentHostUserData(rfh, UserDataKey());
  }

  static const void* UserDataKey() { return &T::kUserDataKey; }
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// RenderDocumentHostUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class on/from a WebState.
#define RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL() \
  static constexpr int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(Type) \
  const int Type::kUserDataKey;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_DOCUMENT_HOST_USER_DATA_H_
