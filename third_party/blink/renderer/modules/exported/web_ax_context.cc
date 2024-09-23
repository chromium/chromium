// Copyright 2018 The Chromium Authors
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

bool WebAXContext::HasActiveDocument() const {
  return private_->HasActiveDocument();
}

bool WebAXContext::HasAXObjectCache() const {
  CHECK(HasActiveDocument());
  return private_->GetDocument()->ExistingAXObjectCache();
}

const ui::AXMode& WebAXContext::GetAXMode() const {
  DCHECK(!private_->GetAXMode().is_mode_off());
  return private_->GetAXMode();
}

void WebAXContext::SetAXMode(const ui::AXMode& mode) const {
  private_->SetAXMode(mode);
}

void WebAXContext::MarkDocumentDirty() {
  if (!HasActiveDocument()) {
    return;
  }
  private_->GetAXObjectCache().MarkDocumentDirty();
}

void WebAXContext::ResetSerializer() {
  if (!HasActiveDocument()) {
    return;
  }
  private_->GetAXObjectCache().ResetSerializer();
}

bool WebAXContext::SerializeEntireTree(
    size_t max_node_count,
    base::TimeDelta timeout,
    ui::AXTreeUpdate* response,
    std::set<ui::AXSerializationErrorFlag>* out_error) {
  CHECK(HasActiveDocument());
  CHECK(HasAXObjectCache());
  CHECK(private_->GetDocument()->ExistingAXObjectCache());

  UpdateAXForAllDocuments();

  ScopedFreezeAXCache freeze(private_->GetAXObjectCache());
  return private_->GetAXObjectCache().SerializeEntireTree(
      max_node_count, timeout, response, out_error);
}

void WebAXContext::GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                                       std::vector<ui::AXNodeData*>& nodes) {
  private_->GetAXObjectCache().GetImagesToAnnotate(updates, nodes);
}

void WebAXContext::UpdateAXForAllDocuments() {
  if (!HasActiveDocument()) {
    return;
  }
  return private_->GetAXObjectCache().UpdateAXForAllDocuments();
}

void WebAXContext::ScheduleImmediateSerialization() {
  if (!HasActiveDocument()) {
    return;
  }

  auto& cache = private_->GetAXObjectCache();
  cache.ScheduleImmediateSerialization();
}

void WebAXContext::AddEventToSerializationQueue(const ui::AXEvent& event,
                                                bool immediate_serialization) {
  if (!HasActiveDocument()) {
    return;
  }

  auto& cache = private_->GetAXObjectCache();
  cache.AddEventToSerializationQueue(event, immediate_serialization);
}

void WebAXContext::OnSerializationCancelled() {
  if (!HasActiveDocument()) {
    return;
  }

  auto& cache = private_->GetAXObjectCache();
  cache.OnSerializationCancelled();
}

void WebAXContext::OnSerializationStartSend() {
  if (!HasActiveDocument()) {
    return;
  }

  auto& cache = private_->GetAXObjectCache();
  cache.OnSerializationStartSend();
}

bool WebAXContext::IsSerializationInFlight() const {
  if (!HasActiveDocument()) {
    return false;
  }

  const auto& cache = private_->GetAXObjectCache();
  return cache.IsSerializationInFlight();
}

void WebAXContext::OnSerializationReceived() {
  if (!HasActiveDocument()) {
    return;
  }
  return private_->GetAXObjectCache().OnSerializationReceived();
}

void WebAXContext::FireLoadCompleteIfLoaded() {
  if (!private_->HasActiveDocument())
    return;
  return private_->GetDocument()->DispatchHandleLoadComplete();
}

void WebAXContext::SetSerializationResetToken(uint32_t reset_token) const {
  private_->GetAXObjectCache().SetSerializationResetToken(reset_token);
}
}  // namespace blink
