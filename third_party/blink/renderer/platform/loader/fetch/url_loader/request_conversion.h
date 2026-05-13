// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_REQUEST_CONVERSION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_REQUEST_CONVERSION_H_

// This file consists of request conversion functions between blink and network.

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace network {
class ResourceRequestBody;
struct ResourceRequest;
}  // namespace network

namespace blink {

class ResourceRequestHead;
class ResourceRequestBody;

scoped_refptr<network::ResourceRequestBody> NetworkResourceRequestBodyFor(
    const ResourceRequestBody src_body);

void BLINK_PLATFORM_EXPORT
PopulateResourceRequest(const ResourceRequestHead& src,
                        ResourceRequestBody src_body,
                        const String& context_default_user_agent,
                        network::ResourceRequest* dest);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_REQUEST_CONVERSION_H_
