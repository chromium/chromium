// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_H_

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

namespace content {

// Base class for mojo interface implementations tied to a document's lifetime.
// The service will be destroyed when any of the following happens:
// 1. mojo interface connection error happened,
// 2. the RenderFrameHost was deleted, or
// 3. navigation was committed on the RenderFrameHost (not same document).
//
// WARNING: To avoid race conditions, subclasses MUST only get the origin via
// origin() instead of from |render_frame_host| passed in the constructor.
// See https://crbug.com/769189 for an example of such a race.
template <typename Interface>
class DocumentServiceBase : public Interface, public WebContentsObserver {
 public:
  DocumentServiceBase(RenderFrameHost* render_frame_host,
                      mojo::PendingReceiver<Interface> pending_receiver)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host),
        origin_(render_frame_host_->GetLastCommittedOrigin()),
        receiver_(this, std::move(pending_receiver)) {
    // |this| owns |receiver_|, so unretained is safe.
    receiver_.set_disconnect_handler(
        base::BindOnce(&DocumentServiceBase::Close, base::Unretained(this)));
  }

 protected:
  // Make the destructor private since |this| can only be deleted by Close().
  virtual ~DocumentServiceBase() = default;

  // All subclasses should use this function to obtain the origin instead of
  // trying to get it from the RenderFrameHost pointer directly.
  const url::Origin& origin() const { return origin_; }

  // Returns the RenderFrameHost held by this object.
  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

  // Subclasses can use this to check thread safety.
  // For example: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  THREAD_CHECKER(thread_checker_);

 private:
  // Disallow calling web_contents() directly from the subclasses to ensure that
  // tab-level state doesn't get queried or updated when the RenderFrameHost is
  // not active.
  // Use WebContents::From(render_frame_host()) instead, but please keep in mind
  // that the render_frame_host() might not be active. See
  // RenderFrameHost::IsActive() for details.
  using WebContentsObserver::web_contents;

  // WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (render_frame_host == render_frame_host_) {
      DVLOG(1) << __func__ << ": RenderFrame destroyed.";
      Close();
    }
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument() ||
        navigation_handle->IsPageActivation()) {
      return;
    }

    if (navigation_handle->GetRenderFrameHost() == render_frame_host_) {
      // DocumentServiceBase is destroyed either when RenderFrameHost is
      // destroyed (covered by RenderFrameDeleted) or when a new document
      // commits in the same RenderFrameHost (covered by DidFinishNavigation).
      // Only committed non-same-document non-bfcache non-prerendering
      // activation navigations replace a document in existing RenderFrameHost.
      DVLOG(1) << __func__ << ": Close connection on navigation.";
      Close();
    }
  }

  // Stops observing WebContents and delete |this|.
  void Close() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << __func__;
    delete this;
  }

  RenderFrameHost* const render_frame_host_ = nullptr;
  const url::Origin origin_;
  mojo::Receiver<Interface> receiver_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_H_
