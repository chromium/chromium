// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_

#include "content/public/browser/background_fetch_delegate.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class RenderFrameHostImpl;

namespace background_fetch {

// Records the BackgroundFetch UKM event. Must be called before a Background
// Fetch registration has been created. Will be a no-op if `rfh` is null or
// inactive.
void RecordBackgroundFetchUkmEvent(
    const blink::StorageKey& storage_key,
    int requests_size,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    RenderFrameHostImpl* rfh,
    BackgroundFetchPermission permission);

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_METRICS_H_
