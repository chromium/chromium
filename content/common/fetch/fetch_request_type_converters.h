// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FETCH_FETCH_REQUEST_TYPE_CONVERTERS_H_
#define CONTENT_COMMON_FETCH_FETCH_REQUEST_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

// This is used only by Service Workers for now.

namespace content {

CONTENT_EXPORT blink::mojom::FetchCacheMode
GetFetchCacheModeFromLoadFlagsForTest(int load_flags);

}  // namespace content

namespace mojo {

template <>
struct TypeConverter<blink::mojom::FetchAPIRequestPtr,
                     network::ResourceRequest> {
  static blink::mojom::FetchAPIRequestPtr Convert(
      const network::ResourceRequest& input);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_FETCH_FETCH_REQUEST_TYPE_CONVERTERS_H_
