// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REQUEST_DESTINATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REQUEST_DESTINATION_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"

namespace blink {

// This function maps from Blink's internal "request context" concept to
// Fetch's notion of a request's "destination":
// https://fetch.spec.whatwg.org/#concept-request-destination.
BLINK_COMMON_EXPORT const char* GetRequestDestinationFromContext(
    mojom::RequestContextType context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REQUEST_DESTINATION_H_
