// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_page_data.h"

namespace content::webid {

RequestPageData::RequestPageData(Page& page)
    : PageUserData<RequestPageData>(page) {}

RequestPageData::~RequestPageData() = default;

PAGE_USER_DATA_KEY_IMPL(RequestPageData);

Request* RequestPageData::PendingWebIdentityRequest() {
  return pending_web_identity_request_;
}

void RequestPageData::SetPendingWebIdentityRequest(Request* request) {
  pending_web_identity_request_ = request;
}

}  // namespace content::webid
