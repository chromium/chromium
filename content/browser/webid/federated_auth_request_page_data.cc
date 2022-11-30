// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_page_data.h"

namespace content {

FederatedAuthRequestPageData::FederatedAuthRequestPageData(Page& page)
    : PageUserData<FederatedAuthRequestPageData>(page) {}

PAGE_USER_DATA_KEY_IMPL(FederatedAuthRequestPageData);

bool FederatedAuthRequestPageData::HasPendingWebIdentityRequest() {
  return has_pending_web_identity_request_;
}

void FederatedAuthRequestPageData::SetHasPendingWebIdentityRequest(
    bool has_pending_request) {
  has_pending_web_identity_request_ = has_pending_request;
}

}  // namespace content
