// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_
#define CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_

#include <memory>

#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"
#include "url/gurl.h"

namespace content {

namespace webid {
class Request;
}

namespace webid {

class CONTENT_EXPORT RequestPageData : public PageUserData<RequestPageData> {
 public:
  ~RequestPageData() override;

  // The currently pending web identity request, if any.
  // Used to ensure that we do not allow two separate calls on the same page.
  Request* PendingWebIdentityRequest();
  // Sets the pending web identity request, or nullptr when a pending request
  // has finished.
  void SetPendingWebIdentityRequest(Request* request);

 private:
  explicit RequestPageData(Page& page);

  friend class PageUserData<RequestPageData>;
  PAGE_USER_DATA_KEY_DECL();

  // Non-null when there is some Web Identity API request currently pending.
  // Used to ensure that we do not allow two separate calls on the same page
  // and to access the currently pending request.
  raw_ptr<Request> pending_web_identity_request_ = nullptr;
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_PAGE_DATA_H_
