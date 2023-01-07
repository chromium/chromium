// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FETCH_FETCH_API_REQUEST_PROTO_H_
#define CONTENT_COMMON_FETCH_FETCH_API_REQUEST_PROTO_H_

#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

CONTENT_EXPORT std::string SerializeFetchRequestToString(
    const blink::mojom::FetchAPIRequest& request);

CONTENT_EXPORT blink::mojom::FetchAPIRequestPtr
DeserializeFetchRequestFromString(const std::string& serialized);

}  // namespace content

#endif  // CONTENT_COMMON_FETCH_FETCH_API_REQUEST_PROTO_H_
