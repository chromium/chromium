// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_URL_RESPONSE_INFO_UTIL_H_
#define CONTENT_RENDERER_PEPPER_URL_RESPONSE_INFO_UTIL_H_

#include "ppapi/shared_impl/url_response_info_data.h"

namespace blink {
class WebURLResponse;
}

namespace content {

ppapi::URLResponseInfoData DataFromWebURLResponse(
    const blink::WebURLResponse& response);

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_URL_RESPONSE_INFO_UTIL_H_
