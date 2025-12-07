// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/autofill_source.h"

#include <optional>

#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/request_service.h"

namespace content::webid {

// static
AutofillSource* AutofillSource::FromPage(content::Page& page) {
  auto* request = webid::RequestPageData::GetOrCreateForPage(page)
                      ->PendingWebIdentityRequest();

  if (!request || request->GetMediationRequirement() !=
                      webid::MediationRequirement::kConditional) {
    return nullptr;
  }

  return request;
}

}  // namespace content::webid
