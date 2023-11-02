// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/weak_document_ptr.h"

namespace content {

WeakDocumentPtr::WeakDocumentPtr(base::WeakPtr<RenderFrameHost> weak_rfh)
    : weak_document_(std::move(weak_rfh)) {}

}  // namespace content
