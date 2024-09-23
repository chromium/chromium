// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"

#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

FetchClientSettingsObjectImpl::FetchClientSettingsObjectImpl(
    ExecutionContext& execution_context)
    : execution_context_(execution_context) {
  DCHECK(execution_context_->IsContextThread());
}

const KURL& FetchClientSettingsObjectImpl::GlobalObjectUrl() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->Url();
}

const KURL& FetchClientSettingsObjectImpl::BaseUrl() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->BaseURL();
}

const SecurityOrigin* FetchClientSettingsObjectImpl::GetSecurityOrigin() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->GetSecurityOrigin();
}

network::mojom::ReferrerPolicy
FetchClientSettingsObjectImpl::GetReferrerPolicy() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->GetReferrerPolicy();
}

const String FetchClientSettingsObjectImpl::GetOutgoingReferrer() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->OutgoingReferrer();
}

HttpsState FetchClientSettingsObjectImpl::GetHttpsState() const {
  DCHECK(execution_context_->IsContextThread());
  return execution_context_->GetHttpsState();
}

AllowedByNosniff::MimeTypeCheck
FetchClientSettingsObjectImpl::MimeTypeCheckForClassicWorkerScript() const {
  if (execution_context_->IsWindow()) {
    // For worker creation on a document, don't impose strict MIME-type checks
    // on the top-level worker script for backward compatibility. Note that
    // there is a plan to deprecate legacy mime types for workers. See
    // https://crbug.com/794548.
    //
    // For worker creation on a document with off-the-main-thread top-level
    // worker classic script loading, this value is propagated to
    // outsideSettings FCSO.
    return AllowedByNosniff::MimeTypeCheck::kLaxForWorker;
  }

  // For importScripts() and nested worker top-level scripts impose the strict
  // MIME-type checks.
  // Nested workers is a new feature (enabled by default in M69) and there is no
  // backward compatibility issue.
  return AllowedByNosniff::MimeTypeCheck::kStrict;
}

mojom::blink::InsecureRequestPolicy
FetchClientSettingsObjectImpl::GetInsecureRequestsPolicy() const {
  return execution_context_->GetSecurityContext().GetInsecureRequestPolicy();
}

const FetchClientSettingsObject::InsecureNavigationsSet&
FetchClientSettingsObjectImpl::GetUpgradeInsecureNavigationsSet() const {
  return execution_context_->GetSecurityContext()
      .InsecureNavigationsToUpgrade();
}

void FetchClientSettingsObjectImpl::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  FetchClientSettingsObject::Trace(visitor);
}

}  // namespace blink
