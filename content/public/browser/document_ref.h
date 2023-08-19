// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_REF_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_REF_H_

#include "base/memory/safe_ref.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// A non-nullable, checked reference to a document. This will CHECK if it is
// accessed after the document is no longer valid, because the RenderFrameHost
// is deleted or navigated to a different document. See also
// document_user_data.h.
//
// Note that though this is implemented as a base::SafeRef<RenderFrameHost>,
// it is different from an ordinary SafeRef to the RenderFrameHost.
//
// docs/render_document.md will make these equivalent in the future.
//
// If the document may become invalid, use a WeakDocumentPtr instead.
//
// Treat this like you would a base::SafeRef, because that's essentially what it
// is.
class CONTENT_EXPORT DocumentRef {
 public:
  // Copyable and movable.
  DocumentRef(DocumentRef&&);
  DocumentRef& operator=(DocumentRef&&);
  DocumentRef(const DocumentRef&);
  DocumentRef& operator=(const DocumentRef&);

  ~DocumentRef();

  RenderFrameHost& AsRenderFrameHost() const { return *safe_document_; }

 private:
  explicit DocumentRef(base::SafeRef<RenderFrameHost> safe_document);

  friend class RenderFrameHostImpl;

  // Created from a factory scoped to document, rather than RenderFrameHost,
  // lifetime.
  base::SafeRef<RenderFrameHost> safe_document_;
};

// [chromium-style] requires these be out of line, but they are small enough to
// inline the defaults.
inline DocumentRef::DocumentRef(DocumentRef&&) = default;
inline DocumentRef& DocumentRef::operator=(DocumentRef&&) = default;
inline DocumentRef::DocumentRef(const DocumentRef&) = default;
inline DocumentRef& DocumentRef::operator=(const DocumentRef&) = default;
inline DocumentRef::~DocumentRef() = default;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_REF_H_
