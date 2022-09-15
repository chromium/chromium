// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
#define CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

// TODO(Richard): Remove this struct. Move CloneResponse() and CloneRequest() to
// related _utils files.
struct CONTENT_EXPORT BackgroundFetchSettledFetch {
  static blink::mojom::FetchAPIResponsePtr CloneResponse(
      const blink::mojom::FetchAPIResponsePtr& response);
  static blink::mojom::FetchAPIRequestPtr CloneRequest(
      const blink::mojom::FetchAPIRequestPtr& request);
};

}  // namespace content

#endif  // CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
