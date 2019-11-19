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

#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child_client.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/imports/html_import_tree_root.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

HTMLImportsController::HTMLImportsController(Document& master)
    : root_(MakeGarbageCollected<HTMLImportTreeRoot>(&master)) {}

void HTMLImportsController::Dispose() {
  // TODO(tkent): We copy loaders_ before iteration to avoid crashes.
  // This copy should be unnecessary. loaders_ is not modified during
  // the iteration.  Also, null-check for |loader| should be
  // unnecessary.  crbug.com/843151.
  LoaderList list;
  list.swap(loaders_);
  for (const auto& loader : list) {
    if (loader)
      loader->Dispose();
  }

  if (root_) {
    root_->Dispose();
    root_.Clear();
  }
}

static bool MakesCycle(HTMLImport* parent, const KURL& url) {
  for (HTMLImport* ancestor = parent; ancestor; ancestor = ancestor->Parent()) {
    if (!ancestor->IsRoot() &&
        EqualIgnoringFragmentIdentifier(ToHTMLImportChild(parent)->Url(), url))
      return true;
  }

  return false;
}

HTMLImportChild* HTMLImportsController::CreateChild(
    const KURL& url,
    HTMLImportLoader* loader,
    HTMLImport* parent,
    HTMLImportChildClient* client) {
  HTMLImport::SyncMode mode = client->IsSync() && !MakesCycle(parent, url)
                                  ? HTMLImport::kSync
                                  : HTMLImport::kAsync;
  if (mode == HTMLImport::kAsync) {
    UseCounter::Count(root_->GetDocument(),
                      WebFeature::kHTMLImportsAsyncAttribute);
  }

  HTMLImportChild* child =
      MakeGarbageCollected<HTMLImportChild>(url, loader, client, mode);
  parent->AppendImport(child);
  loader->AddImport(child);
  return root_->Add(child);
}

HTMLImportChild* HTMLImportsController::Load(const Document& parent_document,
                                             HTMLImportChildClient* client,
                                             FetchParameters& params) {
  DCHECK(client);

  HTMLImportLoader* parent_loader = LoaderFor(parent_document);
  HTMLImport* parent =
      parent_loader ? static_cast<HTMLImport*>(parent_loader->FirstImport())
                    : static_cast<HTMLImport*>(root_);

  const KURL& url = params.Url();

  DCHECK(!url.IsEmpty());
  DCHECK(url.IsValid());

  if (HTMLImportChild* child_to_share_with = root_->Find(url)) {
    HTMLImportLoader* loader = child_to_share_with->Loader();
    DCHECK(loader);
    HTMLImportChild* child = CreateChild(url, loader, parent, client);
    child->DidShareLoader();
    return child;
  }

  scoped_refptr<const SecurityOrigin> security_origin =
      Master()->GetSecurityOrigin();
  ResourceFetcher* fetcher = parent->GetDocument()->Fetcher();

  if (base::FeatureList::IsEnabled(
          features::kHtmlImportsRequestInitiatorLock)) {
    Document* context_document = parent->GetDocument()->ContextDocument();
    if (!context_document)
      return nullptr;

    security_origin = context_document->GetSecurityOrigin();
    fetcher = context_document->Fetcher();
  }

  params.SetCrossOriginAccessControl(security_origin.get(),
                                     kCrossOriginAttributeAnonymous);

  auto* loader = MakeGarbageCollected<HTMLImportLoader>(this);
  RawResource::FetchImport(params, fetcher, loader);
  loaders_.push_back(loader);
  HTMLImportChild* child = CreateChild(url, loader, parent, client);
  child->DidStartLoading();
  return child;
}

Document* HTMLImportsController::Master() const {
  return root_ ? root_->GetDocument() : nullptr;
}

bool HTMLImportsController::ShouldBlockScriptExecution(
    const Document& document) const {
  DCHECK_EQ(document.ImportsController(), this);
  if (HTMLImportLoader* loader = LoaderFor(document))
    return loader->ShouldBlockScriptExecution();
  return root_->GetState().ShouldBlockScriptExecution();
}

HTMLImportLoader* HTMLImportsController::LoaderFor(
    const Document& document) const {
  for (const auto& loader : loaders_) {
    if (loader->GetDocument() == &document)
      return loader.Get();
  }

  return nullptr;
}

void HTMLImportsController::Trace(Visitor* visitor) {
  visitor->Trace(root_);
  visitor->Trace(loaders_);
}

}  // namespace blink
