// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

namespace internal {

CONTENT_EXPORT base::SupportsUserData::Data* GetDocumentUserData(
    const RenderFrameHost* rfh,
    const void* key);

CONTENT_EXPORT void SetDocumentUserData(
    RenderFrameHost* rfh,
    const void* key,
    std::unique_ptr<base::SupportsUserData::Data> data);

CONTENT_EXPORT void RemoveDocumentUserData(RenderFrameHost* rfh,
                                           const void* key);

}  // namespace internal

// This class approximates the lifetime of a single blink::Document in the
// browser process. At the moment RenderFrameHost can correspond to multiple
// blink::Documents (when RenderFrameHost is reused for same-process
// navigation). DocumentUserData is created when a user of an API
// inherits this class and calls CreateForCurrentDocument.
//
// DocumentUserData is cleared when either:
// - RenderFrameHost is deleted, or
// - A cross-document non-bfcached navigation is committed in the same
// RenderFrameHost i.e., DocumentUserData persists when a document is
// put in the BackForwardCache. It will still be present when the user navigates
// back to the document.
//
// DocumentUserData is assumed to be associated with the document in
// the RenderFrameHost. It can be associated even before RenderFrameHost commits
// i.e., on speculative RFHs and gets destroyed along with speculative RFHs if
// it ends up never committing.
//
// In case of crashes, DocumentUserData's lifetime doesn't match blink::Document
// lifetime. DocumentUserData is not cleared when the RenderFrame is deleted but
// is cleared on a subsequent navigation after a crash. This is done to ensure
// that the data associated with RenderFrameHost is not reset when the non-live
// RenderFrameHost is still in use by browser features like permissions or
// settings, which continue to work on the crashed pages. For more details,
// please refer to crbug.com/1099237.
//
// Note: RenderFrameHost is being replaced with RenderDocumentHost
// [https://crbug.com/936696]. After this is completed, every
// DocumentUserData object will be 1:1 with RenderFrameHost. Also
// RenderFrameHost/RenderDocument would start inheriting directly from
// SupportsUserData then we wouldn't need the use of
// GetDocumentUserData/SetDocumentUserData anymore.
//
// This is similar to WebContentsUserData but attached to a document instead.
// Example usage of DocumentUserData:
//
// --- in foo_document_helper.h ---
// class FooDocumentHelper
//     : public content::DocumentUserData<FooDocumentHelper> {
//  public:
//   ~FooDocumentHelper() override;
//
//   // ... more public stuff here ...
//
//  private:
//   // No public constructors to force going through static methods of
//   // DocumentUserData (e.g. CreateForCurrentDocument).
//   explicit FooDocumentHelper(content::RenderFrameHost* rfh);
//
//   friend DocumentUserData;
//   DOCUMENT_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_document_helper.cc ---
// DOCUMENT_USER_DATA_KEY_IMPL(FooDocumentHelper);
//
// FooDocumentHelper::FooDocumentHelper(content::RenderFrameHost* rfh)
//     : DocumentUserData(rfh) {}
//
// FooDocumentHelper::~FooDocumentHelper() {}
//
template <typename T>
class DocumentUserData : public base::SupportsUserData::Data {
 public:
  template <typename... Args>
  static void CreateForCurrentDocument(RenderFrameHost* rfh, Args&&... args) {
    DCHECK(rfh);
    if (!GetForCurrentDocument(rfh)) {
      T* data = new T(rfh, std::forward<Args>(args)...);
      internal::SetDocumentUserData(rfh, UserDataKey(), base::WrapUnique(data));
    }
  }

  static T* GetForCurrentDocument(RenderFrameHost* rfh) {
    DCHECK(rfh);
    return static_cast<T*>(internal::GetDocumentUserData(rfh, UserDataKey()));
  }

  static const T* GetForCurrentDocument(const RenderFrameHost* rfh) {
    DCHECK(rfh);
    return static_cast<const T*>(
        internal::GetDocumentUserData(rfh, UserDataKey()));
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
    internal::RemoveDocumentUserData(rfh, UserDataKey());
  }

  // Returns the RenderFrameHost associated with `this` object of a subclass
  // which inherits from DocumentUserData.
  //
  // The returned `render_frame_host()` is guaranteed to live as long as `this`
  // DocumentUserData (due to how UserData works - RenderFrameHost
  // owns `this` UserData).  Note that only the lifetime of
  // `render_frame_host()` is guaranteed, but not its state - e.g. the frame may
  // be `!IsActive()`.
  RenderFrameHost& render_frame_host() const { return *render_frame_host_; }

 protected:
  // TODO(crbug.com/40198594): Take a reference instead of a pointer
  // (here + transitively/as-far-as-reasonably-possible in callers).
  explicit DocumentUserData(RenderFrameHost* rfh) : render_frame_host_(rfh) {
    CHECK(rfh);
  }

  // Returns the origin of the associated document.
  //
  // Note that a DocumentUserData can be attached to a speculative
  // RenderFrameHost, but while the RenderFrameHost remains speculative/pending
  // commit, `origin()` returns a meaningless unique opaque origin. Only after
  // the commit will the returned value become meaningful.
  const url::Origin& origin() const {
    // `this` is promptly deleted if `render_frame_host_` commits a
    // cross-document navigation, so it is always safe to simply call
    // `GetLastCommittedOrigin()` directly, even without RenderDocument.
    return render_frame_host().GetLastCommittedOrigin();
  }

 private:
  static const void* UserDataKey() { return &T::kUserDataKey; }

  // This is a pointer (rather than a reference) to ensure that go/miracleptr
  // can cover this field (see also //base/memory/raw_ptr.md).
  const raw_ptr<RenderFrameHost> render_frame_host_ = nullptr;
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// DocumentUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class on/from a WebState.
#define DOCUMENT_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define DOCUMENT_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_USER_DATA_H_
