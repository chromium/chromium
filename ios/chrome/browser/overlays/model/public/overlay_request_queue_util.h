// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_UTIL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_UTIL_H_

#include <stddef.h>

#include "base/functional/callback.h"

class OverlayRequest;
class OverlayRequestQueue;

// Searches through `queue` for requests for which `matcher` returns true.  If
// a matching request is found, returns true and populates `index` with the
// index of the first matching request.  Returns false if no matching request is
// found.  All arguments must be non-null.
bool GetIndexOfMatchingRequest(
    OverlayRequestQueue* queue,
    size_t* index,
    base::RepeatingCallback<bool(OverlayRequest*)> matcher);

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_UTIL_H_
