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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_CHILD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_CHILD_H_

#include "third_party/blink/renderer/core/html/imports/html_import.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class HTMLImportLoader;
class HTMLImportChildClient;
class HTMLLinkElement;

//
// An import tree node subclass to encapsulate imported document
// lifecycle. This class is owned by HTMLImportsController. The actual loading
// is done by HTMLImportLoader, which can be shared among multiple
// HTMLImportChild of same link URL.
//
class HTMLImportChild final : public HTMLImport {
 public:
  HTMLImportChild(const KURL&,
                  HTMLImportLoader*,
                  HTMLImportChildClient*,
                  SyncMode);
  ~HTMLImportChild() final;
  void Dispose();

  HTMLLinkElement* Link() const;
  const KURL& Url() const { return url_; }

  void OwnerInserted();
  void DidShareLoader();
  void DidStartLoading();

  // HTMLImport
  Document* GetDocument() const final;
  bool HasFinishedLoading() const final;
  HTMLImportLoader* Loader() const final;
  void StateWillChange() final;
  void StateDidChange() final;
  void Trace(Visitor*) const override;

  void DidFinishLoading();
  void DidFinishUpgradingCustomElements();
  void Normalize();

 private:
  void DidFinish();
  void ShareLoader();
  void CreateCustomElementMicrotaskStepIfNeeded();
  void InvalidateCustomElementMicrotaskStep();

  KURL url_;
  Member<HTMLImportLoader> loader_;
  Member<HTMLImportChildClient> client_;
};

inline HTMLImportChild* ToHTMLImportChild(HTMLImport* import) {
  DCHECK(!import || !import->IsRoot());
  return static_cast<HTMLImportChild*>(import);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_CHILD_H_
