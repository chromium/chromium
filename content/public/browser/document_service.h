// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/document_service_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

namespace content {

enum class DocumentServiceDestructionReason : int {
  // The mojo connection terminated.
  kConnectionTerminated,
  // The document pointed to by `render_frame_host()` is being destroyed.
  kEndOfDocumentLifetime,
};

// Provides a safe alternative to mojo::MakeSelfOwnedReceiver<T>(...) for
// document-scoped Mojo interface implementations. Use of this helper prevents
// logic bugs when Mojo IPCs for `Interface` race against Mojo IPCs for
// navigation. One example of a past bug caused by this IPC race is
// https://crbug.com/769189, where an interface implementation performed a
// permission check using the wrong origin.
//
// Like C++ implementations owned by mojo::MakeSelfOwnedReceiver<T>(...), a
// subclass of DocumentService<Interface> will delete itself when the
// corresponding message pipe is disconnected by setting a disconnect handler on
// the mojo::Receiver<T>.
//
// In addition, a subclass of DocumentService<Interface> will also track
// the lifetime of the current document of the supplied RenderFrameHost and
// delete itself:
//
// - if the RenderFrameHost is deleted (for example, the <iframe> element the
//   RenderFrameHost represents is removed from the DOM) or
// - if the RenderFrameHost commits a cross-document navigation. Specifically,
//   DocumentService instances (and DocumentUserData instances)
//   are deleted with the same timing, before the last committed origin and
//   URL have been updated.
//
// When to use:
// Any Mojo interface implementation that references a RenderFrameHost, whether
// directly via a RenderFrameHost pointer, or indirectly, via the
// RenderFrameHost routing ID, should strongly consider:
//
// - `DocumentService` when there may be multiple instances per
//   RenderFrameHost.
// - `DocumentUserData` when there should only be a single instance
//   per RenderFrameHost.
//
// There are very few circumstances where a Mojo interface needs to be reused
// after a cross-document navigation.
template <typename Interface>
class DocumentService : public Interface, public internal::DocumentServiceBase {
 public:
  DocumentService(RenderFrameHost* render_frame_host,
                  mojo::PendingReceiver<Interface> pending_receiver)
      : DocumentServiceBase(render_frame_host),
        receiver_(this, std::move(pending_receiver)) {
    // |this| owns |receiver_|, so unretained is safe.
    receiver_.set_disconnect_handler(base::BindOnce(
        [](DocumentServiceBase* document_service) {
          document_service->WillBeDestroyed(
              DocumentServiceDestructionReason::kConnectionTerminated);
          delete document_service;
        },
        base::Unretained(this)));
  }

  ~DocumentService() override = default;

 protected:
  // `this` is promptly deleted if `render_frame_host_` commits a cross-document
  // navigation, so it is always safe to simply call `GetLastCommittedOrigin()`
  // and `GetLastCommittedURL()` directly.
  const url::Origin& origin() const {
    return render_frame_host()->GetLastCommittedOrigin();
  }

  mojo::Receiver<Interface>* receiver() { return &receiver_; }
  const mojo::Receiver<Interface>* receiver() const { return &receiver_; }

  // Returns the RenderFrameHost tracked by this object. Guaranteed to never be
  // null.
  using DocumentServiceBase::render_frame_host;

  // Subclasses can use this to check thread safety.
  // For example: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  THREAD_CHECKER(thread_checker_);

 private:
  mojo::Receiver<Interface> receiver_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_
