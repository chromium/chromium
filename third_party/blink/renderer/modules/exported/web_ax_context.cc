// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_ax_context.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

WebAXContext::WebAXContext(WebDocument root_document)
    : private_(new AXContext(*root_document.Unwrap<Document>())) {}

WebAXContext::~WebAXContext() {}

WebAXObject WebAXContext::Root() const {
  // It is an error to call AXContext::GetAXObjectCache() if the underlying
  // document is no longer active, so early return in that case to prevent
  // crashes that might otherwise happen in some cases (see crbug.com/1094576).
  if (!private_->HasActiveDocument())
    return WebAXObject();

  return WebAXObject(
      static_cast<AXObjectCacheImpl*>(&private_->GetAXObjectCache())->Root());
}

}  // namespace blink
