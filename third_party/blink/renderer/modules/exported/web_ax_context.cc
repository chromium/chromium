// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_ax_context.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

WebAXContext::WebAXContext(WebDocument root_document, const ui::AXMode& mode)
    : private_(new AXContext(*root_document.Unwrap<Document>(), mode)) {}

WebAXContext::~WebAXContext() {}

WebAXObject WebAXContext::Root() const {
  // It is an error to call AXContext::GetAXObjectCache() if the underlying
  // document is no longer active, so early return in that case to prevent
  // crashes that might otherwise happen in some cases (see crbug.com/1094576).
  if (!private_->HasActiveDocument())
    return WebAXObject();

  // Make sure that layout is updated before a root ax object is created.
  WebAXObject::UpdateLayout(WebDocument(private_->GetDocument()));

  return WebAXObject(
      static_cast<AXObjectCacheImpl*>(&private_->GetAXObjectCache())->Root());
}

const ui::AXMode& WebAXContext::GetAXMode() const {
  return private_->GetAXMode();
}

void WebAXContext::SetAXMode(const ui::AXMode& mode) const {
  private_->SetAXMode(mode);
}

void WebAXContext::ResetSerializer() {
  private_->GetAXObjectCache().ResetSerializer();
}

int WebAXContext::GenerateAXID() const {
  return private_->GetAXObjectCache().GenerateAXID();
}

void WebAXContext::SerializeLocationChanges() const {
  private_->GetAXObjectCache().SerializeLocationChanges();
}

WebAXObject WebAXContext::GetPluginRoot() {
  return WebAXObject(private_->GetAXObjectCache().GetPluginRoot());
}

void WebAXContext::Freeze() {
  private_->GetAXObjectCache().Freeze();
}

void WebAXContext::Thaw() {
  private_->GetAXObjectCache().Thaw();
}

bool WebAXContext::SerializeEntireTree(bool exclude_offscreen,
                                       size_t max_node_count,
                                       base::TimeDelta timeout,
                                       ui::AXTreeUpdate* response) {
  return private_->GetAXObjectCache().SerializeEntireTree(
      exclude_offscreen, max_node_count, timeout, response);
}

void WebAXContext::MarkAllImageAXObjectsDirty(
    ax::mojom::blink::Action event_from_action) {
  return private_->GetAXObjectCache().MarkAllImageAXObjectsDirty(
      event_from_action);
}

}  // namespace blink
