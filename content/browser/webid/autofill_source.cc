// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/autofill_source.h"

#include <optional>

#include "content/browser/webid/request.h"
#include "content/browser/webid/request_page_data.h"

namespace content::webid {

using MediationRequirement = ::password_manager::CredentialMediationRequirement;

// static
AutofillSource* AutofillSource::FromPage(Page& page) {
  auto* request =
      RequestPageData::GetOrCreateForPage(page)->PendingWebIdentityRequest();

  if (!request || request->GetMediationRequirement() !=
                      MediationRequirement::kConditional) {
    return nullptr;
  }

  return request;
}

}  // namespace content::webid
