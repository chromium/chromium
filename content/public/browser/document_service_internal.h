// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_

#include "base/memory/raw_ptr.h"
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

  // To be called just before the destructor, when the object does not
  // self-destroy (via `delete this`). It reports the reason that the object is
  // being destroyed via DocumentServiceDestructionReason, which gives the
  // subclass a chance to react in a specific way.
  virtual void WillBeDestroyed(DocumentServiceDestructionReason) {}

 protected:
  explicit DocumentServiceBase(RenderFrameHost* render_frame_host);

  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

 private:
  const raw_ptr<RenderFrameHost> render_frame_host_ = nullptr;
};

}  // namespace internal

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_INTERNAL_H_
