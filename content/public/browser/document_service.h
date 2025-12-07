// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_

#include <cstdint>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "content/public/browser/document_service_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

namespace content {

class DocumentAssociatedData;

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
  DocumentService(RenderFrameHost& render_frame_host,
                  mojo::PendingReceiver<Interface> pending_receiver)
      : DocumentServiceBase(render_frame_host),
        receiver_(this, std::move(pending_receiver)) {
    // This is a developer error; it does not make sense to bind a
    // DocumentService with a null PendingReceiver.
    DUMP_WILL_BE_CHECK(receiver_.is_bound());
    // |this| owns |receiver_|, so base::Unretained is safe.
    receiver_.set_disconnect_handler(base::BindOnce(
        [](DocumentService* document_service) {
          document_service->WillBeDestroyed(
              DocumentServiceDestructionReason::kConnectionTerminated);
          document_service->ResetAndDeleteThis();
        },
        base::Unretained(this)));
  }

  ~DocumentService() override {
    // To avoid potential destruction order issues, subclasses must use one of
    // the *AndDeleteThis() methods below instead of using `delete this`.
    DUMP_WILL_BE_CHECK(!receiver_.is_bound());
  }

  // Subclasses may end their lifetime early by calling this method; `delete
  // this` is not permitted for a `DocumentService` and will trigger the
  // `DCHECK` in the destructor above.
  //
  // If there is a specific reason for self-deletion, one of the following may
  // be more appropriate instead:
  //
  // - To report a failure when validating inputs received over IPC (e.g. the
  //   sender is malicious or buggy), use `ReportBadMessageAndDeleteThis()`.
  //
  // - Otherwise, to attach a specific numeric code to the `mojo::Receiver`
  //   reset, which will be passed to the other endpoint's disconnect with
  //   reason handler (if any), use `ResetWithReasonAndDeleteThis()`.
  //
  // The ordering of events is important: by resetting the mojo::Receiver before
  // invoking the destructor, any pending Mojo reply callbacks can simply be
  // dropped by an interface implementation, without forcing the implementation
  // to (pointlessly) first run those reply callbacks.
  void ResetAndDeleteThis() {
    InternalUnregister(base::PassKey<DocumentService>());
    receiver_.reset();
    delete this;
  }

  // Internal implementation helper:
  void ResetAndDeleteThisInternal(base::PassKey<DocumentAssociatedData>) final {
    receiver_.reset();
    delete this;
  }

 protected:
  // `this` is promptly deleted if `render_frame_host_` commits a cross-document
  // navigation, so it is always safe to simply call `GetLastCommittedOrigin()`
  // and `GetLastCommittedURL()` directly.
  const url::Origin& origin() const {
    return render_frame_host().GetLastCommittedOrigin();
  }

  // Reports a bad message and deletes `this`.
  //
  // Prefer over `mojo::ReportBadMessage()`, since using this method avoids the
  // need to run any pending reply callbacks with placeholder arguments.
  NOT_TAIL_CALLED void ReportBadMessageAndDeleteThis(std::string_view error) {
    InternalUnregister(base::PassKey<DocumentService>());
    receiver_.ReportBadMessage(error);
    delete this;
  }

  // Resets the `mojo::Receiver` with a `reason` and `description` and deletes
  // `this`.
  void ResetWithReasonAndDeleteThis(uint32_t reason,
                                    std::string_view description) {
    receiver_.ResetWithReason(reason, description);
    delete this;
  }

  // Returns a reference to the RenderFrameHost tracked by this object.
  using DocumentServiceBase::render_frame_host;

  // Subclasses can use this to check thread safety.
  // For example: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  THREAD_CHECKER(thread_checker_);

 private:
  // Note: `receiver_` is intentionally not exposed to implementations, since it
  // is otherwise easy to write bugs that leak `this` by resetting the receiver
  // without deleting `this`.
  mojo::Receiver<Interface> receiver_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_H_
