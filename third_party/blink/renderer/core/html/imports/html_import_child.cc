/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/imports/html_import_child.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child_client.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/imports/html_import_tree_root.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"

namespace blink {

HTMLImportChild::HTMLImportChild(const KURL& url,
                                 HTMLImportLoader* loader,
                                 HTMLImportChildClient* client,
                                 SyncMode sync)
    : HTMLImport(sync), url_(url), loader_(loader), client_(client) {
  DCHECK(loader_);
  DCHECK(client_);
}

HTMLImportChild::~HTMLImportChild() = default;

void HTMLImportChild::OwnerInserted() {
  if (!loader_->IsDone())
    return;

  DCHECK(Root());
  DCHECK(Root()->GetDocument());
  Root()->GetDocument()->GetStyleEngine().HtmlImportAddedOrRemoved();
}

void HTMLImportChild::DidShareLoader() {
  StateWillChange();
}

void HTMLImportChild::DidStartLoading() {
}

void HTMLImportChild::DidFinish() {
  if (client_)
    client_->DidFinish();
}

void HTMLImportChild::DidFinishLoading() {
  StateWillChange();
}

void HTMLImportChild::DidFinishUpgradingCustomElements() {
  StateWillChange();
}

void HTMLImportChild::Dispose() {
  if (Parent())
    Parent()->RemoveChild(this);

  DCHECK(loader_);
  loader_->RemoveImport(this);
  loader_ = nullptr;

  if (client_) {
    client_->ImportChildWasDisposed(this);
    client_ = nullptr;
  }
}

Document* HTMLImportChild::GetDocument() const {
  DCHECK(loader_);
  return loader_->GetDocument();
}

void HTMLImportChild::StateWillChange() {
  To<HTMLImportTreeRoot>(Root())->ScheduleRecalcState();
}

void HTMLImportChild::StateDidChange() {
  HTMLImport::StateDidChange();

  if (GetState().IsReady())
    DidFinish();
}

bool HTMLImportChild::HasFinishedLoading() const {
  DCHECK(loader_);

  return loader_->IsDone();
}

HTMLImportLoader* HTMLImportChild::Loader() const {
  // This should never be called after dispose().
  DCHECK(loader_);
  return loader_;
}

HTMLLinkElement* HTMLImportChild::Link() const {
  if (!client_)
    return nullptr;
  return client_->Link();
}

// Ensuring following invariants against the import tree:
// - HTMLImportLoader::FirstImport() is the "first import" of the DFS order of
//   the import tree loaded by the loader.
// - The "first import" manages all the children that are loaded by the
// document.
void HTMLImportChild::Normalize() {
  DCHECK(loader_);

  if (!loader_->IsFirstImport(this) && Precedes(loader_->FirstImport())) {
    HTMLImportChild* old_first = loader_->FirstImport();
    loader_->MoveToFirst(this);
    TakeChildrenFrom(old_first);
  }

  for (HTMLImportChild* child = ToHTMLImportChild(FirstChild()); child;
       child = ToHTMLImportChild(child->Next())) {
    child->Normalize();
  }
}

void HTMLImportChild::Trace(Visitor* visitor) const {
  visitor->Trace(loader_);
  visitor->Trace(client_);
  HTMLImport::Trace(visitor);
}

}  // namespace blink
