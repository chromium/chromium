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

int WebAXContext::GenerateAXID() const {
  DCHECK(HasActiveDocument());
  return private_->GetAXObjectCache().GenerateAXID();
}

void WebAXContext::SerializeLocationChanges(uint32_t reset_token) const {
  if (!HasActiveDocument()) {
    return;
  }
  ScopedFreezeAXCache freeze(private_->GetAXObjectCache());
  private_->GetAXObjectCache().SerializeLocationChanges(reset_token);
}

WebAXObject WebAXContext::GetPluginRoot() {
  if (!HasActiveDocument()) {
    return WebAXObject();
  }
  return WebAXObject(private_->GetAXObjectCache().GetPluginRoot());
}

bool WebAXContext::SerializeEntireTree(size_t max_node_count,
                                       base::TimeDelta timeout,
                                       ui::AXTreeUpdate* response) {
  CHECK(HasActiveDocument());
  CHECK(private_->GetDocument()->ExistingAXObjectCache());

  UpdateAXForAllDocuments();

  ScopedFreezeAXCache freeze(private_->GetAXObjectCache());
  return private_->GetAXObjectCache().SerializeEntireTree(max_node_count,
                                                          timeout, response);
}

void WebAXContext::SerializeDirtyObjectsAndEvents(
    bool has_plugin_tree_source,
    std::vector<ui::AXTreeUpdate>& updates,
    std::vector<ui::AXEvent>& events,
    bool& had_end_of_test_event,
    bool& had_load_complete_messages,
    bool& need_to_send_location_changes) {
  if (!HasActiveDocument()) {
    return;
  }

  ScopedFreezeAXCache freeze(private_->GetAXObjectCache());
  private_->GetAXObjectCache().SerializeDirtyObjectsAndEvents(
      has_plugin_tree_source, updates, events, had_end_of_test_event,
      had_load_complete_messages, need_to_send_location_changes);
}

void WebAXContext::GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                                       std::vector<ui::AXNodeData*>& nodes) {
  private_->GetAXObjectCache().GetImagesToAnnotate(updates, nodes);
}

bool WebAXContext::HasDirtyObjects() {
  if (!HasActiveDocument()) {
    return true;
  }
  return private_->GetAXObjectCache().HasDirtyObjects();
}

bool WebAXContext::AddPendingEvent(const ui::AXEvent& event,
                                   bool insert_at_beginning) {
  if (!HasActiveDocument()) {
    return true;
  }
  return private_->GetAXObjectCache().AddPendingEvent(event,
                                                      insert_at_beginning);
}

void WebAXContext::UpdateAXForAllDocuments() {
  if (!HasActiveDocument()) {
    return;
  }
  return private_->GetAXObjectCache().UpdateAXForAllDocuments();
}

void WebAXContext::ScheduleAXUpdate() {
  if (!HasActiveDocument()) {
    return;
  }

  const auto& cache = private_->GetAXObjectCache();

  // If no dirty objects are queued, it's not necessary to schedule an extra
  // visual update.
  if (!cache.HasDirtyObjects())
    return;

  return cache.ScheduleAXUpdate();
}

void WebAXContext::FireLoadCompleteIfLoaded() {
  if (!private_->HasActiveDocument())
    return;
  return private_->GetDocument()->DispatchHandleLoadComplete();
}
}  // namespace blink
