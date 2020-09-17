// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class AXObjectCache;

AXContext::AXContext(Document& document) : document_(&document) {
  DCHECK(document_);
  document_->AddAXContext(this);
}

AXContext::~AXContext() {
  if (document_)
    document_->RemoveAXContext(this);
}

AXObjectCache& AXContext::GetAXObjectCache() {
  DCHECK(document_);
  DCHECK(document_->IsActive());
  return *document_->ExistingAXObjectCache();
}

bool AXContext::HasActiveDocument() {
  return document_ && document_->IsActive();
}

}  // namespace blink
