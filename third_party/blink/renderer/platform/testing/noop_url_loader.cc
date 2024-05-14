// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/noop_url_loader.h"

#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void NoopURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient*,
    WebURLResponse&,
    std::optional<WebURLError>&,
    scoped_refptr<SharedBuffer>&,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  NOTREACHED_IN_MIGRATION();
}

void NoopURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    URLLoaderClient*) {}

}  // namespace blink
