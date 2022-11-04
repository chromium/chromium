// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_web_contents_data.h"

namespace content {

FederatedAuthRequestWebContentsData::FederatedAuthRequestWebContentsData(
    WebContents* contents)
    : WebContentsUserData<FederatedAuthRequestWebContentsData>(*contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FederatedAuthRequestWebContentsData);

bool FederatedAuthRequestWebContentsData::HasPendingWebIdentityRequest() {
  return has_pending_web_identity_request_;
}

void FederatedAuthRequestWebContentsData::SetHasPendingWebIdentityRequest(
    bool has_pending_request) {
  has_pending_web_identity_request_ = has_pending_request;
}

}  // namespace content
