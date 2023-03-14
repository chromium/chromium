// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_page_data.h"

namespace content {

FederatedAuthRequestPageData::FederatedAuthRequestPageData(Page& page)
    : PageUserData<FederatedAuthRequestPageData>(page) {}

FederatedAuthRequestPageData::~FederatedAuthRequestPageData() = default;

PAGE_USER_DATA_KEY_IMPL(FederatedAuthRequestPageData);

FederatedAuthRequestImpl*
FederatedAuthRequestPageData::PendingWebIdentityRequest() {
  return pending_web_identity_request_;
}

void FederatedAuthRequestPageData::SetPendingWebIdentityRequest(
    FederatedAuthRequestImpl* request) {
  pending_web_identity_request_ = request;
}

}  // namespace content
