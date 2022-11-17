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

const ui::AXMode& WebAXContext::GetAXMode() const {
  return private_->GetAXMode();
}

void WebAXContext::SetAXMode(const ui::AXMode& mode) const {
  private_->SetAXMode(mode);
}

void WebAXContext::ResetSerializer() {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().ResetSerializer();
}

int WebAXContext::GenerateAXID() const {
  if (!private_->HasActiveDocument())
    return -1;
  return private_->GetAXObjectCache().GenerateAXID();
}

void WebAXContext::SerializeLocationChanges() const {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().SerializeLocationChanges();
}

WebAXObject WebAXContext::GetPluginRoot() {
  if (!private_->HasActiveDocument())
    return WebAXObject();
  return WebAXObject(private_->GetAXObjectCache().GetPluginRoot());
}

void WebAXContext::Freeze() {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().Freeze();
}

void WebAXContext::Thaw() {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().Thaw();
}

bool WebAXContext::SerializeEntireTree(bool exclude_offscreen,
                                       size_t max_node_count,
                                       base::TimeDelta timeout,
                                       ui::AXTreeUpdate* response) {
  if (!private_->HasActiveDocument())
    return false;
  if (!private_->GetDocument()->ExistingAXObjectCache()) {
    // TODO(chrishtr): not clear why this can happen.
    NOTREACHED();
    return false;
  }

  return private_->GetAXObjectCache().SerializeEntireTree(
      exclude_offscreen, max_node_count, timeout, response);
}

void WebAXContext::MarkAllImageAXObjectsDirty() {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().MarkAllImageAXObjectsDirty();
}

void WebAXContext::SerializeDirtyObjectsAndEvents(
    bool has_plugin_tree_source,
    std::vector<ui::AXTreeUpdate>& updates,
    std::vector<ui::AXEvent>& events,
    bool& had_end_of_test_event,
    bool& had_load_complete_messages,
    bool& need_to_send_location_changes) {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().SerializeDirtyObjectsAndEvents(
      has_plugin_tree_source, updates, events, had_end_of_test_event,
      had_load_complete_messages, need_to_send_location_changes);
}

void WebAXContext::ClearDirtyObjectsAndPendingEvents() {
  if (!private_->HasActiveDocument())
    return;
  private_->GetAXObjectCache().ClearDirtyObjectsAndPendingEvents();
}

bool WebAXContext::HasDirtyObjects() {
  if (!private_->HasActiveDocument())
    return true;
  return private_->GetAXObjectCache().HasDirtyObjects();
}

bool WebAXContext::AddPendingEvent(const ui::AXEvent& event,
                                   bool insert_at_beginning) {
  if (!private_->HasActiveDocument())
    return true;
  return private_->GetAXObjectCache().AddPendingEvent(event,
                                                      insert_at_beginning);
}

void WebAXContext::UpdateAXForAllDocuments() {
  if (!private_->HasActiveDocument())
    return;
  return private_->GetAXObjectCache().UpdateAXForAllDocuments();
}

void WebAXContext::ScheduleAXUpdate() {
  if (!private_->HasActiveDocument())
    return;

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
