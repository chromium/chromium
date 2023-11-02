// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;
enum class DocumentServiceDestructionReason : int;

namespace internal {

// Internal helper to provide a common base class for `DocumentService<T>` so
// that //content can internally track all live `DocumentService<T>` instances.
class CONTENT_EXPORT DocumentServiceBase {
 public:
  DocumentServiceBase(const DocumentServiceBase&) = delete;
  DocumentServiceBase& operator=(const DocumentServiceBase&) = delete;

  virtual ~DocumentServiceBase();

  // Virtual as an implementation detail of //content, which keeps a generic
  // container of pointers to document services via this base class, but still
  // needs to be able to end the lifetime of document service instances. See
  // `DocumentService<T>` for more information.
  virtual void ResetAndDeleteThis() = 0;

  // To be called just before the destructor, when the object does not
  // self-destroy via one of the *AndDeleteThis() helpers. `reason` provides
  // context on why `this` is being destroyed (i.e. the document is deleted or
  // the Mojo message pipe is disconnected); subclasses can override this method
  // to react in a specific way to a destruction reason.
  virtual void WillBeDestroyed(DocumentServiceDestructionReason reason) {}

 protected:
  explicit DocumentServiceBase(RenderFrameHost& render_frame_host);

  RenderFrameHost& render_frame_host() const { return *render_frame_host_; }

 private:
  const raw_ref<RenderFrameHost> render_frame_host_;
};

}  // namespace internal

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_
