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

#include "third_party/blink/renderer/core/html/imports/link_import.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/imports/html_import_tree_root.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LinkImport::LinkImport(HTMLLinkElement* owner)
    : LinkResource(owner), child_(nullptr) {}

LinkImport::~LinkImport() = default;

Document* LinkImport::ImportedDocument() const {
  if (!child_ || !owner_ || !owner_->isConnected())
    return nullptr;
  if (child_->Loader()->HasError())
    return nullptr;
  return child_->GetDocument();
}

void LinkImport::Process() {
  if (child_)
    return;
  if (!owner_)
    return;
  if (!ShouldLoadResource())
    return;

  const KURL& url = owner_->GetNonEmptyURLAttribute(html_names::kHrefAttr);
  if (url.IsEmpty() || !url.IsValid()) {
    DidFinish();
    return;
  }

  ResourceRequest resource_request(GetDocument().CompleteURL(url));
  network::mojom::ReferrerPolicy referrer_policy = owner_->GetReferrerPolicy();
  resource_request.SetReferrerPolicy(referrer_policy);

  ResourceLoaderOptions options;
  options.initiator_info.name = owner_->localName();

  FetchParameters params(resource_request, options);
  params.SetCharset(GetCharset());
  params.SetContentSecurityPolicyNonce(owner_->nonce());

  HTMLImportsController* controller = GetDocument().EnsureImportsController();
  child_ = controller->Load(GetDocument(), this, params);
  if (!child_) {
    DidFinish();
    return;
  }
}

void LinkImport::DidFinish() {
  if (!owner_ || !owner_->isConnected())
    return;
  owner_->ScheduleEvent();
}

void LinkImport::ImportChildWasDisposed(HTMLImportChild* child) {
  DCHECK_EQ(child_, child);
  child_ = nullptr;
  owner_ = nullptr;
}

bool LinkImport::IsSync() const {
  return owner_ && !owner_->Async();
}

HTMLLinkElement* LinkImport::Link() {
  return owner_;
}

bool LinkImport::HasLoaded() const {
  return owner_ && child_ && child_->HasFinishedLoading() &&
         !child_->Loader()->HasError();
}

void LinkImport::OwnerInserted() {
  if (child_)
    child_->OwnerInserted();
}

void LinkImport::OwnerRemoved() {
  if (owner_)
    GetDocument().GetStyleEngine().HtmlImportAddedOrRemoved();
}

void LinkImport::Trace(Visitor* visitor) {
  visitor->Trace(child_);
  HTMLImportChildClient::Trace(visitor);
  LinkResource::Trace(visitor);
}

}  // namespace blink
