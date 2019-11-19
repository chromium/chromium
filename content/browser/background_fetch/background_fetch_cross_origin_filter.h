// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CROSS_ORIGIN_FILTER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CROSS_ORIGIN_FILTER_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BackgroundFetchRequestInfo;

// A filter that decides the CORS rules based on the source of a request and the
// response URL and headers included in the |request| info.
class CONTENT_EXPORT BackgroundFetchCrossOriginFilter {
 public:
  BackgroundFetchCrossOriginFilter(const url::Origin& source_origin,
                                   const BackgroundFetchRequestInfo& request);
  ~BackgroundFetchCrossOriginFilter();

  // Returns whether the Response object passed to the Service Worker event
  // should include the body included in the response.
  bool CanPopulateBody() const;

 private:
  // Whether the response comes from the same origin as the requester.
  bool is_same_origin_ = false;

  // Whether the Access-Control-Allow-Origin header includes the source origin.
  bool access_control_allow_origin_ = false;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchCrossOriginFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_CROSS_ORIGIN_FILTER_H_
