// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/federated_auth_autofill_source.h"

#include <optional>

#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_auth_request_page_data.h"

namespace content {

// static
FederatedAuthAutofillSource* FederatedAuthAutofillSource::FromPage(
    content::Page& page) {
  auto* request = FederatedAuthRequestPageData::GetOrCreateForPage(page)
                      ->PendingWebIdentityRequest();

  if (!request || request->GetMediationRequirement() !=
                      MediationRequirement::kConditional) {
    return nullptr;
  }

  return request;
}

}  // namespace content
