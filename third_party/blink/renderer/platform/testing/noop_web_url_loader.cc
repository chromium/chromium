// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/noop_web_url_loader.h"

#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"

namespace blink {

void NoopWebURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    WebURLLoaderClient*,
    WebURLResponse&,
    absl::optional<WebURLError>&,
    WebData&,
    int64_t& encoded_data_length,
    int64_t& encoded_body_length,
    WebBlobInfo& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  NOTREACHED();
}

void NoopWebURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    WebURLLoaderClient*) {}

}  // namespace blink
