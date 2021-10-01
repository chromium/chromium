// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_INTERNAL_H_
#define CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_INTERNAL_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Internal helper to provide a common base class for `DocumentServiceBase<T>`
// so that //content can internally track all live `DocumentServiceBase<T>`
// instances.
//
// TODO(dcheng): Rename DocumentServiceBase to DocumentService and
// DocumentServiceBaseInternal to DocumentServiceBase.
class CONTENT_EXPORT DocumentServiceBaseInternal {
 public:
  DocumentServiceBaseInternal(const DocumentServiceBaseInternal&) = delete;
  DocumentServiceBaseInternal& operator=(const DocumentServiceBaseInternal&) =
      delete;

  virtual ~DocumentServiceBaseInternal();

 protected:
  explicit DocumentServiceBaseInternal(RenderFrameHost* render_frame_host);

  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

 private:
  const raw_ptr<RenderFrameHost> render_frame_host_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOCUMENT_SERVICE_BASE_INTERNAL_H_
