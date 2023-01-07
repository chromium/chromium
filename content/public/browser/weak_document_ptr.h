// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEAK_DOCUMENT_PTR_H_
#define CONTENT_PUBLIC_BROWSER_WEAK_DOCUMENT_PTR_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Weakly refers to a document.
//
// This is invalidated at the same time as DocumentUserData. It
// becomes null whenever the RenderFrameHost is deleted or navigates to a
// different document. See also document_user_data.h.
//
// Note that though this is implemented as a base::WeakPtr<RenderFrameHost>,
// it is different from an ordinary weak pointer to the RenderFrameHost.
//
// docs/render_document.md will make these equivalent in the future.
//
// Treat this like you would a base::WeakPtr, because that's essentially what it
// is.
class WeakDocumentPtr {
 public:
  WeakDocumentPtr();

  // Copyable and movable.
  WeakDocumentPtr(WeakDocumentPtr&&);
  WeakDocumentPtr& operator=(WeakDocumentPtr&&);
  WeakDocumentPtr(const WeakDocumentPtr&);
  WeakDocumentPtr& operator=(const WeakDocumentPtr&);

  ~WeakDocumentPtr();

  // Callers must handle this returning null, in case the frame has been deleted
  // or a cross-document navigation has committed in the same RenderFrameHost.
  RenderFrameHost* AsRenderFrameHostIfValid() const {
    return weak_document_.get();
  }

 private:
  explicit WeakDocumentPtr(base::WeakPtr<RenderFrameHost> weak_rfh);

  friend class RenderFrameHostImpl;

  // Created from a factory scoped to document, rather than RenderFrameHost,
  // lifetime.
  base::WeakPtr<RenderFrameHost> weak_document_;
};

// [chromium-style] requires these be out of line, but they are small enough to
// inline the defaults.
inline WeakDocumentPtr::WeakDocumentPtr() = default;
inline WeakDocumentPtr::WeakDocumentPtr(WeakDocumentPtr&&) = default;
inline WeakDocumentPtr& WeakDocumentPtr::operator=(WeakDocumentPtr&&) = default;
inline WeakDocumentPtr::WeakDocumentPtr(const WeakDocumentPtr&) = default;
inline WeakDocumentPtr& WeakDocumentPtr::operator=(const WeakDocumentPtr&) =
    default;
inline WeakDocumentPtr::~WeakDocumentPtr() = default;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEAK_DOCUMENT_PTR_H_
