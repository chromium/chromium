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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_H_

#include "third_party/blink/renderer/core/html/imports/html_import_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/tree_node.h"

namespace blink {

class Document;
class HTMLImportLoader;

// The superclass of HTMLImportTreeRoot and HTMLImportChild
// This represents the import tree data structure.
class HTMLImport : public GarbageCollected<HTMLImport>,
                   public TreeNode<HTMLImport> {
 public:
  enum SyncMode { kSync = 0, kAsync = 1 };

  virtual ~HTMLImport() = default;

  // FIXME: Consider returning HTMLImportTreeRoot.
  HTMLImport* Root();
  bool Precedes(HTMLImport*);
  bool IsRoot() const { return !Parent(); }
  bool IsSync() const { return SyncMode(sync_) == kSync; }
  bool FormsCycle() const;
  const HTMLImportState& GetState() const { return state_; }

  void AppendImport(HTMLImport*);

  virtual Document* GetDocument() const = 0;
  virtual bool HasFinishedLoading() const = 0;
  virtual HTMLImportLoader* Loader() const { return nullptr; }
  virtual void StateWillChange() {}
  virtual void StateDidChange();

  virtual void Trace(Visitor* visitor) {}

 protected:
  // Stating from most conservative state.
  // It will be corrected through state update flow.
  explicit HTMLImport(SyncMode sync) : sync_(sync) {}

  static void RecalcTreeState(HTMLImport* root);

 private:
  HTMLImportState state_;
  unsigned sync_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_H_
