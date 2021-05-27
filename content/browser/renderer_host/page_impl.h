// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_

#include <memory>

namespace content {

class RenderFrameHostImpl;

// Page is a main document together with all of its subframes.

// At the moment some navigations might create a new blink::Document in the
// existing RenderFrameHost, which will lead to a creation of a new Page
// associated with the same main RenderFrameHost. See the comment in
// |RenderDocumentHostUserData| for more details and crbug.com/936696 for the
// progress on always creating a new RenderFrameHost for each new document.

// Page is created when a main document created, which can happen in the
// following ways:
// 1) Main RenderFrameHost is created.
// 2) A cross-document non-bfcached navigation is committed in the same
//    RenderFrameHost.
// 3) Main RenderFrameHost is re-created after crash.

// Page is deleted in the following cases:
// 1) Main RenderFrameHost is deleted.
// 2) A cross-document non-bfcached navigation is committed in the same
//    RenderFrameHost.
// 3) Before main RenderFrameHost is re-created after crash.

// If a method can't be called on the non-main-frame RenderFrameHost or its
// behaviour is always identical when called on the parent / child
// RenderFrameHosts, it should be added to Page(Impl).

// NOTE: Depending on the process model, the cross-origin iframes are likely to
// be hosted in a different renderer process than the main document, so a given
// page is hosted in multiple renderer processes at the same time.
// RenderViewHost / RenderView / blink::Page (which are all 1:1:1) represent a
// part of a given content::Page in a given renderer process (note, however,
// that like RenderFrameHosts, these objects at the moment can be reused for a
// new content::Page for a cross-document same-origin main-frame navigation).
class PageImpl {
 public:
  explicit PageImpl(RenderFrameHostImpl& rfh);

  ~PageImpl();

  RenderFrameHostImpl* main_document() { return &main_document_; }

 private:
  // This class is owned by the main RenderFrameHostImpl and it's safe to keep a
  // reference to it.
  RenderFrameHostImpl& main_document_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
