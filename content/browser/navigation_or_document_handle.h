// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_OR_DOCUMENT_HANDLE_H_
#define CONTENT_BROWSER_NAVIGATION_OR_DOCUMENT_HANDLE_H_

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/weak_document_ptr.h"
#include "url/origin.h"

namespace content {

class FrameTreeNode;
class WebContents;
class NavigationRequest;
class RenderFrameHostImpl;

// This handle allows the user to attribute events to a navigation and a
// document, supporting a seamless transfer from a navigation to a committed
// document. Typically this is needed when processing events which are racing
// against the navigation (e.g. notifications from the network service).
class CONTENT_EXPORT NavigationOrDocumentHandle
    : public base::RefCounted<NavigationOrDocumentHandle> {
 public:
  static scoped_refptr<NavigationOrDocumentHandle> CreateForDocument(
      GlobalRenderFrameHostId render_frame_host_id);

  static scoped_refptr<NavigationOrDocumentHandle> CreateForNavigation(
      NavigationRequest& navigation_request);

  // Returns NavigationRequest associated with this instance.
  // One of GetNavigationRequest()/GetDocument() would be non-null
  // depending on the state of the navigation unless they are destroyed.
  NavigationRequest* GetNavigationRequest() const;

  // Returns RenderFrameHost associated with this instance.
  RenderFrameHost* GetDocument() const;

  WebContents* GetWebContents() const;

  FrameTreeNode* GetFrameTreeNode() const;

  // Returns the outermost top-frame origin, if available; otherwise
  // `std::nullopt`.
  std::optional<url::Origin> GetTopmostFrameOrigin() const;

  bool IsInPrimaryMainFrame() const;

  // Called when the navigation is committed. This is used to update
  // `render_frame_host_` before the navigation request is destroyed.
  void OnNavigationCommitted(NavigationRequest& navigation_request);

 private:
  friend class base::RefCounted<NavigationOrDocumentHandle>;

  explicit NavigationOrDocumentHandle(
      GlobalRenderFrameHostId render_frame_host_id);
  explicit NavigationOrDocumentHandle(NavigationRequest& navigation_request);
  ~NavigationOrDocumentHandle();

  // Is set when this is created for a navigation request. Can be null after the
  // navigation is committed.
  base::WeakPtr<NavigationRequest> navigation_request_;
  // Is set when this is created for a document or after a navigation is
  // committed.
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_OR_DOCUMENT_HANDLE_H_
