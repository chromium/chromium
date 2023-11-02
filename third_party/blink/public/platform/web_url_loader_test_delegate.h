// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_TEST_DELEGATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_TEST_DELEGATE_H_

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

struct WebNavigationParams;
class WebURLResponse;
class WebURLLoaderClient;
struct WebURLError;

// Use with WebURLLoaderMockFactory::SetLoaderDelegate to intercept calls to a
// WebURLLoaderClient for controlling network responses in a test. Default
// implementations of all methods just call the original method on the
// WebURLLoaderClient.
class BLINK_PLATFORM_EXPORT WebURLLoaderTestDelegate {
 public:
  WebURLLoaderTestDelegate();
  virtual ~WebURLLoaderTestDelegate();

  virtual void DidReceiveResponse(WebURLLoaderClient* original_client,
                                  const WebURLResponse&);
  virtual void DidReceiveData(WebURLLoaderClient* original_client,
                              const char* data,
                              int data_length);
  virtual void DidFail(WebURLLoaderClient* original_client,
                       const WebURLError&,
                       int64_t total_encoded_data_length,
                       int64_t total_encoded_body_length,
                       int64_t total_decoded_body_length);
  virtual void DidFinishLoading(WebURLLoaderClient* original_client,
                                base::TimeTicks finish_time,
                                int64_t total_encoded_data_length,
                                int64_t total_encoded_body_length,
                                int64_t total_decoded_body_length);
  // Default implementation will load mocked url and fill in redirects,
  // response and body loader.
  // To override default behavior, fill in response (always), redirects
  // (if needed) and body loader (if not empty, see WebNavigationParams)
  // and return true.
  virtual bool FillNavigationParamsResponse(WebNavigationParams*) {
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_LOADER_TEST_DELEGATE_H_
