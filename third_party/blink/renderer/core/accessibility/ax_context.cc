// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class AXObjectCache;

AXContext::AXContext(Document& document, const ui::AXMode& ax_mode)
    : document_(&document), ax_mode_(ax_mode) {
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
  DCHECK(document_->ExistingAXObjectCache());
  DCHECK_EQ(ax_mode_.flags(),
            document_->ExistingAXObjectCache()->GetAXMode().flags() &
                ax_mode_.flags());

  return *document_->ExistingAXObjectCache();
}

bool AXContext::HasActiveDocument() {
  return document_ && document_->IsActive();
}

Document* AXContext::GetDocument() {
  return document_;
}

void AXContext::SetAXMode(const ui::AXMode& mode) {
  DCHECK(!mode.is_mode_off()) << "When turning off accessibility, call "
                                 "document_->RemoveAXContext() instead.";
  ax_mode_ = mode;
  document_->AXContextModeChanged();

  DCHECK_EQ(ax_mode_.flags(),
            document_->ExistingAXObjectCache()->GetAXMode().flags() &
                ax_mode_.flags());
}

}  // namespace blink
