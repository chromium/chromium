// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_page_data.h"

namespace content::webid {

RequestPageData::RequestPageData(Page& page)
    : PageUserData<RequestPageData>(page) {}

RequestPageData::~RequestPageData() = default;

PAGE_USER_DATA_KEY_IMPL(RequestPageData);

RequestHandler* RequestPageData::PendingRequestHandler() {
  return pending_request_handler_;
}

void RequestPageData::SetPendingRequestHandler(
    RequestHandler* request_handler) {
  pending_request_handler_ = request_handler;
}

}  // namespace content::webid
