// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/imports/html_import_tree_root.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"

namespace blink {

HTMLImportTreeRoot::HTMLImportTreeRoot(Document* document)
    : HTMLImport(HTMLImport::kSync),
      document_(document),
      recalc_timer_(
          document->GetFrame()->GetTaskRunner(TaskType::kInternalDefault),
          this,
          &HTMLImportTreeRoot::RecalcTimerFired) {
  ScheduleRecalcState();  // This recomputes initial state.
}

HTMLImportTreeRoot::~HTMLImportTreeRoot() = default;

void HTMLImportTreeRoot::Dispose() {
  for (const auto& import_child : imports_)
    import_child->Dispose();
  imports_.clear();
  document_ = nullptr;
  recalc_timer_.Stop();
}

Document* HTMLImportTreeRoot::GetDocument() const {
  return document_;
}

bool HTMLImportTreeRoot::HasFinishedLoading() const {
  return !document_->Parsing() &&
         document_->GetStyleEngine().HaveScriptBlockingStylesheetsLoaded();
}

void HTMLImportTreeRoot::StateWillChange() {
  ScheduleRecalcState();
}

void HTMLImportTreeRoot::StateDidChange() {
  HTMLImport::StateDidChange();

  if (GetState().IsReady())
    document_->CheckCompleted();
}

void HTMLImportTreeRoot::ScheduleRecalcState() {
  DCHECK(document_);
  if (recalc_timer_.IsActive() || !document_->IsActive())
    return;
  recalc_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

HTMLImportChild* HTMLImportTreeRoot::Add(HTMLImportChild* child) {
  imports_.push_back(child);
  return imports_.back().Get();
}

HTMLImportChild* HTMLImportTreeRoot::Find(const KURL& url) const {
  for (const auto& candidate : imports_) {
    if (EqualIgnoringFragmentIdentifier(candidate->Url(), url))
      return candidate;
  }

  return nullptr;
}

void HTMLImportTreeRoot::RecalcTimerFired(TimerBase*) {
  DCHECK(document_);
  HTMLImport::RecalcTreeState(this);
}

void HTMLImportTreeRoot::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(imports_);
  HTMLImport::Trace(visitor);
}

}  // namespace blink
