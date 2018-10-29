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

#include "third_party/blink/renderer/core/html/imports/html_import.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/imports/html_import_state_resolver.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

HTMLImport* HTMLImport::Root() {
  HTMLImport* i = this;
  while (i->Parent())
    i = i->Parent();
  return i;
}

bool HTMLImport::Precedes(HTMLImport* import) {
  for (HTMLImport* i = this; i; i = TraverseNext(i)) {
    if (i == import)
      return true;
  }

  return false;
}

bool HTMLImport::FormsCycle() const {
  for (const HTMLImport* i = Parent(); i; i = i->Parent()) {
    if (i->GetDocument() == GetDocument())
      return true;
  }

  return false;
}

void HTMLImport::AppendImport(HTMLImport* child) {
  AppendChild(child);

  // This prevents HTML parser from going beyond the
  // blockage line before the precise state is computed by recalcState().
  if (child->IsSync())
    state_ = HTMLImportState::BlockedState();

  StateWillChange();
}

void HTMLImport::StateDidChange() {
  if (!GetState().ShouldBlockScriptExecution()) {
    if (Document* document = GetDocument())
      document->DidLoadAllImports();
  }
}

void HTMLImport::RecalcTreeState(HTMLImport* root) {
  HeapHashMap<Member<HTMLImport>, HTMLImportState> snapshot;
  HeapVector<Member<HTMLImport>> updated;

  for (HTMLImport* i = root; i; i = TraverseNext(i)) {
    snapshot.insert(i, i->GetState());
    i->state_ = HTMLImportState::InvalidState();
  }

  // The post-visit DFS order matters here because
  // HTMLImportStateResolver in RecalcState() Depends on
  // |state_| of its children and precedents of ancestors.
  // Accidental cycle dependency of state computation is prevented
  // by InvalidateCachedState() and IsStateCacheValid() check.
  for (HTMLImport* i = TraverseFirstPostOrder(root); i;
       i = TraverseNextPostOrder(i)) {
    DCHECK(!i->state_.IsValid());
    i->state_ = HTMLImportStateResolver(i).Resolve();

    HTMLImportState new_state = i->GetState();
    HTMLImportState old_state = snapshot.at(i);
    // Once the state reaches Ready, it shouldn't go back.
    DCHECK(!old_state.IsReady() || old_state <= new_state);
    if (new_state != old_state)
      updated.push_back(i);
  }

  for (const auto& import : updated)
    import->StateDidChange();
}

}  // namespace blink
