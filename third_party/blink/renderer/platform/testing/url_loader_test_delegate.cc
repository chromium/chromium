// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/url_loader_test_delegate.h"

#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"

namespace blink {

URLLoaderTestDelegate::URLLoaderTestDelegate() = default;

URLLoaderTestDelegate::~URLLoaderTestDelegate() = default;

void URLLoaderTestDelegate::DidReceiveResponse(URLLoaderClient* original_client,
                                               const WebURLResponse& response) {
  original_client->DidReceiveResponse(
      response,
      /*body=*/mojo::ScopedDataPipeConsumerHandle(),
      /*cached_metadata=*/std::nullopt);
}

void URLLoaderTestDelegate::DidReceiveData(URLLoaderClient* original_client,
                                           base::span<const char> data) {
  original_client->DidReceiveDataForTesting(data);
}

void URLLoaderTestDelegate::DidFail(URLLoaderClient* original_client,
                                    const WebURLError& error,
                                    int64_t total_encoded_data_length,
                                    int64_t total_encoded_body_length,
                                    int64_t total_decoded_body_length) {
  original_client->DidFail(error, base::TimeTicks::Now(),
                           total_encoded_data_length, total_encoded_body_length,
                           total_decoded_body_length);
}

void URLLoaderTestDelegate::DidFinishLoading(
    URLLoaderClient* original_client,
    base::TimeTicks finish_time,
    int64_t total_encoded_data_length,
    int64_t total_encoded_body_length,
    int64_t total_decoded_body_length) {
  original_client->DidFinishLoading(finish_time, total_encoded_data_length,
                                    total_encoded_body_length,
                                    total_decoded_body_length);
}

}  // namespace blink
