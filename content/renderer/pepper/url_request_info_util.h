// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_URL_REQUEST_INFO_UTIL_H_
#define CONTENT_RENDERER_PEPPER_URL_REQUEST_INFO_UTIL_H_

#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"

namespace ppapi {
struct URLRequestInfoData;
}

namespace blink {
class WebLocalFrame;
class WebURLRequest;
}

namespace content {

// Creates the WebKit URL request from the current request info. Returns true
// on success, false if the request is invalid (in which case *dest may be
// partially initialized). Any upload files with only resource IDs (no file ref
// pointers) will be populated by this function on success.
CONTENT_EXPORT bool CreateWebURLRequest(PP_Instance instance,
                                        ppapi::URLRequestInfoData* data,
                                        blink::WebLocalFrame* frame,
                                        blink::WebURLRequest* dest);

// Returns true if universal access is required to use the given request.
CONTENT_EXPORT bool URLRequestRequiresUniversalAccess(
    const ppapi::URLRequestInfoData& data);

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_URL_REQUEST_INFO_UTIL_H_
