// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_ref.h"

namespace content {

DocumentRef::DocumentRef(base::SafeRef<RenderFrameHost> safe_document)
    : safe_document_(std::move(safe_document)) {}

}  // namespace content
